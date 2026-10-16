#include "MainComponent.h"
#include "DJLookAndFeel.h"

using namespace DJColours;

juce::PropertiesFile::Options MainComponent::settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "MiDJ";
    o.filenameSuffix = ".settings";
    o.folderName = "MiDJ";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

MainComponent::MainComponent()
{
    addAndMakeVisible (waveforms);
    addAndMakeVisible (deckViewA);
    addAndMakeVisible (mixerView);
    addAndMakeVisible (deckViewB);
    addAndMakeVisible (browser);

    browser.onLoadRequest = [this] (const juce::File& f, int deckIndex) { loadFromBrowser (f, deckIndex); };
    browser.loadedFile = [this] (int d) -> juce::File
    {
        const auto* t = (d == 0 ? deckA : deckB).getTrack();
        return t != nullptr ? t->file : juce::File();
    };
    browser.referenceBpm = [this]
    {
        // El deck que suena manda; si no suena ninguno, el A
        if (deckA.isPlaying() && deckA.getEffectiveBpm() > 0.0) return deckA.getEffectiveBpm();
        if (deckB.isPlaying() && deckB.getEffectiveBpm() > 0.0) return deckB.getEffectiveBpm();
        return deckA.getEffectiveBpm() > 0.0 ? deckA.getEffectiveBpm() : deckB.getEffectiveBpm();
    };

    connectDeck (0, deckA, deckViewA);
    connectDeck (1, deckB, deckViewB);

    masterVolume.setRange (0.0, 1.0);
    masterVolume.setValue (settings.getDoubleValue ("masterVolume", 1.0), juce::dontSendNotification);
    mixer.masterVolume = (float) masterVolume.getValue();
    masterVolume.setDoubleClickReturnValue (true, 1.0);
    masterVolume.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    masterVolume.onValueChange = [this] { mixer.masterVolume = (float) masterVolume.getValue(); };
    masterVolume.setTooltip ("Volumen master");
    masterVolume.setWantsKeyboardFocus (false);
    addAndMakeVisible (masterVolume);

    audioButton.setWantsKeyboardFocus (false);
    audioButton.onClick = [this] { showAudioSettings(); };
    audioButton.setTooltip ("Tarjeta de sonido, latencia y salidas (3-4 = auriculares)");
    addAndMakeVisible (audioButton);

    setWantsKeyboardFocus (true);

    // Tamaño inicial: 92 % de la pantalla
    auto screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
    setSize (juce::jmax (1280, screen.getWidth() * 92 / 100), juce::jmax (760, screen.getHeight() * 90 / 100));

    // Audio: 4 salidas si la tarjeta las tiene (1-2 master, 3-4 auriculares); recuerda la última configuración
    auto savedAudio = settings.getXmlValue ("audioDevice");
    setAudioChannels (0, 4, savedAudio.get());

    startTimerHz (2);
}

MainComponent::~MainComponent()
{
    if (auto state = deviceManager.createStateXml())
        settings.setValue ("audioDevice", state.get());
    settings.setValue ("masterVolume", masterVolume.getValue());
    settings.saveIfNeeded();

    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    mixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    mixer.getNextAudioBlock (bufferToFill);
}

void MainComponent::releaseResources()
{
    mixer.releaseResources();
}

// Cada deck guarda en la biblioteca lo que aprende de la canción y recupera lo ya guardado
void MainComponent::connectDeck (int index, DeckEngine& deck, DeckComponent& view)
{
    view.infoForFile = [this] (const juce::File& f) { return infoFor (f); };

    deck.onAnalysed = [this] (const Track& t)
    {
        library.setAnalysis (t.file, t.bpm, t.firstBeat, t.getLength());
    };

    deck.onCuesChanged = [this, &deck]
    {
        if (const auto* t = deck.getTrack())
        {
            std::array<double, 4> hc;
            for (int i = 0; i < 4; ++i)
                hc[(size_t) i] = deck.getHotCue (i);
            library.setCues (t->file, deck.getCuePoint(), hc);
        }
    };

    deck.onStartedPlaying = [this, &deck, index]
    {
        if (const auto* t = deck.getTrack())
        {
            library.markPlayed (t->file, index);
            browser.markPlayedThisSession (t->file);
        }
    };
}

