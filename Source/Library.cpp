#include "Library.h"
#include "TrackAnalyser.h"
#include <sqlite3.h>
#include <deque>

namespace
{
    juce::int64 now()   { return juce::Time::currentTimeMillis() / 1000; }

    juce::String hotCuesToText (const std::array<double, 4>& h)
    {
        juce::StringArray parts;
        for (auto v : h)
            parts.add (juce::String (v, 4));
        return parts.joinIntoString (";");
    }

    std::array<double, 4> hotCuesFromText (const juce::String& s)
    {
        std::array<double, 4> h { -1.0, -1.0, -1.0, -1.0 };
        auto parts = juce::StringArray::fromTokens (s, ";", "");
        for (int i = 0; i < 4 && i < parts.size(); ++i)
            h[(size_t) i] = parts[i].getDoubleValue();
        return h;
    }

    // Escapa % y _ para usar el texto dentro de LIKE ... ESCAPE '\'
    juce::String escapeLike (const juce::String& s)
    {
        return s.replace ("\\", "\\\\").replace ("%", "\\%").replace ("_", "\\_");
    }

    const char* const trackColumns =
        "t.id, t.path, t.title, t.artist, t.album, t.genre, t.year, t.comment, t.musical_key, "
        "t.duration, t.bpm, t.first_beat, t.analysed, t.rating, t.play_count, t.last_played, "
        "t.date_added, t.cue_point, t.hot_cues";

    // Columnas por las que se puede ordenar (lista blanca: nunca se mete texto libre en el SQL)
    juce::String orderClause (const juce::String& column, bool ascending)
    {
        static const std::map<juce::String, juce::String> columns = {
            { "title", "t.title COLLATE NOCASE" }, { "artist", "t.artist COLLATE NOCASE" },
            { "album", "t.album COLLATE NOCASE" }, { "genre", "t.genre COLLATE NOCASE" },
            { "year", "t.year" }, { "key", "t.musical_key" }, { "bpm", "t.bpm" },
            { "duration", "t.duration" }, { "rating", "t.rating" }, { "plays", "t.play_count" },
            { "added", "t.date_added" }, { "lastPlayed", "t.last_played" },
        };

        const auto it = columns.find (column);
        if (it == columns.end())
            return {};
        return " ORDER BY " + it->second + (ascending ? " ASC" : " DESC") + ", t.title COLLATE NOCASE";
    }
}

//==============================================================================
struct Library::Statement
{
    Statement (sqlite3* database, const juce::String& sql)
    {
        sqlite3_prepare_v2 (database, sql.toRawUTF8(), -1, &stmt, nullptr);
    }

    ~Statement()                                        { sqlite3_finalize (stmt); }

    Statement& bind (int i, const juce::String& s)      { sqlite3_bind_text (stmt, i, s.toRawUTF8(), -1, SQLITE_TRANSIENT); return *this; }
    Statement& bind (int i, double v)                   { sqlite3_bind_double (stmt, i, v); return *this; }
    Statement& bind (int i, juce::int64 v)              { sqlite3_bind_int64 (stmt, i, v); return *this; }
    Statement& bind (int i, int v)                      { return bind (i, (juce::int64) v); }

    Statement& bindVar (int i, const juce::var& v)
    {
        if (v.isString())   return bind (i, v.toString());
        if (v.isDouble())   return bind (i, (double) v);
        return bind (i, (juce::int64) v);
    }

    bool next()                                         { return stmt != nullptr && sqlite3_step (stmt) == SQLITE_ROW; }
    void run()                                          { if (stmt != nullptr) sqlite3_step (stmt); }
    void reset()                                        { sqlite3_reset (stmt); sqlite3_clear_bindings (stmt); }

    juce::String text (int c) const
    {
        const auto* t = sqlite3_column_text (stmt, c);
        return t != nullptr ? juce::String::fromUTF8 ((const char*) t) : juce::String();
    }
    double real (int c) const                           { return sqlite3_column_double (stmt, c); }
    juce::int64 integer (int c) const                   { return sqlite3_column_int64 (stmt, c); }

