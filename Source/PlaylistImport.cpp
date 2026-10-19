#include "PlaylistImport.h"
#include "TrackMatcher.h"
#include "DJLookAndFeel.h"
#include <juce_cryptography/juce_cryptography.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "SpotifyConfig.h"
#include "Branding.h"

using namespace DJColours;

namespace
{
    juce::String fromBytes (const juce::File& f)
    {
        juce::MemoryBlock mb;
        f.loadFileAsData (mb);
        auto s = juce::String::fromUTF8 ((const char*) mb.getData(), (int) mb.getSize());
        if (s.startsWithChar ((juce::juce_wchar) 0xfeff))
            s = s.substring (1);
        return s;
    }

    // Duración en segundos: "3:45", "225" (s) o "225000" (ms)
    double parseDuration (const juce::String& s)
    {
        const auto t = s.trim();
        if (t.containsChar (':'))
        {
            auto parts = juce::StringArray::fromTokens (t, ":", "");
            double total = 0.0;
            for (auto& p : parts)
                total = total * 60.0 + p.getDoubleValue();
            return total;
        }
        const double v = t.getDoubleValue();
        return v > 10000.0 ? v / 1000.0 : v;
    }

    // Separa una línea CSV respetando comillas ("a, b", "dice ""hola""")
    juce::StringArray splitCsvLine (const juce::String& line, juce::juce_wchar sep)
    {
        juce::StringArray out;
        juce::String cur;
        bool quoted = false;
        for (int i = 0; i < line.length(); ++i)
        {
            const auto c = line[i];
            if (quoted)
            {
                if (c == '"' && line[i + 1] == '"') { cur += '"'; ++i; }
                else if (c == '"')                  quoted = false;
                else                                cur += c;
            }
            else if (c == '"')  quoted = true;
            else if (c == sep)  { out.add (cur.trim()); cur.clear(); }
            else                cur += c;
        }
        out.add (cur.trim());
        return out;
    }

    // Une líneas CSV partidas por saltos de línea dentro de comillas
    juce::StringArray csvRecords (const juce::String& content)
    {
        juce::StringArray records;
        juce::String cur;
        bool quoted = false;
        for (auto line : juce::StringArray::fromLines (content))
        {
            cur = cur.isEmpty() ? line : cur + "\n" + line;
            for (auto c : line)
                if (c == '"')
                    quoted = ! quoted;
            if (! quoted)
            {
                if (cur.trim().isNotEmpty())
                    records.add (cur);
                cur.clear();
            }
        }
        return records;
    }

    int findColumn (const juce::StringArray& header, std::initializer_list<const char*> names)
    {
        for (auto* n : names)
            for (int i = 0; i < header.size(); ++i)
                if (header[i].toLowerCase().trim() == n)
                    return i;
        for (auto* n : names)
            for (int i = 0; i < header.size(); ++i)
                if (header[i].toLowerCase().contains (n))
                    return i;
        return -1;
    }

    juce::String base64Url (const void* data, size_t size)
    {
        return juce::Base64::toBase64 (data, size).replace ("+", "-").replace ("/", "_").removeCharacters ("=");
    }

    juce::String randomToken (int length)
    {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
        juce::Random r;
        r.setSeedRandomly();
        juce::String s;
        for (int i = 0; i < length; ++i)
            s += chars[r.nextInt (66)];
        return s;
    }

    juce::String urlEncode (const juce::String& s)      { return juce::URL::addEscapeChars (s, true); }
}

//==============================================================================
// Archivos de listas
//==============================================================================
std::optional<ImportedPlaylist> PlaylistFiles::readCsv (const juce::File& file)
{
    const auto records = csvRecords (fromBytes (file));
    if (records.size() < 2)
        return std::nullopt;

    const auto& first = records[0];
    const juce::juce_wchar sep = first.retainCharacters (";").length() > first.retainCharacters (",").length() ? ';' : ',';
    const auto header = splitCsvLine (first, sep);

    const int titleCol = findColumn (header, { "track name", "title", "song", "name", "nombre", juce::CharPointer_UTF8 ("canci\xc3\xb3n").getAddress(), "track" });
    const int artistCol = findColumn (header, { "artist name(s)", "artist name", "artist", "artista", "artists" });
    const int albumCol = findColumn (header, { "album name", "album", juce::CharPointer_UTF8 ("\xc3\xa1lbum").getAddress() });
    const int durCol = findColumn (header, { "duration (ms)", "duration", "length", juce::CharPointer_UTF8 ("duraci\xc3\xb3n").getAddress(), "time" });
    const int listCol = findColumn (header, { "playlist name", "playlist" });
    if (titleCol < 0)
        return std::nullopt;

    ImportedPlaylist p;
    p.name = file.getFileNameWithoutExtension();
    p.source = "CSV";

    for (int r = 1; r < records.size(); ++r)
    {
        const auto row = splitCsvLine (records[r], sep);
        ImportedTrack t;
        t.title = row[titleCol];
        t.artist = artistCol >= 0 ? row[artistCol].replace (";", ", ") : juce::String();
        t.album = albumCol >= 0 ? row[albumCol] : juce::String();
        t.duration = durCol >= 0 ? parseDuration (row[durCol]) : 0.0;
        if (listCol >= 0 && r == 1 && row[listCol].isNotEmpty())
            p.name = row[listCol];
        if (t.title.isNotEmpty())
            p.tracks.push_back (t);
    }
    return p;
}