DeckEngine::TrackInfo MainComponent::infoFor (const juce::File& file)
{
    DeckEngine::TrackInfo info;

    if (auto t = library.findByFile (file))
    {
        info.hasAnalysis = t->analysed;
        info.bpm = t->bpm;
        info.firstBeat = t->firstBeat;
        info.cuePoint = t->cuePoint;
        info.hotCues = t->hotCues;
    }

    return info;
}

void MainComponent::loadFromBrowser (const juce::File& file, int deckIndex)
{
    if (deckIndex < 0)
    {
        if (! deckA.isPlaying())        deckIndex = 0;
        else if (! deckB.isPlaying())   deckIndex = 1;
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, juce::String::fromUTF8 ("Los dos decks están sonando"),
                                                    juce::String::fromUTF8 ("Pausa uno de los decks para cargar la canción."));
            return;
        }
    }

    (deckIndex == 0 ? deckViewA : deckViewB).loadFile (file);
}

void MainComponent::loadFiles (const juce::String& commandLine)
{
    auto args = juce::StringArray::fromTokens (commandLine, true);
    int deckIndex = 0;

    for (auto arg : args)
    {
        const juce::File f (arg.unquoted());
        if (f.existsAsFile() && deckIndex < 2)
            (deckIndex++ == 0 ? deckViewA : deckViewB).loadFile (f);
    }
}

// Atajos:  A -> Q cue, W play, 1-4 hot cues      B -> O cue, P play, 7-0 hot cues      Ctrl+F buscar
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('f', juce::ModifierKeys::ctrlModifier, 0))
    {
        browser.focusSearch();
        return true;
    }

    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    switch (c)
    {
        case 'q': deckA.cue();          return true;
        case 'w': deckA.togglePlay();   return true;
        case 'o': deckB.cue();          return true;
        case 'p': deckB.togglePlay();   return true;
        case '1': case '2': case '3': case '4':
            deckA.triggerHotCue ((int) (c - '1'));
            return true;
        case '7': case '8': case '9': case '0':
            deckB.triggerHotCue (c == '0' ? 3 : (int) (c - '7'));
            return true;
        default:
            return false;
    }
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 4, false, false, true, false);
    selector->setSize (560, 460);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = juce::String::fromUTF8 ("Configuración de audio");
    options.dialogBackgroundColour = panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // Barra superior
    auto top = getLocalBounds().removeFromTop (40);
    g.setColour (juce::Colour (0xff1c1e23));
    g.fillRect (top);
    g.setColour (juce::Colours::black);
    g.fillRect (top.removeFromBottom (1));

    // Logo
    auto logo = top.reduced (12, 7).removeFromLeft (92);
    g.setColour (DJColours::deckB);
    g.fillRoundedRectangle (logo.toFloat(), 3.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("MiDJ", logo, juce::Justification::centred);

    // Estado: CPU y hora
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    auto* device = deviceManager.getCurrentAudioDevice();
    juce::String status = "CPU " + juce::String (juce::roundToInt (deviceManager.getCpuUsage() * 100.0)) + "%";
    if (device != nullptr)
        status += "   |   " + juce::String (device->getCurrentSampleRate() / 1000.0, 1) + " kHz   |   latencia "
                + juce::String (juce::roundToInt (device->getOutputLatencyInSamples() * 1000.0 / device->getCurrentSampleRate()))
                + " ms";
    status += "   |   " + juce::Time::getCurrentTime().formatted ("%H:%M");
    g.drawText (status, statusArea, juce::Justification::centred);

    g.drawText ("MASTER", masterLabelArea, juce::Justification::centredRight);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    auto top = area.removeFromTop (40).reduced (10, 6);
    audioButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (14);
    masterVolume.setBounds (top.removeFromRight (180));
    masterLabelArea = top.removeFromRight (60);
    statusArea = top.withTrimmedLeft (110);

    waveforms.setBounds (area.removeFromTop (juce::jlimit (110, 190, area.getHeight() / 6)));
    area.removeFromTop (2);

    auto row = area.removeFromTop (juce::jlimit (360, 430, area.getHeight() * 56 / 100));
    const int mixerW = 380;
    const int deckW = (row.getWidth() - mixerW - 4) / 2;
    deckViewA.setBounds (row.removeFromLeft (deckW));
    row.removeFromLeft (2);
    deckViewB.setBounds (row.removeFromRight (deckW));
    row.removeFromRight (2);
    mixerView.setBounds (row);

    area.removeFromTop (2);
    browser.setBounds (area);
}
