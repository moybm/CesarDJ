#include "SettingsWindow.h"
#include "DJLookAndFeel.h"
#include "Help.h"

using namespace DJColours;

//==============================================================================
namespace AudioRouting
{
    void load (MixerEngine& mixer, juce::PropertiesFile& settings)
    {
        mixer.masterOut = settings.getIntValue ("masterOut", 0);
        mixer.phonesOut = settings.getIntValue ("phonesOut", 2);
        mixer.splitMono = settings.getBoolValue ("splitMono", false);
        mixer.phonesVolume = (float) settings.getDoubleValue ("phonesVolume", 1.0);
        mixer.cueMix = (float) settings.getDoubleValue ("cueMix", 0.0);
    }

    void save (const MixerEngine& mixer, juce::PropertiesFile& settings)
    {
        settings.setValue ("masterOut", mixer.masterOut.load());
        settings.setValue ("phonesOut", mixer.phonesOut.load());
        settings.setValue ("splitMono", mixer.splitMono.load());
        settings.setValue ("phonesVolume", (double) mixer.phonesVolume.load());
        settings.setValue ("cueMix", (double) mixer.cueMix.load());
        settings.saveIfNeeded();
    }

    void applyChannels (juce::AudioDeviceManager& devices, const MixerEngine& mixer)
    {
        auto* device = devices.getCurrentAudioDevice();
        if (device == nullptr)
            return;

        const int available = device->getOutputChannelNames().size();
        int needed = mixer.masterOut.load() + 2;
        if (! mixer.splitMono.load() && mixer.phonesOut.load() >= 0)
            needed = juce::jmax (needed, mixer.phonesOut.load() + 2);
        needed = juce::jlimit (1, juce::jmax (1, available), needed);

        // Se abren las salidas 1..N seguidas: así el índice del buffer es el número de salida
        auto setup = devices.getAudioDeviceSetup();
        juce::BigInteger wanted;
        wanted.setRange (0, needed, true);
        if (setup.outputChannels == wanted && ! setup.useDefaultOutputChannels)
            return;

        setup.outputChannels = wanted;
        setup.useDefaultOutputChannels = false;
        devices.setAudioDeviceSetup (setup, true);
    }
}

namespace
{
    juce::Label* makeLabel (juce::Component& owner, juce::OwnedArray<juce::Label>& store, const juce::String& caption, bool header = false)
    {
        auto* l = store.add (new juce::Label ({}, caption));
        l->setColour (juce::Label::textColourId, header ? juce::Colours::white : textDim);
        l->setFont (header ? juce::FontOptions (13.0f, juce::Font::bold) : juce::FontOptions (13.0f));
        owner.addAndMakeVisible (l);
        return l;
    }

    juce::String pairName (const juce::StringArray& names, int first)
    {
        juce::String s = "Salidas " + juce::String (first + 1) + "-" + juce::String (first + 2);
        const auto a = names[first].trim(), b = names[first + 1].trim();
        if (a.isNotEmpty() && ! a.startsWithIgnoreCase ("output"))
            s << "   (" << a << (b.isNotEmpty() ? " / " + b : juce::String()) << ")";
        return s;
    }
}