std::optional<ImportedPlaylist> PlaylistFiles::readM3u (const juce::File& file)
{
    ImportedPlaylist p;
    p.name = file.getFileNameWithoutExtension();
    p.source = "M3U";

    ImportedTrack pending;
    for (auto line : juce::StringArray::fromLines (fromBytes (file)))
    {
        line = line.trim();
        if (line.isEmpty())
            continue;

        if (line.startsWithIgnoreCase ("#PLAYLIST:"))
        {
            p.name = line.fromFirstOccurrenceOf (":", false, false).trim();
        }
        else if (line.startsWithIgnoreCase ("#EXTINF:"))
        {
            // #EXTINF:225,Artista - Título
            const auto info = line.fromFirstOccurrenceOf (":", false, false);
            pending.duration = info.upToFirstOccurrenceOf (",", false, false).getDoubleValue();
            const auto name = info.fromFirstOccurrenceOf (",", false, false).trim();
            if (name.contains (" - "))
            {
                pending.artist = name.upToFirstOccurrenceOf (" - ", false, false).trim();
                pending.title = name.fromFirstOccurrenceOf (" - ", false, false).trim();
            }
            else
            {
                pending.title = name;
            }
        }
        else if (! line.startsWithChar ('#'))
        {
            juce::File f;
            if (line.startsWithIgnoreCase ("file:"))
                f = juce::URL (line).getLocalFile();
            else if (juce::File::isAbsolutePath (line))
                f = juce::File (line);
            else
                f = file.getParentDirectory().getChildFile (line.replaceCharacter ('/', '\\'));

            pending.file = f.existsAsFile() ? f : juce::File();
            if (pending.title.isEmpty())
            {
                const auto base = juce::File::createFileWithoutCheckingPath (line.replaceCharacter ('/', '\\')).getFileNameWithoutExtension();
                pending.artist = base.contains (" - ") ? base.upToFirstOccurrenceOf (" - ", false, false).trim() : juce::String();
                pending.title = base.contains (" - ") ? base.fromFirstOccurrenceOf (" - ", false, false).trim() : base;
            }
            p.tracks.push_back (pending);
            pending = {};
        }
    }

    if (p.tracks.empty())
        return std::nullopt;
    return p;
}

bool PlaylistFiles::writeM3u (const juce::File& file, const std::vector<LibraryTrack>& tracks)
{
    juce::String out = "#EXTM3U\n";
    for (auto& t : tracks)
    {
        out << "#EXTINF:" << juce::roundToInt (t.duration) << ","
            << (t.artist.isNotEmpty() ? t.artist + " - " : juce::String()) << t.title << "\n"
            << t.file.getFullPathName() << "\n";
    }
    return file.replaceWithText (out, false, false, "\r\n");
}

//==============================================================================
// Spotify
//==============================================================================
SpotifyClient::SpotifyClient (juce::PropertiesFile& s) : settings (s) {}

juce::String SpotifyClient::getClientId() const
{
    const juce::String builtIn (CESARDJ_SPOTIFY_CLIENT_ID);
    return builtIn.isNotEmpty() ? builtIn : settings.getValue ("spotifyClientId");
}

juce::String SpotifyClient::getUserName() const         { return settings.getValue ("spotifyUser"); }

juce::String SpotifyClient::makeAuthUrl (const juce::String& challenge, const juce::String& state, bool forceDialog) const
{
    return juce::String ("https://accounts.spotify.com/authorize?response_type=code")
        + "&client_id=" + urlEncode (getClientId())
        + "&redirect_uri=" + urlEncode (redirectUri)
        + "&code_challenge_method=S256&code_challenge=" + challenge
        + "&state=" + state
        + (forceDialog ? "&show_dialog=true" : "")
        + "&scope=" + urlEncode ("playlist-read-private playlist-read-collaborative user-library-read");
}

bool SpotifyClient::exchangeCode (const juce::String& code, const juce::String& verifier, juce::String& error)
{
    const auto json = postForm ("https://accounts.spotify.com/api/token",
                                "grant_type=authorization_code&code=" + urlEncode (code)
                                + "&redirect_uri=" + urlEncode (redirectUri)
                                + "&client_id=" + urlEncode (getClientId())
                                + "&code_verifier=" + verifier, error);
    return error.isEmpty() && storeTokens (json, error);
}

bool SpotifyClient::fetchUserName (juce::String& error)
{
    if (! ensureToken (error))
        return false;
    int status = 0;
    const auto me = request ("https://api.spotify.com/v1/me", error, status);
    if (error.isNotEmpty())
        return false;
    auto name = me["display_name"].toString();
    if (name.isEmpty())
        name = me["id"].toString();
    settings.setValue ("spotifyUser", name);
    settings.saveIfNeeded();
    return true;
}
void SpotifyClient::setClientId (const juce::String& id) { settings.setValue ("spotifyClientId", id.trim()); settings.saveIfNeeded(); }
bool SpotifyClient::hasSession() const                  { return settings.getValue ("spotifyRefreshToken").isNotEmpty(); }

void SpotifyClient::logout()
{
    const juce::ScopedLock sl (lock);
    accessToken.clear();
    settings.removeValue ("spotifyRefreshToken");
    settings.removeValue ("spotifyUser");
    settings.saveIfNeeded();
}

bool SpotifyClient::storeTokens (const juce::var& json, juce::String& error)
{
    const auto token = json["access_token"].toString();
    if (token.isEmpty())
    {
        error = "Spotify no ha dado acceso: " + json["error_description"].toString() + " " + json["error"].toString();
        return false;
    }

    const juce::ScopedLock sl (lock);
    accessToken = token;
    tokenExpiry = juce::Time::getMillisecondCounter() + (juce::uint32) juce::jmax (60, (int) json["expires_in"] - 60) * 1000u;
    if (json["refresh_token"].toString().isNotEmpty())
    {
        settings.setValue ("spotifyRefreshToken", json["refresh_token"].toString());
        settings.saveIfNeeded();
    }
    return true;
}