    LibraryTrack track() const
    {
        LibraryTrack t;
        t.id = integer (0);
        t.file = juce::File (text (1));
        t.title = text (2);
        t.artist = text (3);
        t.album = text (4);
        t.genre = text (5);
        t.year = text (6);
        t.comment = text (7);
        t.key = text (8);
        t.duration = real (9);
        t.bpm = real (10);
        t.firstBeat = real (11);
        t.analysed = integer (12) == 1;
        t.rating = (int) integer (13);
        t.playCount = (int) integer (14);
        t.lastPlayed = integer (15);
        t.dateAdded = integer (16);
        t.cuePoint = real (17);
        t.hotCues = hotCuesFromText (text (18));
        return t;
    }

    sqlite3_stmt* stmt = nullptr;
};

//==============================================================================
// Hilo de fondo con una cola de trabajos (escanear, analizar...)
class Library::Worker : public juce::Thread
{
public:
    explicit Worker (Library& l) : juce::Thread ("Library worker"), library (l)
    {
        startThread (juce::Thread::Priority::low);
    }

    ~Worker() override
    {
        stopThread (15000);
    }

    void add (std::function<void()> job)
    {
        {
            const juce::ScopedLock sl (jobLock);
            jobs.push_back (std::move (job));
        }
        library.busy = true;
        notify();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            std::function<void()> job;
            {
                const juce::ScopedLock sl (jobLock);
                if (! jobs.empty())
                {
                    job = std::move (jobs.front());
                    jobs.pop_front();
                }
            }

            if (job != nullptr)
            {
                library.busy = true;
                job();

                const juce::ScopedLock sl (jobLock);
                if (jobs.empty())
                {
                    library.busy = false;
                    library.notify();
                }
            }
            else
            {
                wait (500);
            }
        }
    }

private:
    Library& library;
    juce::CriticalSection jobLock;
    std::deque<std::function<void()>> jobs;
};

