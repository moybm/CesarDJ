#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "DeckEngine.h"
#include "DeckComponent.h"
#include "MixerEngine.h"
#include "MixerComponent.h"
#include "ScrollingWaveforms.h"
#include "BrowserComponent.h"
#include "Library.h"

// Ventana principal: posee el audio, los dos decks, el mezclador y el navegador.
class MainComponent : public juce::AudioAppComponent,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Archivos pasados al abrir MiDJ (línea de comandos / "Abrir con"): el 1º al deck A, el 2º al B
    void loadFiles (const juce::String& commandLine);

private:
    void timerCallback() override       { repaint (statusArea); }
    void showAudioSettings();
    void loadFromBrowser (const juce::File& file, int deckIndex);
    void connectDeck (int index, DeckEngine& deck, DeckComponent& view);
    DeckEngine::TrackInfo infoFor (const juce::File& file);
    static juce::PropertiesFile::Options settingsOptions();

    juce::PropertiesFile settings { settingsOptions() };

    juce::AudioFormatManager formatManager;
    const bool formatsReady = [this] { formatManager.registerBasicFormats(); return true; }();

    Library library { settings.getFile().getSiblingFile ("library.db"), formatManager.getWildcardForAllFormats() };

    DeckEngine deckA { formatManager }, deckB { formatManager };
    MixerEngine mixer { deckA, deckB };

    ScrollingWaveforms waveforms { deckA, deckB };
    DeckComponent deckViewA { 0, deckA, deckB };
    DeckComponent deckViewB { 1, deckB, deckA };
    MixerComponent mixerView { mixer };
    BrowserComponent browser { library, settings };

    juce::Slider masterVolume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton audioButton { "AUDIO" };
    juce::TooltipWindow tooltips { this, 700 };
    juce::Rectangle<int> statusArea, masterLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