juce::var SpotifyClient::postForm (const juce::String& url, const juce::String& body, juce::String& error)
{
    int status = 0;
    auto stream = juce::URL (url).withPOSTData (body).createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders ("Content-Type: application/x-www-form-urlencoded")
            .withConnectionTimeoutMs (15000)
            .withStatusCode (&status));

    if (stream == nullptr)
    {
        error = juce::String::fromUTF8 ("No se puede conectar con Spotify (¿hay internet?)");
        return {};
    }
    return juce::JSON::parse (stream->readEntireStreamAsString());
}

bool SpotifyClient::login (const std::function<bool()>& shouldStop, juce::String& error)
{
    const auto clientId = getClientId();
    if (clientId.isEmpty())
    {
        error = "Falta el Client ID de Spotify";
        return false;
    }

    // PKCE: un secreto de un solo uso; a Spotify solo se le envía su huella SHA-256
    const auto verifier = randomToken (64);
    const juce::SHA256 hash (verifier.toRawUTF8(), (size_t) verifier.getNumBytesAsUTF8());
    const auto digest = hash.getRawData();
    const auto challenge = base64Url (digest.getData(), digest.getSize());
    const auto state = randomToken (16);

    // Servidor local de un solo uso para recibir la respuesta del navegador
    juce::StreamingSocket listener;
    if (! listener.createListener (8888, "127.0.0.1"))
    {
        error = juce::String::fromUTF8 ("El puerto 8888 está ocupado por otro programa; ciérralo y vuelve a intentarlo");
        return false;
    }

    const auto authUrl = juce::String ("https://accounts.spotify.com/authorize?response_type=code")
        + "&client_id=" + urlEncode (clientId)
        + "&redirect_uri=" + urlEncode (redirectUri)
        + "&code_challenge_method=S256&code_challenge=" + challenge
        + "&state=" + state
        + "&scope=" + urlEncode ("playlist-read-private playlist-read-collaborative user-library-read");
    juce::URL (authUrl).launchInDefaultBrowser();

    juce::String code;
    const auto deadline = juce::Time::getMillisecondCounter() + 180000;
    while (code.isEmpty() && juce::Time::getMillisecondCounter() < deadline && ! shouldStop())
    {
        if (listener.waitUntilReady (true, 500) != 1)
            continue;

        std::unique_ptr<juce::StreamingSocket> client (listener.waitForNextConnection());
        if (client == nullptr)
            continue;

        char buffer[4096] = {};
        client->waitUntilReady (true, 3000);
        client->read (buffer, sizeof (buffer) - 1, false);
        const juce::String request (buffer);
        const auto path = request.fromFirstOccurrenceOf ("GET ", false, false).upToFirstOccurrenceOf (" ", false, false);
        const juce::URL u ("http://127.0.0.1" + path);

        juce::String page;
        if (u.getParameterValues()[u.getParameterNames().indexOf ("state")] == state)
            code = u.getParameterValues()[u.getParameterNames().indexOf ("code")];

        page = code.isNotEmpty()
            ? juce::String::fromUTF8 ("<h2>CesarDJ ya está conectado con Spotify.</h2><p>Puedes cerrar esta pestaña y volver a CesarDJ.</p>")
            : juce::String::fromUTF8 ("<h2>No se pudo conectar.</h2><p>Vuelve a CesarDJ e inténtalo de nuevo.</p>");
        const auto html = "<html><head><meta charset='utf-8'><title>CesarDJ</title></head>"
                          "<body style='font-family:sans-serif;background:#141519;color:#ddd;text-align:center;padding-top:80px'>"
                          + page + "</body></html>";
        const auto response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                              + juce::String ((int) html.getNumBytesAsUTF8()) + "\r\n\r\n" + html;
        client->write (response.toRawUTF8(), (int) response.getNumBytesAsUTF8());

        if (code.isEmpty() && u.getParameterNames().contains ("error"))
        {
            error = "Has cancelado el acceso en Spotify";
            return false;
        }
    }

    if (code.isEmpty())
    {
        error = juce::String::fromUTF8 ("No se recibió respuesta de Spotify (tiempo agotado)");
        return false;
    }

    const auto json = postForm ("https://accounts.spotify.com/api/token",
                                "grant_type=authorization_code&code=" + urlEncode (code)
                                + "&redirect_uri=" + urlEncode (redirectUri)
                                + "&client_id=" + urlEncode (clientId)
                                + "&code_verifier=" + verifier, error);
    return error.isEmpty() && storeTokens (json, error);
}

bool SpotifyClient::ensureToken (juce::String& error)
{
    {
        const juce::ScopedLock sl (lock);
        if (accessToken.isNotEmpty() && juce::Time::getMillisecondCounter() < tokenExpiry)
            return true;
    }

    const auto refresh = settings.getValue ("spotifyRefreshToken");
    if (refresh.isEmpty())
    {
        error = juce::String::fromUTF8 ("Inicia sesión en Spotify");
        return false;
    }

    const auto json = postForm ("https://accounts.spotify.com/api/token",
                                "grant_type=refresh_token&refresh_token=" + urlEncode (refresh)
                                + "&client_id=" + urlEncode (getClientId()), error);
    if (! error.isEmpty())
        return false;   // sin internet: la sesión se conserva

    if (! storeTokens (json, error))
    {
        // Permiso caducado (los de Spotify duran hasta 180 días) o revocado: hay que volver a entrar
        if (json["error"].toString() == "invalid_grant")
        {
            logout();
            error = juce::String::fromUTF8 ("La sesión de Spotify ha caducado. Vuelve a iniciar sesión (SPOTIFY > Iniciar sesión).");
        }
        return false;
    }
    return true;
}