//==============================================================================
Library::Library (const juce::File& databaseFile, const juce::String& audioWildcard)
    : wildcard (audioWildcard)
{
    formats.registerBasicFormats();
    databaseFile.getParentDirectory().createDirectory();

    if (sqlite3_open_v2 (databaseFile.getFullPathName().toRawUTF8(), &db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        sqlite3_close (db);
        db = nullptr;
        return;
    }

    exec ("PRAGMA journal_mode=WAL");
    exec ("PRAGMA foreign_keys=ON");
    createSchema();

    worker = std::make_unique<Worker> (*this);

    // Al arrancar, buscar canciones nuevas en las carpetas vigiladas (las que no cambian no se vuelven a leer)
    rescanAll();
}

Library::~Library()
{
    cancelAnalysisFlag = true;
    worker = nullptr;

    if (db != nullptr)
        sqlite3_close (db);
}

void Library::exec (const char* sql)
{
    const juce::ScopedLock sl (lock);
    sqlite3_exec (db, sql, nullptr, nullptr, nullptr);
}

void Library::createSchema()
{
    exec (R"sql(
        CREATE TABLE IF NOT EXISTS tracks (
            id          INTEGER PRIMARY KEY,
            path        TEXT NOT NULL UNIQUE COLLATE NOCASE,
            title       TEXT DEFAULT '',
            artist      TEXT DEFAULT '',
            album       TEXT DEFAULT '',
            genre       TEXT DEFAULT '',
            year        TEXT DEFAULT '',
            comment     TEXT DEFAULT '',
            musical_key TEXT DEFAULT '',
            duration    REAL DEFAULT 0,
            bpm         REAL DEFAULT 0,
            first_beat  REAL DEFAULT 0,
            analysed    INTEGER DEFAULT 0,     -- 0 pendiente, 1 analizada, 2 error
            rating      INTEGER DEFAULT 0,
            play_count  INTEGER DEFAULT 0,
            last_played INTEGER DEFAULT 0,
            date_added  INTEGER DEFAULT 0,
            file_size   INTEGER DEFAULT 0,
            file_mtime  INTEGER DEFAULT 0,
            cue_point   REAL DEFAULT 0,
            hot_cues    TEXT DEFAULT ''
        );
        CREATE TABLE IF NOT EXISTS folders (path TEXT PRIMARY KEY COLLATE NOCASE);
        CREATE TABLE IF NOT EXISTS playlists (
            id      INTEGER PRIMARY KEY,
            name    TEXT NOT NULL,
            created INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS playlist_tracks (
            id          INTEGER PRIMARY KEY,
            playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
            track_id    INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
            position    INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS history (
            id        INTEGER PRIMARY KEY,
            track_id  INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
            played_at INTEGER NOT NULL,
            deck      INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_playlist ON playlist_tracks (playlist_id, position);
        CREATE INDEX IF NOT EXISTS idx_history ON history (played_at);
    )sql");
}

//==============================================================================
// Consultas
//==============================================================================
std::vector<LibraryTrack> Library::query (const LibraryView& view, const juce::String& search,
                                          const juce::String& sortColumn, bool ascending,
                                          double bpmMin, double bpmMax)
{
    std::vector<LibraryTrack> result;
    if (db == nullptr)
        return result;

    juce::String sql, defaultOrder;
    juce::StringArray where;
    juce::Array<juce::var> params;

    switch (view.type)
    {
        case LibraryView::Type::history:
            sql = juce::String ("SELECT ") + trackColumns + ", h.id, h.played_at FROM history h JOIN tracks t ON t.id = h.track_id";
            defaultOrder = " ORDER BY h.played_at DESC";
            break;

        case LibraryView::Type::playlist:
            sql = juce::String ("SELECT ") + trackColumns + ", pt.id, 0 FROM playlist_tracks pt JOIN tracks t ON t.id = pt.track_id";
            where.add ("pt.playlist_id = ?");
            params.add (view.playlistId);
            defaultOrder = " ORDER BY pt.position";
            break;

        case LibraryView::Type::folder:
            sql = juce::String ("SELECT ") + trackColumns + ", 0, 0 FROM tracks t";
            where.add ("t.path LIKE ? ESCAPE '\\'");
            params.add (escapeLike (view.folder.getFullPathName() + juce::File::getSeparatorString()) + "%");
            defaultOrder = " ORDER BY t.path COLLATE NOCASE";
            break;

        case LibraryView::Type::recentlyAdded:
            sql = juce::String ("SELECT ") + trackColumns + ", 0, 0 FROM tracks t";
            where.add ("t.date_added > ?");
            params.add (now() - 30 * 24 * 3600);
            defaultOrder = " ORDER BY t.date_added DESC";
            break;

        case LibraryView::Type::all:
            sql = juce::String ("SELECT ") + trackColumns + ", 0, 0 FROM tracks t";
            defaultOrder = " ORDER BY t.artist COLLATE NOCASE, t.title COLLATE NOCASE";
            break;
    }

    for (auto& word : juce::StringArray::fromTokens (search, true))
    {
        if (word.trim().isEmpty())
            continue;
        where.add ("(t.title LIKE ? ESCAPE '\\' OR t.artist LIKE ? ESCAPE '\\' OR t.album LIKE ? ESCAPE '\\' OR t.genre LIKE ? ESCAPE '\\')");
        const auto pattern = "%" + escapeLike (word.unquoted()) + "%";
        for (int i = 0; i < 4; ++i)
            params.add (pattern);
    }

    if (bpmMin > 0.0 && bpmMax > bpmMin)
    {
        where.add ("t.bpm BETWEEN ? AND ?");
        params.add (bpmMin);
        params.add (bpmMax);
    }

    if (! where.isEmpty())
        sql += " WHERE " + where.joinIntoString (" AND ");

    const auto order = orderClause (sortColumn, ascending);
    sql += order.isNotEmpty() ? order : defaultOrder;

    const juce::ScopedLock sl (lock);
    Statement st (db, sql);
    for (int i = 0; i < params.size(); ++i)
        st.bindVar (i + 1, params[i]);

    while (st.next())
    {
        auto t = st.track();
        t.entryId = st.integer (19);
        t.playedAt = st.integer (20);
        result.push_back (std::move (t));
    }

    return result;
}

std::optional<LibraryTrack> Library::findByFile (const juce::File& file)
{
    if (db == nullptr)
        return std::nullopt;

    const juce::ScopedLock sl (lock);
    Statement st (db, juce::String ("SELECT ") + trackColumns + " FROM tracks t WHERE t.path = ?");
    st.bind (1, file.getFullPathName());
    if (st.next())
        return st.track();
    return std::nullopt;
}

int Library::countTracks()
{
    const juce::ScopedLock sl (lock);
    Statement st (db, "SELECT COUNT(*) FROM tracks");
    return st.next() ? (int) st.integer (0) : 0;
}

int Library::countPending()
{
    const juce::ScopedLock sl (lock);
    Statement st (db, "SELECT COUNT(*) FROM tracks WHERE analysed = 0");
    return st.next() ? (int) st.integer (0) : 0;
}

//==============================================================================
// Carpetas y escaneo
//==============================================================================
juce::Array<juce::File> Library::getFolders()
{
    juce::Array<juce::File> folders;
    const juce::ScopedLock sl (lock);
    Statement st (db, "SELECT path FROM folders ORDER BY path COLLATE NOCASE");
    while (st.next())
        folders.add (juce::File (st.text (0)));
    return folders;
}

void Library::addFolder (const juce::File& folder)
{
    if (db == nullptr || ! folder.isDirectory())
        return;

    {
        const juce::ScopedLock sl (lock);
        Statement st (db, "INSERT OR IGNORE INTO folders (path) VALUES (?)");
        st.bind (1, folder.getFullPathName()).run();
    }

    notify();
    worker->add ([this, folder] { scanFolder (folder); });
}

void Library::removeFolder (const juce::File& folder)
{
    // Solo se deja de vigilar la carpeta: las canciones, listas e historial se conservan
    const juce::ScopedLock sl (lock);
    Statement st (db, "DELETE FROM folders WHERE path = ?");
    st.bind (1, folder.getFullPathName()).run();
    notify();
}

void Library::rescanAll()
{
    for (auto& f : getFolders())
        worker->add ([this, f] { scanFolder (f); });
}

void Library::addFileAsync (const juce::File& file)
{
    if (db == nullptr || ! file.existsAsFile())
        return;

    worker->add ([this, file]
    {
        double duration = 0.0;
        if (std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (file)); r != nullptr && r->sampleRate > 0)
            duration = (double) r->lengthInSamples / r->sampleRate;

        insertOrUpdate (file, TagReader::read (file), duration);
        notify();
    });
}

void Library::insertOrUpdate (const juce::File& file, const TagInfo& tags, double duration)
{
    const juce::ScopedLock sl (lock);
    Statement st (db, R"sql(
        INSERT INTO tracks (path, title, artist, album, genre, year, comment, musical_key, duration, bpm,
                            date_added, file_size, file_mtime)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT (path) DO UPDATE SET
            title = excluded.title, artist = excluded.artist, album = excluded.album, genre = excluded.genre,
            year = excluded.year, comment = excluded.comment, musical_key = excluded.musical_key,
            duration = CASE WHEN excluded.duration > 0 THEN excluded.duration ELSE duration END,
            bpm = CASE WHEN analysed = 1 THEN bpm ELSE excluded.bpm END,
            file_size = excluded.file_size, file_mtime = excluded.file_mtime
    )sql");

    st.bind (1, file.getFullPathName()).bind (2, tags.title).bind (3, tags.artist).bind (4, tags.album)
      .bind (5, tags.genre).bind (6, tags.year).bind (7, tags.comment).bind (8, tags.key)
      .bind (9, duration).bind (10, tags.bpm).bind (11, now())
      .bind (12, file.getSize()).bind (13, file.getLastModificationTime().toMilliseconds() / 1000);
    st.run();
}

void Library::scanFolder (const juce::File& folder)
{
    {
        const juce::ScopedLock sl (lock);
        progressLabel = "Escaneando " + folder.getFileName();
    }
    progressDone = 0;
    progressTotal = 0;

    // 1) Lista de archivos de audio (recursivo)
    juce::Array<juce::File> files;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, wildcard, juce::File::findFiles))
    {
        if (worker->threadShouldExit())
            return;
        files.add (entry.getFile());
        progressTotal = files.size();
    }

    // 2) Etiquetas y duración solo de los archivos nuevos o modificados
    auto lastNotify = juce::Time::getMillisecondCounter();

    for (int i = 0; i < files.size(); ++i)
    {
        if (worker->threadShouldExit())
            return;

        const auto& f = files.getReference (i);
        progressDone = i + 1;

        bool unchanged = false;
        {
            const juce::ScopedLock sl (lock);
            Statement st (db, "SELECT file_size, file_mtime FROM tracks WHERE path = ?");
            st.bind (1, f.getFullPathName());
            unchanged = st.next() && st.integer (0) == f.getSize()
                        && st.integer (1) == f.getLastModificationTime().toMilliseconds() / 1000;
        }

        if (! unchanged)
        {
            double duration = 0.0;
            if (std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f)); r != nullptr && r->sampleRate > 0)
                duration = (double) r->lengthInSamples / r->sampleRate;

            insertOrUpdate (f, TagReader::read (f), duration);
        }

        if (juce::Time::getMillisecondCounter() - lastNotify > 1000)
        {
            lastNotify = juce::Time::getMillisecondCounter();
            notify();
        }
    }

    notify();
}

