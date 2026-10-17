#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "TagReader.h"

struct sqlite3;

// Una canción de la biblioteca
struct LibraryTrack
{
    juce::int64 id = 0;
    juce::int64 entryId = 0;            // fila dentro de una lista (para quitarla o reordenar)
    juce::File file;
    juce::String title, artist, album, genre, year, comment, key;
    double duration = 0.0, bpm = 0.0, firstBeat = 0.0;
    bool analysed = false;
    bool keyAnalysed = false;           // tonalidad calculada por CesarDJ (no solo la de la etiqueta)
    int rating = 0, playCount = 0;
    juce::int64 lastPlayed = 0, dateAdded = 0, playedAt = 0;
    double cuePoint = 0.0;
    std::array<double, 4> hotCues { -1.0, -1.0, -1.0, -1.0 };
};

struct LibraryPlaylist
{
    juce::int64 id = 0;
    juce::String name;
    int count = 0;
};

// Qué se está viendo en el navegador
struct LibraryView
{
    enum class Type { all, recentlyAdded, history, playlist, folder };
    Type type = Type::all;
    juce::int64 playlistId = 0;
    juce::File folder;

    bool operator== (const LibraryView& o) const
    {
        return type == o.type && playlistId == o.playlistId && folder == o.folder;
    }
};

// Biblioteca de música en SQLite (library.db).
// Hilo de mensajes: consultas y cambios. Hilo propio: escanear carpetas y analizar BPM en lote.
// Avisa de cambios con ChangeBroadcaster (siempre llega al hilo de mensajes).
class Library : public juce::ChangeBroadcaster
{
public:
    Library (const juce::File& databaseFile, const juce::String& audioWildcard);
    ~Library() override;

    bool isOpen() const                                 { return db != nullptr; }

    //== Consultas ================================================================
    // 'search': palabras que deben aparecer todas (en título, artista, álbum o género)
    // 'bpmMin/bpmMax' > 0: filtrar por BPM
    std::vector<LibraryTrack> query (const LibraryView& view, const juce::String& search,
                                     const juce::String& sortColumn, bool ascending,
                                     double bpmMin = 0.0, double bpmMax = 0.0);
    std::optional<LibraryTrack> findByFile (const juce::File& file);
    int countTracks();

    //== Carpetas =================================================================
    juce::Array<juce::File> getFolders();
    void addFolder (const juce::File& folder);          // la escanea en segundo plano
    void removeFolder (const juce::File& folder);       // quita también sus canciones
    void rescanAll();
    void addFileAsync (const juce::File& file);         // canción suelta (p. ej. arrastrada desde el Explorador)

    //== Datos de canciones =======================================================
    // Guarda BPM, beatgrid y tonalidad (Camelot, "" si no se pudo calcular)
    void setAnalysis (const juce::File& file, double bpm, double firstBeat, double duration, const juce::String& key);
    void setCues (const juce::File& file, double cuePoint, const std::array<double, 4>& hotCues);
    void setRating (juce::int64 trackId, int rating);
    void markPlayed (const juce::File& file, int deck);
    void removeTracks (const juce::Array<juce::int64>& trackIds);
    void requeueAnalysis (const juce::Array<juce::int64>& trackIds);   // volver a calcular el BPM

    //== Listas ===================================================================
    std::vector<LibraryPlaylist> getPlaylists();
    juce::int64 createPlaylist (const juce::String& name);
    void renamePlaylist (juce::int64 id, const juce::String& name);
    void deletePlaylist (juce::int64 id);
    void addToPlaylist (juce::int64 playlistId, const juce::Array<juce::int64>& trackIds);
    void removeFromPlaylist (const juce::Array<juce::int64>& entryIds);
    void clearHistory();

    //== Trabajo en segundo plano ================================================
    void analysePending();                              // BPM de todas las canciones sin analizar
    void cancelAnalysis()                               { cancelAnalysisFlag = true; }
    bool isBusy() const                                 { return busy.load(); }
    juce::String getStatusText() const;
    int countPending();

private:
    class Worker;
    struct Statement;

    // Versión del esquema de la base de datos. Cada actualización de CesarDJ que cambie la base de datos
    // sube este número y añade su paso en migrate(): las bibliotecas antiguas se actualizan solas al abrirlas.
    static constexpr int schemaVersion = 2;

    void exec (const char* sql);
    void createSchema();
    void migrate();
    void scanFolder (const juce::File& folder);
    void analyseLoop();
    void insertOrUpdate (const juce::File& file, const TagInfo& tags, double duration);
    void notify()                                       { sendChangeMessage(); }

    sqlite3* db = nullptr;
    juce::CriticalSection lock;
    juce::String wildcard;
    juce::AudioFormatManager formats;           // propio: se usa desde el hilo de fondo

    std::unique_ptr<Worker> worker;
    std::atomic<bool> busy { false }, cancelAnalysisFlag { false };
    std::atomic<int> progressDone { 0 }, progressTotal { 0 };
    juce::String progressLabel;                 // protegido por 'lock'

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Library)
};
