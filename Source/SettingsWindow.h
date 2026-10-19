#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "MixerEngine.h"
#include "MidiController.h"

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
class SettingsWindow : public juce::DocumentWindow
{
public:
    enum Tab { audioTab = 0, controllersTab = 1 };

    SettingsWindow (juce::AudioDeviceManager& devices, MixerEngine& mixer, MidiController& midi,
                    juce::PropertiesFile& settings, std::function<void (bool)> setLearning, std::function<bool()> isLearning);

    void showTab (Tab tab);
    void closeButtonPressed() override;

    std::function<void()> onClose;

private:
    juce::TabbedComponent* tabs = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};
