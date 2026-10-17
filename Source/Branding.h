#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Identidad de CesarDJ: nombre, logo (Vespa), pantalla de bienvenida y carpeta de datos
namespace Branding
{
    const juce::String appName = "CesarDJ";

    juce::String version();                                         // "0.6.0"
    std::unique_ptr<juce::Drawable> createLogo();                   // icono completo (fondo negro)
    std::unique_ptr<juce::Drawable> createMark (juce::Colour colour);   // solo la Vespa, del color indicado
    juce::Image createSplashImage();

    // %APPDATA%\CesarDJ: biblioteca, configuración... (no se toca al actualizar el programa)
    juce::File dataFolder();

    // Primera vez como CesarDJ: copia los datos de la época "MiDJ" (biblioteca, ajustes, MIDI...)
    void migrateOldData();

    void showAbout();
}