//==============================================================================
// Datos de canciones
//==============================================================================
void Library::setAnalysis (const juce::File& file, double bpm, double firstBeat, double duration)
{
    if (db == nullptr)
        return;

    if (! findByFile (file).has_value())
        insertOrUpdate (file, TagReader::read (file), duration);

    {
        const juce::ScopedLock sl (lock);
        Statement st (db, "UPDATE tracks SET bpm = ?, first_beat = ?, analysed = ?, "
                          "duration = CASE WHEN ? > 0 THEN ? ELSE duration END WHERE path = ?");
        st.bind (1, bpm).bind (2, firstBeat).bind (3, bpm > 0.0 ? 1 : 2)
          .bind (4, duration).bind (5, duration).bind (6, file.getFullPathName());
        st.run();
    }
    notify();
}

void Library::setCues (const juce::File& file, double cuePoint, const std::array<double, 4>& hotCues)
{
    if (db == nullptr)
        return;

    if (! findByFile (file).has_value())
        insertOrUpdate (file, TagReader::read (file), 0.0);

    const juce::ScopedLock sl (lock);
    Statement st (db, "UPDATE tracks SET cue_point = ?, hot_cues = ? WHERE path = ?");
    st.bind (1, cuePoint).bind (2, hotCuesToText (hotCues)).bind (3, file.getFullPathName());
    st.run();
}

