#include "MainComponent.h"
#include "DJLookAndFeel.h"
#include "Branding.h"
#include "Camelot.h"

using namespace DJColours;

juce::PropertiesFile::Options MainComponent::settingsOptions()
{
    Branding::migrateOldData();     // la primera vez, traer los datos de "MiDJ"

    juce::PropertiesFile::Options o;
    o.applicationName = Branding::appName;
    o.filenameSuffix = ".settings";
    o.folderName = Branding::appName;
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

    browser.referenceKey = [this]
    {
        if (deckA.isPlaying() && deckA.getCamelot() >= 0) return deckA.getCamelot();
        if (deckB.isPlaying() && deckB.getCamelot() >= 0) return deckB.getCamelot();
        return deckA.getCamelot() >= 0 ? deckA.getCamelot() : deckB.getCamelot();
    };

    // Stems: mandos del mezclador -> decks; estado de la IA en el mezclador
    mixerView.onStemChange = [this] (int ch, int stem, float v) { (ch == 0 ? deckA : deckB).stemKnob[stem] = v; };
    mixerView.stemStatus = [this] (int ch) -> juce::String
    {
        const auto* t = (ch == 0 ? deckA : deckB).getTrack();
        if (t == nullptr)                       return {};
        if (t->aiProgress.load() >= 0.0f)       return "IA " + juce::String (juce::roundToInt (t->aiProgress.load() * 100.0f)) + "%";
        if (t->hasAiStems())                    return "IA";
        if (t->stems.load() != nullptr)         return juce::String::fromUTF8 ("RÁPIDO");
        if (! stemWorker.isAvailable())         return "SIN MODELO IA";
        return "...";
    };
    mixerView.onModeChanged = [this] (bool stems) { settings.setValue ("mixerStems", stems); };
    mixerView.setStemMode (settings.getBoolValue ("mixerStems", false));
    browser.onPrepareStems = [this] (const juce::Array<juce::File>& files)
    {
        stemWorker.enqueueFiles (files);
        showStatus ("Stems IA: " + juce::String (files.size()) + juce::String::fromUTF8 (" canción(es) en cola"));
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
    masterVolume.setComponentID ("master");
    masterVolume.setWantsKeyboardFocus (false);
    addAndMakeVisible (masterVolume);

    audioButton.setWantsKeyboardFocus (false);
    audioButton.onClick = [this] { showAudioSettings(); };
    audioButton.setTooltip ("Tarjeta de sonido, latencia y salidas (3-4 = auriculares)");
    addAndMakeVisible (audioButton);

    midiButton.setWantsKeyboardFocus (false);
    midiButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb05cf0).darker (0.3f));
    midiButton.onClick = [this] { showMidiMenu(); };
    midiButton.setTooltip ("Controladoras MIDI y modo aprendizaje (MIDI learn)");
    addAndMakeVisible (midiButton);

    recButton.setWantsKeyboardFocus (false);
    recButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc62828));
    recButton.onClick = [this] { toggleRecording(); };
    recButton.setTooltip (juce::String::fromUTF8 ("Graba la sesión (salida master) en WAV de 24 bits, en Música\\CesarDJ Grabaciones"));
    addAndMakeVisible (recButton);

    automixButton.setWantsKeyboardFocus (false);
    automixButton.setColour (juce::TextButton::buttonOnColourId, DJColours::play.darker (0.3f));
    automixButton.setTooltip (juce::String::fromUTF8 ("Mezcla sola la cola de Automix (árbol del navegador). Clic derecho: duración de la transición, mezclar ahora..."));
    automixButton.onClick = [this]
    {
        if (juce::ModifierKeys::getCurrentModifiers().isPopupMenu())
            showAutomixMenu();
        else
            automix.setEnabled (! automix.isEnabled());
    };
    addAndMakeVisible (automixButton);
    automix.setTransitionBeats (settings.getIntValue ("automixBeats", 16));
    automix.onMessage = [this] (const juce::String& msg) { showStatus (msg); };

    deckViewA.onLoadSample = deckViewB.onLoadSample = [this] (int slot, const juce::File& f) { loadSample (slot, f); };
    deckViewA.onSamplerChanged = deckViewB.onSamplerChanged = [this] { saveSamplerSettings(); };
    restoreSamplerSettings();

    learnListener.onDown = [this] (const juce::MouseEvent& e) { learnClick (e); };

    setWantsKeyboardFocus (true);

    // Tamaño inicial: 92 % de la pantalla
    auto screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
    setSize (juce::jmax (1280, screen.getWidth() * 92 / 100), juce::jmax (760, screen.getHeight() * 90 / 100));

    // Audio: 4 salidas si la tarjeta las tiene (1-2 master, 3-4 auriculares); recuerda la última configuración
    auto savedAudio = settings.getXmlValue ("audioDevice");
    setAudioChannels (0, 4, savedAudio.get());

    midi = std::make_unique<MidiController> (deviceManager, settings, *this);
    midi->onLearned = [this] (const juce::String& id, const juce::String& desc)
    {
        showStatus (juce::String::fromUTF8 ("Asignado: ") + id + "  <-  " + desc);
        repaint();
    };

    startTimerHz (10);
}