//==============================================================================
// Pestaña Audio
class AudioPage : public juce::Component,
                  private juce::ChangeListener
{
public:
    AudioPage (juce::AudioDeviceManager& dm, MixerEngine& m, juce::PropertiesFile& s)
        : devices (dm), mixer (m), settings (s)
    {
        makeLabel (*this, labels, juce::String::fromUTF8 ("CONFIGURACIÓN RÁPIDA"), true);
        presetSpeakers.setButtonText ("Altavoces del PC");
        presetCard.setButtonText ("Controladora o tarjeta con auriculares");
        presetSplit.setButtonText ("PC + auriculares (cable divisor)");
        presetSpeakers.setTooltip (juce::String::fromUTF8 ("Todo por la salida normal del ordenador. Sin pre-escucha."));
        presetCard.setTooltip (juce::String::fromUTF8 ("Para controladoras DJ con tarjeta de sonido (Pioneer, Hercules, Numark...) o tarjetas "
                                                       "de 4 salidas: salidas 1-2 = master, 3-4 = auriculares."));
        presetSplit.setTooltip (juce::String::fromUTF8 ("Sin tarjeta externa: con un cable divisor en Y, el lado izquierdo es el master (mono) "
                                                        "y el derecho los auriculares (mono)."));
        presetSpeakers.onClick = [this] { setRouting (0, -1, false); };
        presetCard.onClick = [this]
        {
            if (numOutputs() < 4)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Auriculares",
                    juce::String::fromUTF8 ("El dispositivo elegido solo tiene ") + juce::String (numOutputs())
                    + juce::String::fromUTF8 (" salidas.\n\nConecta la controladora o la tarjeta de sonido y elígela arriba en \"Dispositivo\" "
                                              "(debe tener 4 salidas o más)."));
                return;
            }
            setRouting (0, 2, false);
            Help::notice ("audioController", "Controladora con tarjeta de sonido",
                juce::String::fromUTF8 ("Listo: salidas 1-2 = master (altavoces) y 3-4 = auriculares. Pulsa \"Probar\" para comprobar cada una.\n\n"
                    "A tener en cuenta:\n"
                    "- CesarDJ usa el audio de Windows, no ASIO. Para menos retardo elige \"Low Latency Mode\" o \"Exclusive Mode\".\n"
                    "- Si tu controladora aparece en Windows como dos dispositivos separados (master y auriculares), "
                    "CesarDJ solo puede abrir uno: usa el que tenga 4 salidas."), "audio");
        };
        presetSplit.onClick = [this] { setRouting (0, -1, true); };
        for (auto* b : { &presetSpeakers, &presetCard, &presetSplit })
        {
            b->setColour (juce::TextButton::buttonColourId, panelLight);
            addAndMakeVisible (b);
        }

        makeLabel (*this, labels, juce::String::fromUTF8 ("TARJETA DE SONIDO"), true);
        typeLabel = makeLabel (*this, labels, "Controlador");
        deviceLabel = makeLabel (*this, labels, "Dispositivo");
        rateLabel = makeLabel (*this, labels, "Frecuencia");
        bufferLabel = makeLabel (*this, labels, juce::String::fromUTF8 ("Búfer (latencia)"));
        for (auto* c : { &typeBox, &deviceBox, &rateBox, &bufferBox, &masterBox, &phonesBox })
            addAndMakeVisible (c);

        typeBox.onChange = [this]
        {
            if (updating) return;
            devices.setCurrentAudioDeviceType (typeBox.getText(), true);
            afterDeviceChange();
        };
        deviceBox.onChange = [this]
        {
            if (updating) return;
            auto setup = devices.getAudioDeviceSetup();
            setup.outputDeviceName = deviceBox.getText();
            setup.inputDeviceName = {};
            setup.useDefaultOutputChannels = true;
            report (devices.setAudioDeviceSetup (setup, true));
            afterDeviceChange();
        };
        rateBox.onChange = [this]
        {
            if (updating) return;
            auto setup = devices.getAudioDeviceSetup();
            setup.sampleRate = rateBox.getSelectedId();
            report (devices.setAudioDeviceSetup (setup, true));
        };
        bufferBox.onChange = [this]
        {
            if (updating) return;
            auto setup = devices.getAudioDeviceSetup();
            setup.bufferSize = bufferBox.getSelectedId();
            report (devices.setAudioDeviceSetup (setup, true));
        };

        controlPanel.setButtonText ("Panel del driver...");
        controlPanel.onClick = [this]
        {
            if (auto* d = devices.getCurrentAudioDevice())
                if (d->hasControlPanel() && d->showControlPanel())
                    devices.restartLastAudioDevice();
        };
        addAndMakeVisible (controlPanel);

        makeLabel (*this, labels, "SALIDAS", true);
        masterLabel = makeLabel (*this, labels, "Master");
        phonesLabel = makeLabel (*this, labels, "Auriculares");
        masterBox.onChange = [this]
        {
            if (updating || masterBox.getSelectedId() < 100) return;
            setRouting (masterBox.getSelectedId() - 100, mixer.phonesOut.load(), false);
        };
        phonesBox.onChange = [this]
        {
            if (updating) return;
            const int id = phonesBox.getSelectedId();
            if (id == 1)        setRouting (mixer.masterOut.load(), -1, false);
            else if (id == 2)   setRouting (mixer.masterOut.load(), -1, true);
            else if (id >= 100) setRouting (mixer.masterOut.load(), id - 100, false);
        };

        testMaster.setButtonText ("Probar master");
        testPhones.setButtonText ("Probar auriculares");
        testMaster.setTooltip (juce::String::fromUTF8 ("Suena un pitido de 1,5 s por la salida del master"));
        testPhones.setTooltip (juce::String::fromUTF8 ("Suena un pitido de 1,5 s por los auriculares"));
        testMaster.onClick = [this] { mixer.testTone = 1; };
        testPhones.onClick = [this] { mixer.testTone = 2; };
        addAndMakeVisible (testMaster);
        addAndMakeVisible (testPhones);

        makeLabel (*this, labels, "AURICULARES", true);
        phonesVolLabel = makeLabel (*this, labels, "Volumen");
        cueMixLabel = makeLabel (*this, labels, juce::String::fromUTF8 ("Mezcla CUE ↔ MASTER"));
        for (auto* sl : { &phonesVolume, &cueMix })
        {
            sl->setSliderStyle (juce::Slider::LinearHorizontal);
            sl->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sl->setRange (0.0, 1.0);
            addAndMakeVisible (sl);
        }
        phonesVolume.setValue (mixer.phonesVolume.load(), juce::dontSendNotification);
        phonesVolume.setDoubleClickReturnValue (true, 1.0);
        cueMix.setValue (mixer.cueMix.load(), juce::dontSendNotification);
        cueMix.setDoubleClickReturnValue (true, 0.0);
        cueMix.setTooltip (juce::String::fromUTF8 ("Izquierda: solo lo que tienes en pre-escucha (botón de auriculares). Derecha: solo el master."));
        phonesVolume.onValueChange = [this] { mixer.phonesVolume = (float) phonesVolume.getValue(); AudioRouting::save (mixer, settings); };
        cueMix.onValueChange = [this] { mixer.cueMix = (float) cueMix.getValue(); AudioRouting::save (mixer, settings); };

        info.setColour (juce::Label::textColourId, textDim);
        info.setFont (juce::FontOptions (12.0f));
        info.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (info);

        devices.addChangeListener (this);
        refresh();
    }

    ~AudioPage() override   { devices.removeChangeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto row = [&r] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (6); return x; };
        auto labelled = [&] (juce::Label* l, juce::Component& c, juce::Component* extra = nullptr)
        {
            auto line = row (26);
            l->setBounds (line.removeFromLeft (140));
            if (extra != nullptr)
            {
                extra->setBounds (line.removeFromRight (160));
                line.removeFromRight (8);
            }
            c.setBounds (line);
        };

        labels[0]->setBounds (row (20));
        {
            auto line = row (30);
            const int w = (line.getWidth() - 16) / 3;
            presetSpeakers.setBounds (line.removeFromLeft (w));
            line.removeFromLeft (8);
            presetCard.setBounds (line.removeFromLeft (w));
            line.removeFromLeft (8);
            presetSplit.setBounds (line);
        }
        r.removeFromTop (8);
        labels[1]->setBounds (row (20));
        labelled (typeLabel, typeBox);
        labelled (deviceLabel, deviceBox, &controlPanel);
        labelled (rateLabel, rateBox);
        labelled (bufferLabel, bufferBox);
        r.removeFromTop (8);
        labels[6]->setBounds (row (20));
        labelled (masterLabel, masterBox, &testMaster);
        labelled (phonesLabel, phonesBox, &testPhones);
        r.removeFromTop (8);
        labels[9]->setBounds (row (20));
        labelled (phonesVolLabel, phonesVolume);
        labelled (cueMixLabel, cueMix);
        r.removeFromTop (6);
        info.setBounds (r);
    }

    void paint (juce::Graphics& g) override     { g.fillAll (panel); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override    { refresh(); }

    int numOutputs() const
    {
        auto* d = devices.getCurrentAudioDevice();
        return d != nullptr ? d->getOutputChannelNames().size() : 0;
    }

    void report (const juce::String& error)
    {
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Audio", error);
    }

    void afterDeviceChange()
    {
        // En una tarjeta con 4 salidas o más, los auriculares van a 3-4 si no había nada elegido
        if (numOutputs() >= 4 && mixer.phonesOut.load() < 0 && ! mixer.splitMono.load())
            mixer.phonesOut = 2;
        if (mixer.masterOut.load() + 2 > numOutputs())
            mixer.masterOut = 0;
        AudioRouting::save (mixer, settings);
        AudioRouting::applyChannels (devices, mixer);
        refresh();
    }

    void setRouting (int masterFirst, int phonesFirst, bool split)
    {
        mixer.masterOut = masterFirst;
        mixer.phonesOut = phonesFirst;
        mixer.splitMono = split;
        AudioRouting::save (mixer, settings);
        AudioRouting::applyChannels (devices, mixer);
        refresh();
    }

    void refresh()
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        auto* device = devices.getCurrentAudioDevice();

        typeBox.clear (juce::dontSendNotification);
        int id = 1;
        for (auto* type : devices.getAvailableDeviceTypes())
        {
            typeBox.addItem (type->getTypeName(), id);
            if (type->getTypeName() == devices.getCurrentAudioDeviceType())
                typeBox.setSelectedId (id, juce::dontSendNotification);
            ++id;
        }

        deviceBox.clear (juce::dontSendNotification);
        if (auto* type = devices.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            id = 1;
            for (auto& name : type->getDeviceNames (false))
            {
                deviceBox.addItem (name, id);
                if (device != nullptr && name == device->getName())
                    deviceBox.setSelectedId (id, juce::dontSendNotification);
                ++id;
            }
        }

        rateBox.clear (juce::dontSendNotification);
        bufferBox.clear (juce::dontSendNotification);
        masterBox.clear (juce::dontSendNotification);
        phonesBox.clear (juce::dontSendNotification);

        if (device == nullptr)
        {
            info.setText (juce::String::fromUTF8 ("No hay ningún dispositivo de audio abierto."), juce::dontSendNotification);
            return;
        }

        const double rate = device->getCurrentSampleRate();
        for (auto sr : device->getAvailableSampleRates())
            rateBox.addItem (juce::String (sr / 1000.0, 1) + " kHz", (int) sr);
        rateBox.setSelectedId ((int) rate, juce::dontSendNotification);

        for (auto bs : device->getAvailableBufferSizes())
            bufferBox.addItem (juce::String (bs) + " muestras  (" + juce::String (bs * 1000.0 / rate, 1) + " ms)", bs);
        bufferBox.setSelectedId (device->getCurrentBufferSizeSamples(), juce::dontSendNotification);

        controlPanel.setEnabled (device->hasControlPanel());

        const auto names = device->getOutputChannelNames();
        const int outs = names.size();
        for (int first = 0; first + 1 < outs; first += 2)
        {
            masterBox.addItem (pairName (names, first), 100 + first);
            phonesBox.addItem (pairName (names, first), 100 + first);
        }
        phonesBox.addSeparator();
        phonesBox.addItem ("Ninguno (sin pre-escucha)", 1);
        phonesBox.addItem (juce::String::fromUTF8 ("Divisor: izquierda master / derecha auriculares (mono)"), 2);

        masterBox.setSelectedId (100 + mixer.masterOut.load(), juce::dontSendNotification);
        const bool phonesPairExists = mixer.phonesOut.load() >= 0 && outs >= mixer.phonesOut.load() + 2;
        phonesBox.setSelectedId (mixer.splitMono.load() ? 2 : phonesPairExists ? 100 + mixer.phonesOut.load() : 1,
                                 juce::dontSendNotification);
        testPhones.setEnabled (mixer.hasPhonesOutput() || mixer.splitMono.load() || (mixer.phonesOut.load() >= 0 && outs >= mixer.phonesOut.load() + 2));

        const int latencyMs = juce::roundToInt ((device->getOutputLatencyInSamples() + device->getCurrentBufferSizeSamples()) * 1000.0 / rate);
        juce::String summary;
        summary << device->getName() << "  -  " << outs << " salidas  -  latencia aprox. " << latencyMs << " ms\n";
        if (outs < 4 && ! mixer.splitMono.load())
            summary << juce::String::fromUTF8 ("Este dispositivo no tiene salida de auriculares separada. Con una controladora DJ, "
                                            "conéctala y elígela en \"Dispositivo\"; sin ella, usa el cable divisor.\n");
        summary << juce::String::fromUTF8 ("Consejo: con \"Windows Audio (Low Latency Mode)\" o \"Exclusive Mode\" la latencia es menor. "
                                        "Si se oyen cortes, sube el búfer.");
        info.setText (summary, juce::dontSendNotification);
    }

    juce::AudioDeviceManager& devices;
    MixerEngine& mixer;
    juce::PropertiesFile& settings;
    bool updating = false;

    juce::OwnedArray<juce::Label> labels;
    juce::Label *typeLabel, *deviceLabel, *rateLabel, *bufferLabel, *masterLabel, *phonesLabel, *phonesVolLabel, *cueMixLabel;
    juce::TextButton presetSpeakers, presetCard, presetSplit, controlPanel, testMaster, testPhones;
    juce::ComboBox typeBox, deviceBox, rateBox, bufferBox, masterBox, phonesBox;
    juce::Slider phonesVolume, cueMix;
    juce::Label info;
};

