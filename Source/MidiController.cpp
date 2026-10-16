#include "MidiController.h"
#include "DeckComponent.h"
#include "JogWheel.h"

namespace
{
    const char* kindName (int k)    { return k == 0 ? "cc" : k == 1 ? "note" : "pb"; }
}

MidiController::MidiController (juce::AudioDeviceManager& dm, juce::PropertiesFile& props, juce::Component& r)
    : devices (dm), settings (props), root (r)
{
    load();
    devices.addMidiInputDeviceCallback ({}, this);     // {} = todos los dispositivos activados
    timerCallback();
    startTimer (2000);                                  // detectar controladoras que se conectan después
}

MidiController::~MidiController()
{
    devices.removeMidiInputDeviceCallback ({}, this);
}

void MidiController::timerCallback()
{
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (! devices.isMidiInputDeviceEnabled (d.identifier))
            devices.setMidiInputDeviceEnabled (d.identifier, true);
}

juce::StringArray MidiController::getDeviceNames() const
{
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        names.add (d.name);
    return names;
}

void MidiController::setLearning (bool shouldLearn)
{
    learning = shouldLearn;
    if (! learning)
        learnTarget = nullptr;
}

bool MidiController::isMapped (const juce::String& id) const
{
    return std::any_of (mappings.begin(), mappings.end(), [&id] (const Mapping& m) { return m.target == id; });
}

void MidiController::clearMappings()
{
    mappings.clear();
    save();
}

void MidiController::clearMappingsFor (const juce::String& id)
{
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&id] (const Mapping& m) { return m.target == id; }),
                    mappings.end());
    save();
}

//==============================================================================
// Llega en el hilo MIDI: se pasa al hilo de mensajes (la interfaz no se toca desde otro hilo)
void MidiController::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    std::weak_ptr<bool> weak = alive;
    juce::MessageManager::callAsync ([this, weak, message]
    {
        if (! weak.expired())
            handleOnMessageThread (message);
    });
}

void MidiController::handleOnMessageThread (const juce::MidiMessage& m)
{
    Kind kind;
    int number = 0, value = 0;
    bool pressed = false;

    if (m.isController())
    {
        kind = Kind::cc;
        number = m.getControllerNumber();
        value = m.getControllerValue();
        pressed = value > 63;
    }
    else if (m.isNoteOn())
    {
        kind = Kind::note;
        number = m.getNoteNumber();
        value = m.getVelocity();
        pressed = true;
    }
    else if (m.isNoteOff())
    {
        kind = Kind::note;
        number = m.getNoteNumber();
        pressed = false;
    }
    else if (m.isPitchWheel())
    {
        kind = Kind::pitchBend;
        value = m.getPitchWheelValue();
    }
    else
    {
        return;
    }

    lastActivity = juce::Time::getMillisecondCounter();
    const int channel = m.getChannel();

    // Modo aprendizaje: el primer mensaje (que no sea soltar una nota) se asigna al control elegido
    if (learning)
    {
        auto* target = learnTarget.getComponent();
        if (target == nullptr || (kind == Kind::note && ! pressed))
            return;

        mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const Mapping& x)
                        { return x.kind == kind && x.channel == channel && x.number == number; }),
                        mappings.end());
        mappings.push_back ({ kind, channel, number, target->getComponentID() });
        save();

        if (onLearned != nullptr)
        {
            const juce::String desc = kind == Kind::cc   ? "CC " + juce::String (number)
                                    : kind == Kind::note ? "Nota " + juce::String (number)
                                                         : juce::String ("Pitch bend");
            onLearned (target->getComponentID(), desc + " (canal " + juce::String (channel) + ")");
        }
        return;
    }

    for (auto& map : mappings)
        if (map.kind == kind && map.channel == channel && map.number == number)
            if (auto* c = findComponent (map.target))
                apply (*c, kind, value, pressed);
}

void MidiController::apply (juce::Component& target, Kind kind, int value, bool pressed)
{
    // Plato: encoder relativo (1..63 = adelante, 65..127 = atrás)
    if (auto* jog = dynamic_cast<JogWheel*> (&target))
    {
        if (kind == Kind::cc && value != 64)
            jog->midiTurn (value < 64 ? value : value - 128);
        return;
    }

    if (auto* slider = dynamic_cast<juce::Slider*> (&target))
    {
        if (kind == Kind::note)
        {
            if (pressed)
                slider->setValue (slider->getDoubleClickReturnValue(), juce::sendNotificationSync);
            return;
        }

        const double proportion = kind == Kind::pitchBend ? value / 16383.0 : value / 127.0;
        slider->setValue (slider->proportionOfLengthToValue (proportion), juce::sendNotificationSync);
        return;
    }

    if (auto* pad = dynamic_cast<PerformancePad*> (&target))
    {
        pad->externalPress (pressed);
        return;
    }

    if (auto* button = dynamic_cast<juce::Button*> (&target))
        if (pressed)
            button->triggerClick();
}

juce::Component* MidiController::findComponent (const juce::String& id)
{
    auto it = componentCache.find (id);
    if (it != componentCache.end() && it->second != nullptr)
        return it->second.getComponent();

    std::function<juce::Component* (juce::Component&)> search = [&] (juce::Component& c) -> juce::Component*
    {
        if (c.getComponentID() == id)
            return &c;
        for (auto* child : c.getChildren())
            if (auto* found = search (*child))
                return found;
        return nullptr;
    };

    auto* found = search (root);
    componentCache[id] = found;
    return found;
}

//==============================================================================
void MidiController::save()
{
    juce::XmlElement xml ("MIDIMAP");
    for (auto& m : mappings)
    {
        auto* e = xml.createNewChildElement ("MAP");
        e->setAttribute ("kind", kindName ((int) m.kind));
        e->setAttribute ("channel", m.channel);
        e->setAttribute ("number", m.number);
        e->setAttribute ("target", m.target);
    }
    settings.setValue ("midiMap", &xml);
    settings.saveIfNeeded();
}

void MidiController::load()
{
    mappings.clear();
    if (auto xml = settings.getXmlValue ("midiMap"))
        for (auto* e : xml->getChildIterator())
        {
            const auto k = e->getStringAttribute ("kind");
            mappings.push_back ({ k == "cc" ? Kind::cc : k == "note" ? Kind::note : Kind::pitchBend,
                                  e->getIntAttribute ("channel"), e->getIntAttribute ("number"),
                                  e->getStringAttribute ("target") });
        }
}