juce::var SpotifyClient::request (const juce::String& url, juce::String& error, int& status)
{
    juce::String token;
    {
        const juce::ScopedLock sl (lock);
        token = accessToken;
    }

    status = 0;
    auto stream = juce::URL (url).createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders ("Authorization: Bearer " + token)
            .withConnectionTimeoutMs (15000)
            .withStatusCode (&status));

    if (stream == nullptr)
    {
        error = juce::String::fromUTF8 ("No se puede conectar con Spotify (¿hay internet?)");
        return {};
    }

    const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
    if (status == 403)
        error = juce::String::fromUTF8 ("Esta cuenta de Spotify no está autorizada en CesarDJ (403).\n\n"
                                        "El administrador de CesarDJ debe añadirla en developer.spotify.com > CesarDJ > "
                                        "User Management (nombre y correo de la cuenta).\n\n"
                                        "Mientras tanto puedes importar tus listas con un archivo CSV (Importar > Desde un archivo).");
    else if (status >= 400)
        error = "Error de Spotify (" + juce::String (status) + "): " + json["error"]["message"].toString();
    return json;
}

std::vector<SpotifyClient::PlaylistInfo> SpotifyClient::listPlaylists (juce::String& error)
{
    std::vector<PlaylistInfo> lists;
    if (! ensureToken (error))
        return lists;

    int status = 0;
    const auto liked = request ("https://api.spotify.com/v1/me/tracks?limit=1", error, status);
    if (! error.isEmpty())
        return lists;
    lists.push_back ({ likedId, juce::String::fromUTF8 ("Canciones que te gustan"), (int) liked["total"] });

    juce::String next = "https://api.spotify.com/v1/me/playlists?limit=50";
    while (next.isNotEmpty())
    {
        const auto page = request (next, error, status);
        if (! error.isEmpty())
            return lists;

        if (auto* items = page["items"].getArray())
            for (auto& p : *items)
            {
                if (p.isVoid() || p["id"].toString().isEmpty())
                    continue;
                const auto counts = p.hasProperty ("tracks") ? p["tracks"] : p["items"];   // nombre según versión de la API
                lists.push_back ({ p["id"].toString(), p["name"].toString(), (int) counts["total"] });
            }
        next = page["next"].toString();
    }

    // Álbumes guardados
    next = "https://api.spotify.com/v1/me/albums?limit=50";
    while (next.isNotEmpty())
    {
        const auto page = request (next, error, status);
        if (! error.isEmpty())
        {
            error.clear();      // sin álbumes no es un error grave
            break;
        }
        if (auto* items = page["items"].getArray())
            for (auto& item : *items)
            {
                const auto a = item["album"];
                juce::StringArray artists;
                if (auto* ar = a["artists"].getArray())
                    for (auto& artist : *ar)
                        artists.add (artist["name"].toString());
                lists.push_back ({ albumPrefix + a["id"].toString(),
                                   juce::String::fromUTF8 ("Álbum: ") + a["name"].toString() + " - " + artists.joinIntoString (", "),
                                   (int) a["total_tracks"] });
            }
        next = page["next"].toString();
    }
    return lists;
}

std::optional<ImportedPlaylist> SpotifyClient::fetchPlaylist (const PlaylistInfo& info, juce::String& error)
{
    if (! ensureToken (error))
        return std::nullopt;

    ImportedPlaylist p;
    p.name = info.name;
    p.source = "Spotify";

    const bool isAlbum = info.id.startsWith (albumPrefix);
    const auto albumName = isAlbum ? info.name.fromFirstOccurrenceOf (": ", false, false).upToLastOccurrenceOf (" - ", false, false) : juce::String();
    juce::String next = info.id == likedId ? juce::String ("https://api.spotify.com/v1/me/tracks?limit=50")
                      : isAlbum ? "https://api.spotify.com/v1/albums/" + info.id.fromFirstOccurrenceOf (albumPrefix, false, false) + "/tracks?limit=50"
                                : "https://api.spotify.com/v1/playlists/" + info.id + "/tracks?limit=100";
    while (next.isNotEmpty())
    {
        int status = 0;
        const auto page = request (next, error, status);
        if (! error.isEmpty())
            return std::nullopt;

        if (auto* items = page["items"].getArray())
            for (auto& item : *items)
            {
                const auto t = isAlbum ? item : (item.hasProperty ("track") ? item["track"] : item["item"]);
                if (t.isVoid() || t["type"].toString() == "episode")
                    continue;       // canciones borradas o podcasts

                ImportedTrack track;
                track.title = t["name"].toString();
                juce::StringArray artists;
                if (auto* a = t["artists"].getArray())
                    for (auto& artist : *a)
                        artists.add (artist["name"].toString());
                track.artist = artists.joinIntoString (", ");
                track.album = isAlbum ? albumName : t["album"]["name"].toString();
                track.duration = (double) t["duration_ms"] / 1000.0;
                if (track.title.isNotEmpty())
                    p.tracks.push_back (track);
            }
        next = page["next"].toString();
    }
    return p;
}

//==============================================================================
// Importador
//==============================================================================
PlaylistImporter::PlaylistImporter (Library& lib, juce::PropertiesFile& settings)
    : library (lib), spotify (settings)
{
}

PlaylistImporter::~PlaylistImporter()
{
    alive.reset();
    cancel = true;
    pool.removeAllJobs (true, 5000);
}