//==============================================================================
// Pestaña Controladoras
class ControllersPage : public juce::Component,
                        private juce::Timer
{
public:
    ControllersPage (MidiController& m, std::function<void (bool)> setLearningFn, std::function<bool()> isLearningFn)
        : midi (m), setLearning (std::move (setLearningFn)), isLearning (std::move (isLearningFn))
    {
        makeLabel (*this, labels, "CONTROLADORAS CONECTADAS", true);
        makeLabel (*this, labels, "ASIGNACIONES", true);

        deviceList.setModel (&deviceModel);
        deviceList.setRowHeight (28);
        deviceList.setColour (juce::ListBox::backgroundColourId, inset);
        deviceList.setColour (juce::ListBox::outlineColourId, border);
        deviceList.setOutlineThickness (1);
        addAndMakeVisible (deviceList);

        auto& header = mapTable.getHeader();
        header.addColumn ("Control de CesarDJ", 1, 200, 80, -1, juce::TableHeaderComponent::notSortable);
        header.addColumn ("Mensaje MIDI", 2, 170, 80, -1, juce::TableHeaderComponent::notSortable);
        header.addColumn ("Controladora", 3, 220, 80, -1, juce::TableHeaderComponent::notSortable);
        mapTable.setModel (&mapModel);
        mapTable.setRowHeight (24);
        mapTable.setMultipleSelectionEnabled (false);
        mapTable.setColour (juce::ListBox::outlineColourId, border);
        mapTable.setOutlineThickness (1);
        addAndMakeVisible (mapTable);

        learnButton.setClickingTogglesState (false);
        learnButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0912a).darker (0.2f));
        learnButton.setTooltip (juce::String::fromUTF8 ("Haz clic en un control de CesarDJ (botón, fader, plato...) y después muévelo en tu controladora. Esc para salir."));
        learnButton.onClick = [this] { setLearning (! isLearning()); updateButtons(); };
        removeButton.onClick = [this]
        {
            midi.removeMapping (mapTable.getSelectedRow());
            reload();
        };
        clearDeviceButton.onClick = [this]
        {
            const auto device = selectedDevice();
            juce::Component::SafePointer<ControllersPage> safe (this);
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Borrar asignaciones",
                device.isNotEmpty() ? juce::String::fromUTF8 ("Se borrarán las asignaciones de ") + device + "."
                                    : juce::String::fromUTF8 ("Se borrarán TODAS las asignaciones."),
                "Borrar", "Cancelar", nullptr,
                juce::ModalCallbackFunction::create ([safe, device] (int r)
                {
                    if (r != 1 || safe == nullptr) return;
                    if (device.isNotEmpty()) safe->midi.clearMappingsForDevice (device);
                    else                     safe->midi.clearMappings();
                    safe->reload();
                }));
        };
        saveProfileButton.onClick = [this] { saveProfile(); };
        loadProfileButton.onClick = [this] { loadProfile(); };
        folderButton.onClick = [] { auto f = MidiController::userProfileFolder(); f.createDirectory(); f.startAsProcess(); };
        for (auto* b : { &learnButton, &removeButton, &clearDeviceButton, &saveProfileButton, &loadProfileButton, &folderButton })
        {
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }

        help.setColour (juce::Label::textColourId, textDim);
        help.setFont (juce::FontOptions (12.0f));
        help.setJustificationType (juce::Justification::topLeft);
        help.setText (juce::String::fromUTF8 (
            "Conecta la controladora por USB: aparece aquí sola (la luz se enciende al tocar un control). Si es una controladora "
            "con tarjeta de sonido, elígela también en la pestaña Audio.\n"
            "Para asignar controles: pulsa \"Aprender\", haz clic en un control de CesarDJ y muévelo en la controladora. "
            "Cuando la tengas lista, pulsa \"Guardar perfil\": la próxima vez (o en otro PC) se carga sola al conectarla."),
            juce::dontSendNotification);
        addAndMakeVisible (help);

        reload();
        startTimerHz (10);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        labels[0]->setBounds (r.removeFromTop (20));
        r.removeFromTop (4);
        deviceList.setBounds (r.removeFromTop (28 * 3 + 2));
        r.removeFromTop (12);

        auto titleRow = r.removeFromTop (28);
        labels[1]->setBounds (titleRow.removeFromLeft (140));
        learnButton.setBounds (titleRow.removeFromRight (190));
        r.removeFromTop (4);

        help.setBounds (r.removeFromBottom (64));
        r.removeFromBottom (8);
        auto buttons = r.removeFromBottom (28);
        const int w = (buttons.getWidth() - 4 * 6) / 5;
        for (auto* b : { &removeButton, &clearDeviceButton, &saveProfileButton, &loadProfileButton, &folderButton })
        {
            b->setBounds (buttons.removeFromLeft (w));
            buttons.removeFromLeft (6);
        }
        r.removeFromBottom (8);
        mapTable.setBounds (r);
    }

    void paint (juce::Graphics& g) override     { g.fillAll (panel); }

    void visibilityChanged() override
    {
        if (isShowing() && ! noticeShown)
        {
            noticeShown = true;
            juce::MessageManager::callAsync ([]
            {
                Help::notice ("controllersProfiles", "Controladoras",
                    juce::String::fromUTF8 ("CesarDJ no trae perfiles de fábrica para modelos concretos (Pioneer, Hercules, Numark...): "
                        "sin el aparato delante no se pueden comprobar sus mensajes MIDI.\n\n"
                        "La primera vez: pulsa \"Aprender\", haz clic en un control de CesarDJ y muévelo en tu controladora. "
                        "Después pulsa \"Guardar perfil\": la próxima vez se carga solo al conectarla, también en otro PC.\n\n"
                        "Las luces (LED) de la controladora no se encienden desde CesarDJ."), "controladoras");
            });
        }
    }

