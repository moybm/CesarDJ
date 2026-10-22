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
    static constexpr const char* albumPrefix = "album:";

    using PlaylistInfo = ExternalPlaylist;

    explicit SpotifyClient (juce::PropertiesFile& settings);

    // Client ID en uso: el propio (CONFIG > Spotify) si se ha puesto; si no, el de serie
    juce::String getClientId() const;
    void setClientId (const juce::String& id);
    static juce::String getBuiltInClientId();
    juce::String getCustomClientId() const;
    void setCustomClientId (const juce::String& id);            // vacío = volver al de serie
    static bool isValidClientId (const juce::String& id);       // 32 caracteres hexadecimales
    bool hasSession() const;
    void logout();
    juce::String getUserName() const;

    // Inicio de sesión (PKCE): la página de Spotify se abre dentro de CesarDJ o en el navegador
    juce::String makeAuthUrl (const juce::String& challenge, const juce::String& state, bool forceDialog) const;
    bool exchangeCode (const juce::String& code, const juce::String& verifier, juce::String& error);
    bool fetchUserName (juce::String& error);

    // Todo lo siguiente es bloqueante: llamar desde un hilo de fondo
    bool login (const std::function<bool()>& shouldStop, juce::String& error);
    bool ensureToken (juce::String& error);
    std::vector<PlaylistInfo> listPlaylists (juce::String& error);
    std::optional<ImportedPlaylist> fetchPlaylist (const PlaylistInfo& playlist, juce::String& error);

    // La última petición fue rechazada con 403: la cuenta no está autorizada (User Management)
    bool wasForbidden() const                       { return forbidden.load(); }

private:
    juce::var request (const juce::String& url, juce::String& error, int& status);
    juce::var postForm (const juce::String& url, const juce::String& body, juce::String& error);
    bool storeTokens (const juce::var& json, juce::String& error);

    juce::PropertiesFile& settings;
    juce::String accessToken;
    juce::uint32 tokenExpiry = 0;
    std::atomic<bool> forbidden { false };
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

    //== Spotify en el navegador (como VirtualDJ) ===================================
    bool isSpotifyConnected() const                 { return spotify.hasSession(); }
    juce::String getSpotifyUser() const             { return spotify.getUserName(); }
    void loginSpotify();
    void logoutSpotify();
    void refreshSpotify();                          // vuelve a leer listas y álbumes
    void importSpotifyPlaylist (const ExternalPlaylist& playlist);
    void importAllSpotify();
    // Canciones de una lista de Spotify, emparejadas con la biblioteca (file vacío = falta)
    void fetchSpotifyTracks (const juce::String& id, std::function<void (std::vector<LibraryTrack>)> onDone);

    // Client ID (CONFIG > Spotify). Cambiarlo cierra la sesión: cada sesión pertenece a un Client ID.
    juce::String getSpotifyClientId() const          { return spotify.getClientId(); }
    juce::String getSpotifyBuiltInClientId() const   { return SpotifyClient::getBuiltInClientId(); }
    bool isUsingCustomClientId() const               { return spotify.getCustomClientId().isNotEmpty(); }
    void setSpotifyCustomClientId (const juce::String& id);

    std::function<void (const juce::String&)> onStatus;     // mensajes para la barra superior
    std::function<void (bool connected, const juce::String& user, const std::vector<ExternalPlaylist>&)> onSpotifyState;

private:
    struct Result { juce::String playlistName; int total = 0, found = 0, missing = 0; };

    Result importPlaylist (const ImportedPlaylist& playlist);
    void report (const Result& r);
    void showSpotifySetup();
    void startLogin();
    void finishLogin (const juce::String& code);
    void publishSpotifyState();
    const ExternalPlaylist* findSpotifyList (const juce::String& id) const;
    void showSpotifyChooser (std::vector<SpotifyClient::PlaylistInfo> lists);
    void status (const juce::String& text);

    Library& library;
    SpotifyClient spotify;
    juce::ThreadPool pool { 1 };
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    std::atomic<bool> cancel { false };

    std::vector<ExternalPlaylist> spotifyLists;
    juce::String pendingVerifier, pendingState;
    std::unique_ptr<juce::DocumentWindow> loginWindow;
};