void PlaylistImporter::status (const juce::String& message)
{
    std::weak_ptr<bool> weak = alive;
    juce::MessageManager::callAsync ([this, weak, message] { if (! weak.expired() && onStatus) onStatus (message); });
}

// Empareja la lista con la biblioteca, crea la lista en CesarDJ y guarda lo que falta en "Por conseguir"
PlaylistImporter::Result PlaylistImporter::importPlaylist (const ImportedPlaylist& p)
{
    const auto lib = library.allTracks();
    std::vector<TrackMatcher::Key> keys;
    keys.reserve (lib.size());
    for (auto& t : lib)
        keys.push_back (TrackMatcher::makeKey (t.title, t.artist, t.duration));

    std::vector<std::pair<juce::int64, int>> found;
    std::vector<LibraryTrack> missing;
    std::vector<int> missingPositions;

    for (size_t i = 0; i < p.tracks.size(); ++i)
    {
        const auto& t = p.tracks[i];
        const int position = (int) i + 1;

        if (t.file.existsAsFile())          // M3U con la ruta del archivo: directo
        {
            if (auto id = library.ensureTrack (t.file); id > 0)
            {
                found.push_back ({ id, position });
                continue;
            }
        }

        const auto want = TrackMatcher::makeKey (t.title, t.artist, t.duration);
        double best = 0.0;
        size_t bestIndex = 0;
        for (size_t k = 0; k < keys.size(); ++k)
        {
            const double s = TrackMatcher::score (want, keys[k]);
            if (s > best) { best = s; bestIndex = k; }
        }

        if (best >= TrackMatcher::matchThreshold)
        {
            found.push_back ({ lib[bestIndex].id, position });
        }
        else
        {
            LibraryTrack m;
            m.title = t.title;
            m.artist = t.artist;
            m.album = t.album;
            m.duration = t.duration;
            missing.push_back (m);
            missingPositions.push_back (position);
        }
    }

    Result r;
    r.total = (int) p.tracks.size();
    r.found = (int) found.size();
    r.missing = (int) missing.size();

    const auto id = library.createUniquePlaylist ((p.source == "Spotify" ? juce::String ("Spotify - ") : juce::String()) + p.name);
    for (auto& pl : library.getPlaylists())
        if (pl.id == id)
            r.playlistName = pl.name;

    library.addToPlaylistAt (id, found);
    if (! missing.empty())
        library.addWanted (id, p.source, missing, missingPositions);
    return r;
}

void PlaylistImporter::report (const Result& r)
{
    juce::String msg = juce::String::fromUTF8 ("Lista \"") + r.playlistName + "\": " + juce::String (r.found) + " de "
                      + juce::String (r.total) + juce::String::fromUTF8 (" canciones están en tu biblioteca.");
    if (r.missing > 0)
        msg += juce::String::fromUTF8 ("\n\nLas ") + juce::String (r.missing) + juce::String::fromUTF8 (
                    " que faltan están en \"Por conseguir\": clic derecho para buscarlas en Beatport, Bandcamp, YouTube... "
                    "Cuando las añadas a tu carpeta de música se colocarán solas en la lista.");

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, juce::String::fromUTF8 ("Importación terminada"), msg);
}

void PlaylistImporter::importFromFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Elige una lista (CSV de Spotify/Apple/Deezer/TIDAL, o M3U)",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads"),
                                                   "*.csv;*.m3u;*.m3u8");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (! f.existsAsFile())
                                  return;

                              std::weak_ptr<bool> weak = alive;
                              status ("Importando " + f.getFileName() + "...");
                              pool.addJob ([this, weak, f]
                              {
                                  const auto p = f.hasFileExtension ("csv") ? PlaylistFiles::readCsv (f) : PlaylistFiles::readM3u (f);
                                  if (! p.has_value() || p->tracks.empty())
                                  {
                                      status ({});
                                      juce::MessageManager::callAsync ([f]
                                      {
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No se pudo importar",
                                              juce::String::fromUTF8 ("No se han encontrado canciones en ") + f.getFileName()
                                              + juce::String::fromUTF8 (".\nEl CSV necesita al menos una columna de título (Track Name, Title...)."));
                                      });
                                      return;
                                  }

                                  const auto r = importPlaylist (*p);
                                  status ({});
                                  juce::MessageManager::callAsync ([this, weak, r] { if (! weak.expired()) report (r); });
                              });
                          });
}

void PlaylistImporter::exportPlaylist (juce::int64 playlistId, const juce::String& name)
{
    LibraryView v;
    v.type = LibraryView::Type::playlist;
    v.playlistId = playlistId;
    const auto tracks = library.query (v, {}, {}, true);

    chooser = std::make_unique<juce::FileChooser> ("Guardar la lista como M3U",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                       .getChildFile (juce::File::createLegalFileName (name) + ".m3u8"),
                                                   "*.m3u8;*.m3u");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, tracks] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;
                              if (! f.hasFileExtension ("m3u;m3u8"))
                                  f = f.withFileExtension ("m3u8");
                              if (PlaylistFiles::writeM3u (f, tracks))
                                  status ("Lista exportada: " + f.getFullPathName());
                          });
}

//==============================================================================
// Spotify: diálogos
//==============================================================================
namespace
{
    class SpotifySetupPanel : public juce::Component
    {
    public:
        std::function<void (const juce::String&)> onConnect;