MainComponent::~MainComponent()
{
    automix.setEnabled (false);
    settings.setValue ("automixBeats", automix.getTransitionBeats());
    setLearning (false);
    midi = nullptr;
    recorder.stop();

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
        library.setAnalysis (t.file, t.bpm, t.firstBeat, t.getLength(), Camelot::toString (t.camelot.load()));
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

    // Cuando la canción está lista, la IA prepara sus stems de calidad (si no estaban en caché)
    deck.onTrackChanged = [this, &deck]
    {
        if (! deck.isLoading())
            if (auto t = deck.getTrackShared(); t != nullptr && t->stems.load() != nullptr && ! t->hasAiStems())
                stemWorker.enqueue (t);
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
        info.camelot = Camelot::parse (t->key);     // la de la etiqueta vale si es legible
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

// Atajos:  A -> Q cue, W play, 1-4 hot cues      B -> O cue, P play, 7-0 hot cues
//          Ctrl+F buscar      Ctrl+M MIDI learn      Esc salir del MIDI learn
namespace
{
    // Ctrl + letra (con Ctrl pulsado Windows no envía el carácter normal, así que se mira el código de tecla)
    bool ctrlKey (const juce::KeyPress& key, char letter)
    {
        const int code = key.getKeyCode();
        return key.getModifiers().isCtrlDown() && ! key.getModifiers().isAltDown()
               && (code == letter || code == letter + 32 || code == letter - 'A' + 1);
    }
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && midi != nullptr && midi->isLearning())
    {
        setLearning (false);
        return true;
    }

    if (ctrlKey (key, 'M') && midi != nullptr)
    {
        setLearning (! midi->isLearning());
        return true;
    }

    if (ctrlKey (key, 'F'))
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

//==============================================================================
void MainComponent::timerCallback()
{
    // Botón REC con el tiempo de grabación
    const bool recording = recorder.isRecording();
    recButton.setToggleState (recording, juce::dontSendNotification);
    if (recording)
    {
        const int s = (int) recorder.getSeconds();
        recButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f ") + juce::String (s / 60).paddedLeft ('0', 2)
                                 + ":" + juce::String (s % 60).paddedLeft ('0', 2));
    }
    else
    {
        recButton.setButtonText ("REC");
    }

    midiButton.setToggleState (midi != nullptr && midi->isLearning(), juce::dontSendNotification);
    automixButton.setToggleState (automix.isEnabled(), juce::dontSendNotification);

    if (midi != nullptr && midi->isLearning())
        repaint();                       // resalte del control elegido
    else
        repaint (getLocalBounds().removeFromTop (40));
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    if (logoArea.contains (e.getPosition()))
        Branding::showAbout();
}

void MainComponent::showStatus (const juce::String& message)
{
    statusMessage = message;
    statusMessageUntil = juce::Time::getMillisecondCounter() + 3500;
}

void MainComponent::showAutomixMenu()
{
    juce::PopupMenu m;
    m.addItem (automix.isEnabled() ? "Detener Automix" : "Iniciar Automix", [this] { automix.setEnabled (! automix.isEnabled()); });
    m.addItem ("Mezclar ahora con la siguiente", automix.isEnabled(), false, [this] { automix.mixNow(); });
    m.addSeparator();

    juce::PopupMenu lengths;
    for (int beats : { 4, 8, 16, 32 })
        lengths.addItem (juce::String (beats) + " beats", true, automix.getTransitionBeats() == beats,
                         [this, beats] { automix.setTransitionBeats (beats); settings.setValue ("automixBeats", beats); });
    m.addSubMenu (juce::String::fromUTF8 ("Duración de la transición"), lengths);
    m.addSeparator();
    m.addItem ("Vaciar la cola de Automix", [this] { library.clearAutomix(); });

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (automixButton));
}