void Library::setRating (juce::int64 trackId, int rating)
{
    {
        const juce::ScopedLock sl (lock);
        Statement st (db, "UPDATE tracks SET rating = ? WHERE id = ?");
        st.bind (1, juce::jlimit (0, 5, rating)).bind (2, trackId).run();
    }
    notify();
}

void Library::markPlayed (const juce::File& file, int deck)
{
    if (db == nullptr)
        return;

    if (! findByFile (file).has_value())
        insertOrUpdate (file, TagReader::read (file), 0.0);

    {
        const juce::ScopedLock sl (lock);
        const auto t = now();

        Statement up (db, "UPDATE tracks SET play_count = play_count + 1, last_played = ? WHERE path = ?");
        up.bind (1, t).bind (2, file.getFullPathName()).run();

        Statement hist (db, "INSERT INTO history (track_id, played_at, deck) SELECT id, ?, ? FROM tracks WHERE path = ?");
        hist.bind (1, t).bind (2, deck).bind (3, file.getFullPathName()).run();
    }
    notify();
}

void Library::removeTracks (const juce::Array<juce::int64>& trackIds)
{
    {
        const juce::ScopedLock sl (lock);
        exec ("BEGIN");
        Statement st (db, "DELETE FROM tracks WHERE id = ?");
        for (auto id : trackIds)
        {
            st.bind (1, id).run();
            st.reset();
        }
        exec ("COMMIT");
    }
    notify();
}

void Library::requeueAnalysis (const juce::Array<juce::int64>& trackIds)
{
    const juce::ScopedLock sl (lock);
    exec ("BEGIN");
    Statement st (db, "UPDATE tracks SET analysed = 0 WHERE id = ?");
    for (auto id : trackIds)
    {
        st.bind (1, id).run();
        st.reset();
    }
    exec ("COMMIT");
}