        explicit SpotifySetupPanel (const juce::String& currentId)
        {
            info.setText (juce::String::fromUTF8 (
                "Para leer tus listas, Spotify pide registrar una app gratuita (solo una vez):\n\n"
                "1. Pulsa \"Abrir panel de Spotify\" e inicia sesión con tu cuenta.\n"
                "2. \"Create app\": nombre CesarDJ, en Redirect URIs pega la dirección de abajo "
                "(botón \"Copiar\") y marca \"Web API\".\n"
                "3. Copia el \"Client ID\" de tu app y pégalo aquí.\n\n"
                "CesarDJ solo lee títulos y artistas de tus listas. Nunca ve tu contraseña "
                "(inicias sesión en la web de Spotify) y no usa el audio de Spotify."), juce::dontSendNotification);
            info.setJustificationType (juce::Justification::topLeft);
            info.setFont (juce::FontOptions (14.0f));
            addAndMakeVisible (info);

            redirect.setText (SpotifyClient::redirectUri, juce::dontSendNotification);
            redirect.setReadOnly (true);
            addAndMakeVisible (redirect);

            clientId.setTextToShowWhenEmpty ("Client ID (32 caracteres)", textDim);
            clientId.setText (currentId, false);
            addAndMakeVisible (clientId);

            openPanel.onClick = [] { juce::URL ("https://developer.spotify.com/dashboard").launchInDefaultBrowser(); };
            copy.onClick = [] { juce::SystemClipboard::copyTextToClipboard (SpotifyClient::redirectUri); };
            connect.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1db954).darker (0.3f));
            connect.onClick = [this]
            {
                const auto id = clientId.getText().trim();
                if (id.length() < 20)
                {
                    clientId.setColour (juce::TextEditor::outlineColourId, juce::Colours::red);
                    return;
                }
                if (onConnect)
                    onConnect (id);
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState (1);
            };
            for (auto* b : { &openPanel, &copy, &connect })
                addAndMakeVisible (b);
            setSize (560, 380);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16);
            info.setBounds (r.removeFromTop (200));
            auto row = r.removeFromTop (30);
            copy.setBounds (row.removeFromRight (90));
            redirect.setBounds (row.withTrimmedRight (8));
            r.removeFromTop (12);
            clientId.setBounds (r.removeFromTop (30));
            r.removeFromTop (16);
            auto buttons = r.removeFromTop (34);
            openPanel.setBounds (buttons.removeFromLeft (200));
            connect.setBounds (buttons.removeFromRight (180));
        }

    private:
        juce::Label info;
        juce::TextEditor redirect, clientId;
        juce::TextButton openPanel { "Abrir panel de Spotify" }, copy { "Copiar" }, connect { "Guardar y conectar" };
    };

    class SpotifyChooserPanel : public juce::Component
    {
    public:
        std::function<void (std::vector<SpotifyClient::PlaylistInfo>)> onImport;
        std::function<void()> onLogout;

        explicit SpotifyChooserPanel (std::vector<SpotifyClient::PlaylistInfo> l) : lists (std::move (l))
        {
            title.setText (juce::String::fromUTF8 ("Elige las listas que quieres traer a CesarDJ:"), juce::dontSendNotification);
            title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            addAndMakeVisible (title);

            for (auto& p : lists)
            {
                auto* t = toggles.add (new juce::ToggleButton (p.name + "  (" + juce::String (p.count) + ")"));
                content.addAndMakeVisible (t);
            }
            viewport.setViewedComponent (&content, false);
            viewport.setScrollBarsShown (true, false);
            addAndMakeVisible (viewport);

            all.onClick = [this] { for (auto* t : toggles) t->setToggleState (true, juce::dontSendNotification); };
            logout.onClick = [this]
            {
                if (onLogout) onLogout();
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState (0);
            };
            import.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1db954).darker (0.3f));
            import.onClick = [this]
            {
                std::vector<SpotifyClient::PlaylistInfo> chosen;
                for (int i = 0; i < toggles.size(); ++i)
                    if (toggles[i]->getToggleState())
                        chosen.push_back (lists[(size_t) i]);
                if (! chosen.empty() && onImport)
                    onImport (chosen);
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState (1);
            };
            for (auto* b : { &all, &logout, &import })
                addAndMakeVisible (b);
            setSize (520, 520);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16);
            title.setBounds (r.removeFromTop (26));
            auto buttons = r.removeFromBottom (34);
            r.removeFromBottom (10);
            viewport.setBounds (r);
            content.setSize (r.getWidth() - 12, toggles.size() * 28);
            for (int i = 0; i < toggles.size(); ++i)
                toggles[i]->setBounds (0, i * 28, content.getWidth(), 26);
            all.setBounds (buttons.removeFromLeft (150));
            buttons.removeFromLeft (8);
            logout.setBounds (buttons.removeFromLeft (150));
            import.setBounds (buttons.removeFromRight (130));
        }

    private:
        std::vector<SpotifyClient::PlaylistInfo> lists;
        juce::Label title;
        juce::Viewport viewport;
        juce::Component content;
        juce::OwnedArray<juce::ToggleButton> toggles;
        juce::TextButton all { "Seleccionar todas" }, logout { juce::String::fromUTF8 ("Cerrar sesión") }, import { "Importar" };
    };

    void launch (juce::Component* content, const juce::String& title)
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (content);
        o.dialogTitle = title;
        o.dialogBackgroundColour = panel;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }
}

void PlaylistImporter::importFromSpotify()
{
    if (! spotify.hasSession())
        loginSpotify();
    else if (! spotifyLists.empty())
        showSpotifyChooser (spotifyLists);
    else
        refreshSpotify();
}

