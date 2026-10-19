#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Library.h"

// Una lista traída de fuera (Spotify, CSV, M3U): solo títulos y artistas, nunca el audio
struct ImportedTrack
{
    juce::String title, artist, album;
    double duration = 0.0;
    juce::File file;                    // solo en M3U, si la ruta existe
};

struct ImportedPlaylist
{
    juce::String name, source;
    std::vector<ImportedTrack> tracks;
};

namespace PlaylistFiles
{
    std::optional<ImportedPlaylist> readCsv (const juce::File& file);      // Exportify, TuneMyMusic, Soundiiz...
    std::optional<ImportedPlaylist> readM3u (const juce::File& file);      // M3U / M3U8 de cualquier programa
    bool writeM3u (const juce::File& file, const std::vector<LibraryTrack>& tracks);
}

//==============================================================================
// Spotify Web API: solo lectura de listas (títulos/artistas). Inicio de sesión con OAuth PKCE:
// el usuario entra en la web de Spotify y CesarDJ nunca ve su contraseña.
class SpotifyClient
{
public:
    static constexpr const char* redirectUri = "http://127.0.0.1:8888/callback";
    static constexpr const char* likedId = "__liked__";

    struct PlaylistInfo
    {
        juce::String id, name;
        int count = 0;
    };

    explicit SpotifyClient (juce::PropertiesFile& settings);

    juce::String getClientId() const;
    void setClientId (const juce::String& id);
    bool hasSession() const;
    void logout();

    // Todo lo siguiente es bloqueante: llamar desde un hilo de fondo
    bool login (const std::function<bool()>& shouldStop, juce::String& error);
    bool ensureToken (juce::String& error);
    std::vector<PlaylistInfo> listPlaylists (juce::String& error);
    std::optional<ImportedPlaylist> fetchPlaylist (const PlaylistInfo& playlist, juce::String& error);

private:
    juce::var request (const juce::String& url, juce::String& error, int& status);
    juce::var postForm (const juce::String& url, const juce::String& body, juce::String& error);
    bool storeTokens (const juce::var& json, juce::String& error);

    juce::PropertiesFile& settings;
    juce::String accessToken;
    juce::uint32 tokenExpiry = 0;
    juce::CriticalSection lock;
};

//==============================================================================
// Orquesta las importaciones en segundo plano y muestra los diálogos y avisos
class PlaylistImporter
{
public:
    PlaylistImporter (Library& library, juce::PropertiesFile& settings);
    ~PlaylistImporter();

    void importFromFile();                              // CSV o M3U (elige el usuario)
    void importFromSpotify();
    void exportPlaylist (juce::int64 playlistId, const juce::String& name);

    std::function<void (const juce::String&)> onStatus;     // mensajes para la barra superior

private:
    struct Result { juce::String playlistName; int total = 0, found = 0, missing = 0; };

    Result importPlaylist (const ImportedPlaylist& playlist);
    void report (const Result& r);
    void showSpotifySetup();
    void showSpotifyChooser (std::vector<SpotifyClient::PlaylistInfo> lists);
    void status (const juce::String& text);

    Library& library;
    SpotifyClient spotify;
    juce::ThreadPool pool { 1 };
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    std::atomic<bool> cancel { false };
};