void MainComponent::toggleRecording()
{
    if (recorder.isRecording())
    {
        const auto file = recorder.getFile();
        const double seconds = recorder.getSeconds();
        recorder.stop();

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::InfoIcon, juce::String::fromUTF8 ("Grabación guardada"),
            file.getFileName() + "\n" + juce::String (seconds / 60.0, 1) + " min  -  " + juce::File::descriptionOfSizeInBytes (file.getSize()),
            "Abrir carpeta", "Cerrar", nullptr,
            juce::ModalCallbackFunction::create ([file] (int r) { if (r == 1) file.revealToUser(); }));
        return;
    }

    auto* device = deviceManager.getCurrentAudioDevice();
    const auto name = Branding::appName + " " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav";
    const auto file = Recorder::defaultFolder().getChildFile (name);

    if (device == nullptr || ! recorder.start (file, device->getCurrentSampleRate()))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No se pudo grabar",
                                                "No se pudo crear el archivo:\n" + file.getFullPathName());
    else
        showStatus (juce::String::fromUTF8 ("Grabando en ") + file.getFullPathName());
}

//==============================================================================
void MainComponent::showMidiMenu()
{
    juce::PopupMenu m;
    const bool learning = midi->isLearning();
    m.addItem ("Modo aprendizaje (MIDI learn)", true, learning, [this, learning] { setLearning (! learning); });
    m.addSeparator();

    m.addSectionHeader ("Controladoras conectadas");
    const auto names = midi->getDeviceNames();
    if (names.isEmpty())
        m.addItem ("(ninguna)", false, false, nullptr);
    for (auto& n : names)
        m.addItem (n, false, false, nullptr);

    m.addSeparator();
    if (auto* target = midi->getLearnTarget())
    {
        const auto id = target->getComponentID();
        m.addItem (juce::String::fromUTF8 ("Borrar la asignación de ") + id, midi->isMapped (id), false,
                   [this, id] { midi->clearMappingsFor (id); repaint(); });
    }
    m.addItem ("Borrar todas las asignaciones (" + juce::String (midi->getNumMappings()) + ")", midi->getNumMappings() > 0, false, [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Borrar asignaciones MIDI",
            juce::String::fromUTF8 ("Se borrarán todas las asignaciones de la controladora."), "Borrar", "Cancelar", nullptr,
            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) { midi->clearMappings(); repaint(); } }));
    });

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (midiButton));
}

void MainComponent::setLearning (bool shouldLearn)
{
    if (midi == nullptr || midi->isLearning() == shouldLearn)
        return;

    midi->setLearning (shouldLearn);

    if (shouldLearn)
    {
        juce::Desktop::getInstance().addGlobalMouseListener (&learnListener);
        showStatus (juce::String::fromUTF8 ("MIDI LEARN: haz clic en un control y después muévelo o púlsalo en tu controladora.  Esc para salir"));
        statusMessageUntil = 0xffffffff;
    }
    else
    {
        juce::Desktop::getInstance().removeGlobalMouseListener (&learnListener);
        statusMessage.clear();
    }

    repaint();
}

// En modo aprendizaje, el control que se pulsa pasa a ser el destino de la asignación
void MainComponent::learnClick (const juce::MouseEvent& e)
{
    auto* c = e.originalComponent;
    if (c == nullptr || (c != this && ! isParentOf (c)))
        return;

    while (c != nullptr && c != this && c->getComponentID().isEmpty())
        c = c->getParentComponent();

    if (c != nullptr && c != this)
    {
        midi->setLearnTarget (c);
        showStatus (juce::String::fromUTF8 ("Elegido: ") + c->getComponentID()
                    + juce::String::fromUTF8 (" -> ahora mueve o pulsa el control de tu controladora.  Esc para salir"));
        statusMessageUntil = 0xffffffff;
        repaint();
    }
}