private:
    bool noticeShown = false;

    //== Lista de controladoras
    struct DeviceModel : public juce::ListBoxModel
    {
        ControllersPage& page;
        explicit DeviceModel (ControllersPage& p) : page (p) {}

        int getNumRows() override   { return juce::jmax (1, (int) page.deviceInfo.size()); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (page.deviceInfo.empty())
            {
                g.setColour (textDim);
                g.setFont (juce::FontOptions (13.0f));
                g.drawText (juce::String::fromUTF8 ("Ninguna controladora conectada. Conéctala por USB y espera unos segundos."),
                            10, 0, w - 20, h, juce::Justification::centredLeft);
                return;
            }
            if (! juce::isPositiveAndBelow (row, (int) page.deviceInfo.size()))
                return;

            const auto& d = page.deviceInfo[(size_t) row];
            if (selected)
                g.fillAll (deckA.withAlpha (0.25f));

            // Casilla activar/desactivar
            const juce::Rectangle<float> box (10.0f, h * 0.5f - 7.0f, 14.0f, 14.0f);
            g.setColour (border);
            g.drawRoundedRectangle (box, 2.0f, 1.2f);
            if (d.enabled)
            {
                g.setColour (play);
                g.fillRoundedRectangle (box.reduced (3.0f), 1.5f);
            }

            // Luz de actividad
            const bool active = juce::Time::getMillisecondCounter() - d.lastActivity < 200 && d.lastActivity != 0;
            g.setColour (active ? juce::Colours::limegreen : juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (34.0f, h * 0.5f - 4.0f, 8.0f, 8.0f);

            g.setColour (d.enabled ? text : textDim);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (d.name, 52, 0, w - 220, h, juce::Justification::centredLeft, true);

            g.setColour (textDim);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (d.enabled ? juce::String (d.numMappings) + " controles asignados" : juce::String ("desactivada"),
                        w - 170, 0, 160, h, juce::Justification::centredRight);
        }

        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            if (juce::isPositiveAndBelow (row, (int) page.deviceInfo.size()) && e.x < 30)
            {
                const auto& d = page.deviceInfo[(size_t) row];
                page.midi.setDeviceEnabled (d.name, ! d.enabled);
                page.reload();
            }
        }
    };

    //== Tabla de asignaciones
    struct MapModel : public juce::TableListBoxModel
    {
        ControllersPage& page;
        explicit MapModel (ControllersPage& p) : page (p) {}

        int getNumRows() override   { return (int) page.mappingInfo.size(); }

        void paintRowBackground (juce::Graphics& g, int row, int, int, bool selected) override
        {
            if (selected)            g.fillAll (deckA.withAlpha (0.3f));
            else if (row % 2 == 1)   g.fillAll (juce::Colours::white.withAlpha (0.025f));
        }

        void paintCell (juce::Graphics& g, int row, int column, int w, int h, bool) override
        {
            if (! juce::isPositiveAndBelow (row, (int) page.mappingInfo.size()))
                return;
            const auto& m = page.mappingInfo[(size_t) row];
            g.setColour (column == 1 ? text : textDim);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText (column == 1 ? m.target : column == 2 ? m.message : m.device, 6, 0, w - 10, h,
                        juce::Justification::centredLeft, true);
        }

        void deleteKeyPressed (int row) override
        {
            page.midi.removeMapping (row);
            page.reload();
        }

        void selectedRowsChanged (int) override     { page.updateButtons(); }
    };

    void timerCallback() override
    {
        // Controladoras que se conectan o desconectan, luces y asignaciones nuevas (MIDI learn)
        const auto devicesNow = midi.getDevices();
        bool changed = devicesNow.size() != deviceInfo.size();
        for (size_t i = 0; ! changed && i < devicesNow.size(); ++i)
            changed = devicesNow[i].name != deviceInfo[i].name || devicesNow[i].numMappings != deviceInfo[i].numMappings
                      || devicesNow[i].enabled != deviceInfo[i].enabled;

        deviceInfo = devicesNow;
        if (changed)
            deviceList.updateContent();
        deviceList.repaint();

        if (midi.getNumMappings() != (int) mappingInfo.size())
            reload();
        updateButtons();
    }

    void reload()
    {
        deviceInfo = midi.getDevices();
        mappingInfo = midi.getMappingList();
        deviceList.updateContent();
        mapTable.updateContent();
        deviceList.repaint();
        mapTable.repaint();
        updateButtons();
    }

    void updateButtons()
    {
        const bool learning = isLearning();
        learnButton.setToggleState (learning, juce::dontSendNotification);
        learnButton.setButtonText (learning ? "Aprendiendo... (clic para salir)" : "Aprender (MIDI learn)");
        removeButton.setEnabled (mapTable.getSelectedRow() >= 0);
        clearDeviceButton.setButtonText (selectedDevice().isNotEmpty() ? "Borrar de esta" : "Borrar todas");
        clearDeviceButton.setEnabled (! mappingInfo.empty());
        saveProfileButton.setEnabled (! mappingInfo.empty());
    }

    juce::String selectedDevice() const
    {
        const int row = deviceList.getSelectedRow();
        return juce::isPositiveAndBelow (row, (int) deviceInfo.size()) ? deviceInfo[(size_t) row].name : juce::String();
    }

    void saveProfile()
    {
        auto device = selectedDevice();
        if (device.isEmpty() && deviceInfo.size() == 1)
            device = deviceInfo[0].name;

        const auto folder = MidiController::userProfileFolder();
        folder.createDirectory();
        const auto suggested = folder.getChildFile (juce::File::createLegalFileName (device.isNotEmpty() ? device : juce::String ("Mi controladora")) + ".xml");

        chooser = std::make_unique<juce::FileChooser> ("Guardar perfil de controladora", suggested, "*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, device] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  f = f.withFileExtension ("xml");
                                  const bool ok = midi.exportProfile (f, device, f.getFileNameWithoutExtension());
                                  juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                      "Perfil de controladora",
                                      ok ? juce::String::fromUTF8 ("Perfil guardado.\n") + (device.isNotEmpty()
                                               ? juce::String::fromUTF8 ("Se cargará solo al conectar \"") + device + "\"."
                                               : juce::String::fromUTF8 ("Cárgalo con \"Cargar perfil\"."))
                                         : juce::String::fromUTF8 ("No se pudo guardar el perfil."));
                              });
    }

    void loadProfile()
    {
        auto device = selectedDevice();
        if (device.isEmpty() && deviceInfo.size() == 1)
            device = deviceInfo[0].name;

        chooser = std::make_unique<juce::FileChooser> ("Cargar perfil de controladora", MidiController::userProfileFolder(), "*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, device] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (! f.existsAsFile())
                                      return;
                                  const int n = midi.importProfile (f, device);
                                  reload();
                                  juce::AlertWindow::showMessageBoxAsync (n > 0 ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                      "Perfil de controladora",
                                      n > 0 ? juce::String (n) + juce::String::fromUTF8 (" controles cargados")
                                                  + (device.isNotEmpty() ? " para " + device : juce::String()) + "."
                                            : juce::String::fromUTF8 ("El archivo no es un perfil de CesarDJ."));
                              });
    }

    MidiController& midi;
    std::function<void (bool)> setLearning;
    std::function<bool()> isLearning;

    std::vector<MidiController::DeviceInfo> deviceInfo;
    std::vector<MidiController::MappingInfo> mappingInfo;
    DeviceModel deviceModel { *this };
    MapModel mapModel { *this };

    juce::OwnedArray<juce::Label> labels;
    juce::ListBox deviceList;
    juce::TableListBox mapTable;
    juce::TextButton learnButton, removeButton { "Quitar" }, clearDeviceButton { "Borrar todas" },
                     saveProfileButton { "Guardar perfil..." }, loadProfileButton { "Cargar perfil..." }, folderButton { "Carpeta de perfiles" };
    juce::Label help;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
