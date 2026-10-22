#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Funciones opcionales de CesarDJ (CONFIG > Funciones). Todas vienen desactivadas:
// cada uno activa las que quiera. Aquí están los nombres de los ajustes y sus valores por defecto.
namespace Features
{
    // Protección anti-desastres
    inline const char* silenceAlert   = "featSilenceAlert";     // bool
    inline const char* silenceSeconds = "featSilenceSeconds";   // 10 / 20 / 30
    inline const char* clipAlert      = "featClipAlert";        // bool
    inline const char* confirmQuit    = "featConfirmQuit";      // bool

    // Después de la sesión
    inline const char* tracklist       = "featTracklist";       // bool
    inline const char* tracklistFormat = "featTracklistFormat"; // 0 YouTube, 1 Mixcloud, 2 texto

    // Calidad de la música
    inline const char* autoGain       = "featAutoGain";         // bool
    inline const char* autoGainLevel  = "featAutoGainLevel";    // 0 suave, 1 normal, 2 club

    // Seguridad
    inline const char* recovery       = "featRecovery";         // 0 desactivada, 1 preguntar, 2 restaurar siempre

    inline bool on (const juce::PropertiesFile& s, const char* key)        { return s.getBoolValue (key, false); }
    inline int  value (const juce::PropertiesFile& s, const char* key, int def) { return s.getIntValue (key, def); }

    // Nivel objetivo del volumen igualado (sonoridad aproximada, dB)
    inline float autoGainTargetDb (int level)       { return level == 0 ? -16.0f : level == 2 ? -10.0f : -13.0f; }
}