//==============================================================================
// Listas
//==============================================================================
std::vector<LibraryPlaylist> Library::getPlaylists()
{
    std::vector<LibraryPlaylist> lists;
    if (db == nullptr)
        return lists;

    const juce::ScopedLock sl (lock);
    Statement st (db, "SELECT p.id, p.name, (SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id = p.id) "
                      "FROM playlists p ORDER BY p.name COLLATE NOCASE");
    while (st.next())
        lists.push_back ({ st.integer (0), st.text (1), (int) st.integer (2) });
    return lists;
}

juce::int64 Library::createPlaylist (const juce::String& name)
{
    juce::int64 id = 0;
    {
        const juce::ScopedLock sl (lock);
        Statement st (db, "INSERT INTO playlists (name, created) VALUES (?, ?)");
        st.bind (1, name).bind (2, now()).run();
        id = sqlite3_last_insert_rowid (db);
    }
    notify();
    return id;
}

void Library::renamePlaylist (juce::int64 id, const juce::String& name)
{
    {
        const juce::ScopedLock sl (lock);
        Statement st (db, "UPDATE playlists SET name = ? WHERE id = ?");
        st.bind (1, name).bind (2, id).run();
    }
    notify();
}

void Library::deletePlaylist (juce::int64 id)
{
    {
        const juce::ScopedLock sl (lock);
        Statement st (db, "DELETE FROM playlists WHERE id = ?");
        st.bind (1, id).run();
    }
    notify();
}

void Library::addToPlaylist (juce::int64 playlistId, const juce::Array<juce::int64>& trackIds)
{
    {
        const juce::ScopedLock sl (lock);
        exec ("BEGIN");

        juce::int64 position = 0;
        {
            Statement maxPos (db, "SELECT COALESCE(MAX(position), 0) FROM playlist_tracks WHERE playlist_id = ?");
            maxPos.bind (1, playlistId);
            if (maxPos.next())
                position = maxPos.integer (0);
        }

        Statement st (db, "INSERT INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?)");
        for (auto id : trackIds)
        {
            st.bind (1, playlistId).bind (2, id).bind (3, ++position).run();
            st.reset();
        }
        exec ("COMMIT");
    }
    notify();
}

void Library::removeFromPlaylist (const juce::Array<juce::int64>& entryIds)
{
    {
        const juce::ScopedLock sl (lock);
        exec ("BEGIN");
        Statement st (db, "DELETE FROM playlist_tracks WHERE id = ?");
        for (auto id : entryIds)
        {
            st.bind (1, id).run();
            st.reset();
        }
        exec ("COMMIT");
    }
    notify();
}

void Library::clearHistory()
{
    exec ("DELETE FROM history");
    notify();
}

//==============================================================================
// Análisis de BPM en lote
//==============================================================================
void Library::analysePending()
{
    if (db == nullptr)
        return;

    cancelAnalysisFlag = false;
    worker->add ([this] { analyseLoop(); });
}

void Library::analyseLoop()
{
    {
        const juce::ScopedLock sl (lock);
        progressLabel = "Analizando BPM";
    }
    progressDone = 0;
    progressTotal = countPending();

    auto stop = [this] { return cancelAnalysisFlag.load() || worker->threadShouldExit(); };

    while (! stop())
    {
        juce::File file;
        {
            const juce::ScopedLock sl (lock);
            Statement st (db, "SELECT path FROM tracks WHERE analysed = 0 LIMIT 1");
            if (! st.next())
                break;
            file = juce::File (st.text (0));
        }

        double bpm = 0.0, firstBeat = 0.0, duration = 0.0;
        if (auto track = TrackAnalyser::loadTrack (formats, file, stop))
        {
            TrackAnalyser::detectBeatgrid (*track, stop);
            bpm = track->bpm;
            firstBeat = track->firstBeat;
            duration = track->getLength();
        }

        if (stop())
            break;

        setAnalysis (file, bpm, firstBeat, duration);   // bpm 0 -> marcada como error (no se reintenta)
        ++progressDone;
    }
}

juce::String Library::getStatusText() const
{
    if (! busy.load())
        return {};

    juce::String label;
    {
        const juce::ScopedLock sl (lock);
        label = progressLabel;
    }

    return progressTotal.load() > 0 ? label + "  " + juce::String (progressDone.load()) + " / " + juce::String (progressTotal.load())
                                    : label + "...";
}