SettingsWindow::SettingsWindow (juce::AudioDeviceManager& devices, MixerEngine& mixer, MidiController& midi,
                                juce::PropertiesFile& settings, std::function<void (bool)> setLearning, std::function<bool()> isLearning)
    : juce::DocumentWindow (juce::String::fromUTF8 ("Configuración - CesarDJ"), panel, juce::DocumentWindow::closeButton)
{
    auto* t = new juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop);
    t->setTabBarDepth (34);
    t->setOutline (0);
    t->getTabbedButtonBar().setColour (juce::TabbedButtonBar::tabTextColourId, textDim);
    t->getTabbedButtonBar().setColour (juce::TabbedButtonBar::frontTextColourId, juce::Colours::white);
    t->getTabbedButtonBar().setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    t->addTab ("Audio", panel, new AudioPage (devices, mixer, settings), true);
    t->addTab ("Controladoras", panel, new ControllersPage (midi, std::move (setLearning), std::move (isLearning)), true);
    tabs = t;

    setContentOwned (t, false);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (640, 560, 1400, 1100);
    centreWithSize (760, 640);
    setVisible (true);
}

void SettingsWindow::showTab (Tab tab)
{
    tabs->setCurrentTabIndex ((int) tab);
    toFront (true);
}

void SettingsWindow::closeButtonPressed()
{
    if (onClose != nullptr)
        onClose();
}