void PlaylistImporter::showSpotifySetup()
{
    auto* panelComp = new SpotifySetupPanel (spotify.getClientId());
    panelComp->onConnect = [this] (const juce::String& id)
    {
        spotify.setClientId (id);
        spotify.logout();
        juce::MessageManager::callAsync ([this] { loginSpotify(); });
    };
    launch (panelComp, "Conectar CesarDJ con Spotify");
}

void PlaylistImporter::showSpotifyChooser (std::vector<SpotifyClient::PlaylistInfo> lists)
{
    auto* panelComp = new SpotifyChooserPanel (std::move (lists));
    panelComp->onLogout = [this] { spotify.logout(); status (juce::String::fromUTF8 ("Sesión de Spotify cerrada")); };
    panelComp->onImport = [this] (std::vector<SpotifyClient::PlaylistInfo> chosen)
    {
        std::weak_ptr<bool> weak = alive;
        pool.addJob ([this, weak, chosen]
        {
            for (size_t i = 0; i < chosen.size(); ++i)
            {
                status ("Spotify: importando " + chosen[i].name + " (" + juce::String ((int) i + 1) + "/" + juce::String ((int) chosen.size()) + ")...");
                juce::String error;
                const auto p = spotify.fetchPlaylist (chosen[i], error);
                if (! p.has_value())
                {
                    juce::MessageManager::callAsync ([error]
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Spotify", error);
                    });
                    continue;
                }

                const auto r = importPlaylist (*p);
                juce::MessageManager::callAsync ([this, weak, r] { if (! weak.expired()) report (r); });
            }
            status ({});
        });
    };
    launch (panelComp, "Listas de Spotify");
}

//==============================================================================
// Spotify integrado: inicio de sesión dentro de CesarDJ (WebView2), sesión guardada
//==============================================================================
namespace
{
    juce::File webViewDataFolder()
    {
        return juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("CesarDJ").getChildFile ("WebView2");
    }

    juce::WebBrowserComponent::Options loginBrowserOptions()
    {
        return juce::WebBrowserComponent::Options()
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2().withUserDataFolder (webViewDataFolder()));
    }

    // Página oficial de Spotify; cuando Spotify vuelve a la dirección de retorno, se intercepta (no se carga)
    class SpotifyLoginBrowser : public juce::WebBrowserComponent
    {
    public:
        SpotifyLoginBrowser (std::function<void (const juce::String&)> onRedirect)
            : juce::WebBrowserComponent (loginBrowserOptions()), redirect (std::move (onRedirect)) {}

        bool pageAboutToLoad (const juce::String& url) override
        {
            if (! url.startsWith (SpotifyClient::redirectUri))
                return true;

            if (! handled && redirect != nullptr)
            {
                handled = true;
                auto r = redirect;
                juce::MessageManager::callAsync ([r, url] { r (url); });
            }
            return false;
        }

        void newWindowAttemptingToLoad (const juce::String& url) override   { goToURL (url); }

    private:
        std::function<void (const juce::String&)> redirect;
        bool handled = false;
    };

    class SpotifyLoginWindow : public juce::DocumentWindow
    {
    public:
        SpotifyLoginWindow (const juce::String& url, std::function<void (const juce::String&)> onRedirect, std::function<void()> onClose)
            : juce::DocumentWindow (juce::String::fromUTF8 ("Iniciar sesión en Spotify"), panel, juce::DocumentWindow::closeButton),
              closed (std::move (onClose))
        {
            auto* browser = new SpotifyLoginBrowser (std::move (onRedirect));
            browser->setSize (500, 760);
            setContentOwned (browser, true);
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            centreWithSize (500, 760);
            setVisible (true);
            toFront (true);
            browser->goToURL (url);
        }

        void closeButtonPressed() override      { if (closed != nullptr) closed(); }

    private:
        std::function<void()> closed;
    };

    void makePkce (juce::String& verifier, juce::String& challenge)
    {
        verifier = randomToken (64);
        const juce::SHA256 hash (verifier.toRawUTF8(), (size_t) verifier.getNumBytesAsUTF8());
        const auto digest = hash.getRawData();
        challenge = base64Url (digest.getData(), digest.getSize());
    }
}

void PlaylistImporter::loginSpotify()
{
    if (spotify.getClientId().isEmpty())
    {
        showSpotifySetup();
        return;
    }
    startLogin();
}