void MainComponent::paintOverChildren (juce::Graphics& g)
{
    if (midi == nullptr || ! midi->isLearning())
        return;

    // Atenuar todo y marcar: verde = ya asignado, naranja = control elegido
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (getLocalBounds().withTrimmedTop (40));

    std::function<void (juce::Component&)> mark = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isShowing())
                continue;
            const auto id = child->getComponentID();
            if (id.isNotEmpty() && midi->isMapped (id))
            {
                g.setColour (juce::Colours::limegreen.withAlpha (0.8f));
                g.drawRect (getLocalArea (child, child->getLocalBounds()), 2);
            }
            mark (*child);
        }
    };
    mark (*this);

    if (auto* target = midi->getLearnTarget())
    {
        const bool blink = (juce::Time::getMillisecondCounter() / 300) % 2 == 0;
        g.setColour (juce::Colour (0xffe0912a).withAlpha (blink ? 1.0f : 0.5f));
        g.drawRect (getLocalArea (target, target->getLocalBounds()).expanded (2), 3);
    }
}

//==============================================================================
void MainComponent::loadSample (int slot, const juce::File& file)
{
    if (sampler.loadFile (slot, file, formatManager))
    {
        saveSamplerSettings();
        showStatus ("Sampler " + juce::String (slot + 1) + ": " + file.getFileName());
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No se pudo cargar el sonido",
                                                "Formato no soportado:\n" + file.getFileName());
    }
}

void MainComponent::saveSamplerSettings()
{
    juce::XmlElement xml ("SAMPLER");
    for (int i = 0; i < SamplerEngine::numSlots; ++i)
    {
        const auto& info = sampler.getInfo (i);
        auto* e = xml.createNewChildElement ("SLOT");
        e->setAttribute ("index", i);
        e->setAttribute ("file", info.file.getFullPathName());
        e->setAttribute ("loop", info.loop);
    }
    settings.setValue ("sampler", &xml);
    settings.saveIfNeeded();
}

void MainComponent::restoreSamplerSettings()
{
    if (auto xml = settings.getXmlValue ("sampler"))
        for (auto* e : xml->getChildIterator())
        {
            const int i = e->getIntAttribute ("index", -1);
            if (i < 0 || i >= SamplerEngine::numSlots)
                continue;

            const juce::File f (e->getStringAttribute ("file"));
            if (f.existsAsFile())
                sampler.loadFile (i, f, formatManager);
            sampler.setLoop (i, e->getBoolAttribute ("loop"));
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

    // Logo: Vespa + nombre (clic = Acerca de)
    {
        auto logo = logoArea;
        if (logoMark != nullptr)
            logoMark->drawWithin (g, logo.removeFromLeft (38).toFloat().reduced (0.0f, 4.0f),
                                  juce::RectanglePlacement::centred, 1.0f);
        logo.removeFromLeft (8);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText (Branding::appName, logo, juce::Justification::centredLeft);
    }

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

    if (statusMessage.isNotEmpty() && juce::Time::getMillisecondCounter() < statusMessageUntil)
    {
        g.setColour (midi != nullptr && midi->isLearning() ? juce::Colour (0xffe0912a) : DJColours::text);
        g.drawText (statusMessage, statusArea, juce::Justification::centred, true);
    }
    else if (automix.isEnabled())
    {
        g.setColour (DJColours::play);
        g.drawText (automix.getStatus(), statusArea, juce::Justification::centred, true);
    }
    else if (const auto stemStatus = stemWorker.getStatusText(); stemStatus.isNotEmpty())
    {
        g.setColour (juce::Colour (0xffb05cf0).brighter (0.3f));
        g.drawText (stemStatus, statusArea, juce::Justification::centred, true);
    }
    else
    {
        g.drawText (status, statusArea, juce::Justification::centred);
    }

    // Luz de actividad MIDI
    const bool activity = midi != nullptr && juce::Time::getMillisecondCounter() - midi->getLastActivity() < 150;
    g.setColour (activity ? juce::Colours::limegreen : juce::Colours::white.withAlpha (0.12f));
    g.fillEllipse (midiLedArea.toFloat());

    g.drawText ("MASTER", masterLabelArea, juce::Justification::centredRight);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    auto top = area.removeFromTop (40).reduced (10, 6);
    audioButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (6);
    midiButton.setBounds (top.removeFromRight (64));
    midiLedArea = top.removeFromRight (14).withSizeKeepingCentre (8, 8);
    top.removeFromRight (6);
    recButton.setBounds (top.removeFromRight (84));
    top.removeFromRight (6);
    automixButton.setBounds (top.removeFromRight (96));
    top.removeFromRight (14);
    masterVolume.setBounds (top.removeFromRight (180));
    masterLabelArea = top.removeFromRight (60);
    logoArea = top.removeFromLeft (150);
    statusArea = top.withTrimmedLeft (20);

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
