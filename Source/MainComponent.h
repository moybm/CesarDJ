#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "DeckEngine.h"
#include "DeckComponent.h"
#include "MixerEngine.h"
#include "MixerComponent.h"
#include "ScrollingWaveforms.h"
#include "BrowserComponent.h"
#include "Library.h"
#include "SamplerEngine.h"
#include "Recorder.h"
#include "MidiController.h"
#include "Branding.h"
#include "Automix.h"
#include "StemSeparator.h"

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
    void paintOverChildren (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Archivos pasados al abrir MiDJ (línea de comandos / "Abrir con"): el 1º al deck A, el 2º al B
    void loadFiles (const juce::String& commandLine);

private:
    void timerCallback() override;
    void showAudioSettings();
    void loadFromBrowser (const juce::File& file, int deckIndex);
    void connectDeck (int index, DeckEngine& deck, DeckComponent& view);
    DeckEngine::TrackInfo infoFor (const juce::File& file);

    void toggleRecording();
    void showAutomixMenu();
    void showMidiMenu();
    void setLearning (bool shouldLearn);
    void learnClick (const juce::MouseEvent& e);
    void showStatus (const juce::String& message);
    void loadSample (int slot, const juce::File& file);
    void saveSamplerSettings();
    void restoreSamplerSettings();

    // Recibe todos los clics de la ventana mientras dura el MIDI learn
    struct ClickListener : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onDown;
        void mouseDown (const juce::MouseEvent& e) override     { if (onDown != nullptr) onDown (e); }
    };
    static juce::PropertiesFile::Options settingsOptions();

    juce::PropertiesFile settings { settingsOptions() };

    juce::AudioFormatManager formatManager;
    const bool formatsReady = [this] { formatManager.registerBasicFormats(); return true; }();

    StemWorker stemWorker { formatManager };
    Library library { settings.getFile().getSiblingFile ("library.db"), formatManager.getWildcardForAllFormats() };

    DeckEngine deckA { formatManager }, deckB { formatManager };
    SamplerEngine sampler;
    Recorder recorder;
    MixerEngine mixer { deckA, deckB, sampler, recorder };

    ScrollingWaveforms waveforms { deckA, deckB };
    DeckComponent deckViewA { 0, deckA, deckB, sampler };
    DeckComponent deckViewB { 1, deckB, deckA, sampler };
    MixerComponent mixerView { mixer, sampler };
    BrowserComponent browser { library, settings };

    juce::Slider masterVolume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton audioButton { "AUDIO" }, midiButton { "MIDI" }, recButton { "REC" }, automixButton { "AUTOMIX" };
    AutomixController automix { library, { &deckA, &deckB }, { &deckViewA, &deckViewB }, mixerView };
    std::unique_ptr<MidiController> midi;
    ClickListener learnListener;
    juce::String statusMessage;
    juce::uint32 statusMessageUntil = 0;
    juce::Rectangle<int> midiLedArea, logoArea;
    std::unique_ptr<juce::Drawable> logoMark = Branding::createMark (juce::Colours::white);
    juce::TooltipWindow tooltips { this, 700 };
    juce::Rectangle<int> statusArea, masterLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