void PlaylistImporter::startLogin()
{
    if (loginWindow != nullptr)
    {
        loginWindow->toFront (true);
        return;
    }

    // Sin WebView2 (raro en Windows 10/11): se usa el navegador del sistema
    if (! juce::WebBrowserComponent::areOptionsSupported (loginBrowserOptions()))
    {
        std::weak_ptr<bool> weak = alive;
        status (juce::String::fromUTF8 ("Spotify: inicia sesión en la ventana del navegador..."));
        cancel = false;
        pool.addJob ([this, weak]
        {
            juce::String error;
            const bool ok = spotify.login ([this] { return cancel.load(); }, error) && spotify.fetchUserName (error);
            if (ok)
                spotifyLists = spotify.listPlaylists (error);
            juce::MessageManager::callAsync ([this, weak, ok, error]
            {
                if (weak.expired()) return;
                if (! ok || error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Spotify", error);
                if (onStatus) onStatus ({});
                publishSpotifyState();
            });
        });
        return;
    }

    juce::String challenge;
    makePkce (pendingVerifier, challenge);
    pendingState = randomToken (16);
    const auto url = spotify.makeAuthUrl (challenge, pendingState, true);

    std::weak_ptr<bool> weak = alive;
    loginWindow = std::make_unique<SpotifyLoginWindow> (url,
        [this, weak] (const juce::String& redirectUrl)
        {
            if (weak.expired()) return;
            const juce::URL u (redirectUrl);
            const auto names = u.getParameterNames();
            const auto values = u.getParameterValues();
            const auto code = values[names.indexOf ("code")];
            const bool stateOk = values[names.indexOf ("state")] == pendingState;

            juce::MessageManager::callAsync ([this, weak] { if (! weak.expired()) loginWindow = nullptr; });

            if (code.isNotEmpty() && stateOk)
                finishLogin (code);
            else
                status (juce::String::fromUTF8 ("Spotify: inicio de sesión cancelado"));
        },
        [this, weak]
        {
            juce::MessageManager::callAsync ([this, weak] { if (! weak.expired()) loginWindow = nullptr; });
        });
}

void PlaylistImporter::finishLogin (const juce::String& code)
{
    std::weak_ptr<bool> weak = alive;
    status ("Spotify: conectando...");
    const auto verifier = pendingVerifier;

    pool.addJob ([this, weak, code, verifier]
    {
        juce::String error;
        const bool ok = spotify.exchangeCode (code, verifier, error) && spotify.fetchUserName (error);
        auto lists = ok ? spotify.listPlaylists (error) : std::vector<ExternalPlaylist>();

        juce::MessageManager::callAsync ([this, weak, ok, error, lists]
        {
            if (weak.expired()) return;
            if (! ok || error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Spotify", error);
                if (onStatus) onStatus ({});
            }
            else
            {
                spotifyLists = lists;
                status ("Spotify: conectado como " + spotify.getUserName());
            }
            publishSpotifyState();
        });
    });
}

void PlaylistImporter::logoutSpotify()
{
    spotify.logout();
    spotifyLists.clear();

    // Se borran también las cookies de la ventana de Spotify: la próxima vez se puede entrar con otra cuenta
    if (loginWindow == nullptr)
        webViewDataFolder().deleteRecursively();

    status (juce::String::fromUTF8 ("Sesión de Spotify cerrada"));
    publishSpotifyState();
}

void PlaylistImporter::refreshSpotify()
{
    if (! spotify.hasSession())
    {
        publishSpotifyState();
        return;
    }

    std::weak_ptr<bool> weak = alive;
    pool.addJob ([this, weak]
    {
        juce::String error;
        auto lists = spotify.listPlaylists (error);
        juce::MessageManager::callAsync ([this, weak, lists, error]
        {
            if (weak.expired()) return;
            if (error.isNotEmpty())
                status ("Spotify: " + error.upToFirstOccurrenceOf ("\n", false, false));
            else
                spotifyLists = lists;
            publishSpotifyState();
        });
    });
}

void PlaylistImporter::publishSpotifyState()
{
    if (onSpotifyState != nullptr)
        onSpotifyState (spotify.hasSession(), spotify.getUserName(), spotifyLists);
}

const ExternalPlaylist* PlaylistImporter::findSpotifyList (const juce::String& id) const
{
    for (auto& l : spotifyLists)
        if (l.id == id)
            return &l;
    return nullptr;
}

void PlaylistImporter::importSpotifyPlaylist (const ExternalPlaylist& info)
{
    std::weak_ptr<bool> weak = alive;
    status ("Spotify: importando " + info.name + "...");
    pool.addJob ([this, weak, info]
    {
        juce::String error;
        const auto p = spotify.fetchPlaylist (info, error);
        status ({});
        juce::MessageManager::callAsync ([this, weak, p, error]
        {
            if (weak.expired()) return;
            if (! p.has_value())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Spotify", error);
        });
        if (p.has_value())
        {
            const auto r = importPlaylist (*p);
            juce::MessageManager::callAsync ([this, weak, r] { if (! weak.expired()) report (r); });
        }
    });
}

void PlaylistImporter::importAllSpotify()
{
    if (! spotifyLists.empty())
        showSpotifyChooser (spotifyLists);
}

// Canciones de una lista de Spotify emparejadas con la biblioteca, para verlas en el navegador
void PlaylistImporter::fetchSpotifyTracks (const juce::String& id, std::function<void (std::vector<LibraryTrack>)> onDone)
{
    const auto* info = findSpotifyList (id);
    if (info == nullptr)
    {
        juce::MessageManager::callAsync ([onDone] { onDone ({}); });
        return;
    }

    std::weak_ptr<bool> weak = alive;
    const auto copy = *info;
    pool.addJob ([this, weak, copy, onDone]
    {
        juce::String error;
        const auto p = spotify.fetchPlaylist (copy, error);
        std::vector<LibraryTrack> rows;

        if (p.has_value())
        {
            const auto lib = library.allTracks();
            std::vector<TrackMatcher::Key> keys;
            keys.reserve (lib.size());
            for (auto& t : lib)
                keys.push_back (TrackMatcher::makeKey (t.title, t.artist, t.duration));

            for (auto& t : p->tracks)
            {
                const auto want = TrackMatcher::makeKey (t.title, t.artist, t.duration);
                double best = 0.0;
                size_t bestIndex = 0;
                for (size_t k = 0; k < keys.size(); ++k)
                {
                    const double s = TrackMatcher::score (want, keys[k]);
                    if (s > best) { best = s; bestIndex = k; }
                }

                LibraryTrack row;
                if (best >= TrackMatcher::matchThreshold)
                    row = lib[bestIndex];           // archivo, BPM, tono... de tu biblioteca
                row.title = t.title;
                row.artist = t.artist;
                row.album = t.album;
                row.duration = t.duration;
                rows.push_back (row);
            }
        }

        juce::MessageManager::callAsync ([weak, rows, onDone, error]
        {
            if (weak.expired()) return;
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Spotify", error);
            onDone (rows);
        });
    });
}
