#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "MixerEngine.h"
#include "MidiController.h"
#include "PlaylistImport.h"

// Enrutado de salidas (master / auriculares): se guarda en la configuración y se aplica al arrancar
namespace AudioRouting
{
    void load (MixerEngine& mixer, juce::PropertiesFile& settings);
    void save (const MixerEngine& mixer, juce::PropertiesFile& settings);
    // Abre en la tarjeta las salidas que hacen falta para el enrutado elegido
    void applyChannels (juce::AudioDeviceManager& devices, const MixerEngine& mixer);
}

// Ventana de Configuración (como la de VirtualDJ):
//  - Audio: tarjeta / controladora, frecuencia, búfer, a qué salidas van master y auriculares
//  - Controladoras: las conectadas, activar/desactivar, MIDI learn, asignaciones y perfiles
//  - Spotify: sesión y Client ID propio (la app de Spotify de una cuenta Premium)
//  - Funciones: funciones opcionales (todas desactivadas por defecto)
class SettingsWindow : public juce::DocumentWindow
{
public:
    enum Tab { audioTab = 0, controllersTab = 1, spotifyTab = 2, featuresTab = 3 };

    SettingsWindow (juce::AudioDeviceManager& devices, MixerEngine& mixer, MidiController& midi, PlaylistImporter& importer,
                    juce::PropertiesFile& settings, std::function<void (bool)> setLearning, std::function<bool()> isLearning);

    void showTab (Tab tab);
    void closeButtonPressed() override;

    std::function<void()> onClose;

private:
    juce::TabbedComponent* tabs = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};
