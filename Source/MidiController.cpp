#include "MidiController.h"
#include "DeckComponent.h"
#include "JogWheel.h"

namespace
{
    const char* kindName (int k)    { return k == 0 ? "cc" : k == 1 ? "note" : "pb"; }

    // Nombre del perfil o del dispositivo que encaja (sin distinguir mayúsculas)
    bool profileMatches (const juce::XmlElement& xml, const juce::String& device)
    {
        for (auto& token : juce::StringArray::fromTokens (xml.getStringAttribute ("match"), ";", ""))
            if (token.trim().isNotEmpty() && device.containsIgnoreCase (token.trim()))
                return true;
        return false;
    }
}

MidiController::MidiController (juce::AudioDeviceManager& dm, juce::PropertiesFile& props, juce::Component& r)
    : devices (dm), settings (props), root (r)
{
    disabledDevices = juce::StringArray::fromLines (settings.getValue ("midiDisabled"));
    disabledDevices.removeEmptyStrings();
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
    {
        const bool wanted = ! disabledDevices.contains (d.name);
        if (devices.isMidiInputDeviceEnabled (d.identifier) != wanted)
            devices.setMidiInputDeviceEnabled (d.identifier, wanted);

        if (! knownDevices.contains (d.name))
        {
            knownDevices.add (d.name);
            if (wanted)
                autoLoadProfile (d.name);
        }
    }
}

// Controladora nueva sin asignaciones propias: se busca un perfil que la nombre
void MidiController::autoLoadProfile (const juce::String& device)
{
    const bool hasOwn = std::any_of (mappings.begin(), mappings.end(), [&device] (const Mapping& m) { return m.device == device; });
    if (hasOwn)
    {
        if (onStatus) onStatus (juce::String::fromUTF8 ("Controladora conectada: ") + device);
        return;
    }

    for (auto& f : findProfiles())
        if (auto xml = juce::parseXML (f))
            if (profileMatches (*xml, device))
            {
                const int n = importProfile (f, device);
                if (onStatus)
                    onStatus (juce::String::fromUTF8 ("Controladora conectada: ") + device + " - perfil \""
                              + xml->getStringAttribute ("name", f.getFileNameWithoutExtension()) + "\" (" + juce::String (n) + " controles)");
                return;
            }

    if (onStatus)
        onStatus (juce::String::fromUTF8 ("Controladora conectada: ") + device
                  + juce::String::fromUTF8 (" - sin perfil: asígnala en CONFIG > Controladoras (MIDI learn)"));
}

juce::StringArray MidiController::getDeviceNames() const
{
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        names.add (d.name);
    return names;
}

std::vector<MidiController::DeviceInfo> MidiController::getDevices() const
{
    std::vector<DeviceInfo> list;
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        DeviceInfo info;
        info.name = d.name;
        info.enabled = ! disabledDevices.contains (d.name);
        if (auto it = deviceActivity.find (d.name); it != deviceActivity.end())
            info.lastActivity = it->second;
        info.numMappings = (int) std::count_if (mappings.begin(), mappings.end(),
                                                [&d] (const Mapping& m) { return m.device == d.name || m.device.isEmpty(); });
        list.push_back (info);
    }
    return list;
}

void MidiController::setDeviceEnabled (const juce::String& name, bool enabled)
{
    if (enabled)
        disabledDevices.removeString (name);
    else
        disabledDevices.addIfNotAlreadyThere (name);
    settings.setValue ("midiDisabled", disabledDevices.joinIntoString ("\n"));
    settings.saveIfNeeded();
    timerCallback();
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

void MidiController::clearMappingsForDevice (const juce::String& device)
{
    mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&device] (const Mapping& m) { return m.device == device; }),
                    mappings.end());
    save();
}

std::vector<MidiController::MappingInfo> MidiController::getMappingList() const
{
    std::vector<MappingInfo> list;
    for (auto& m : mappings)
        list.push_back ({ m.target, describe (m), m.device.isEmpty() ? juce::String ("(cualquiera)") : m.device });
    return list;
}

void MidiController::removeMapping (int index)
{
    if (juce::isPositiveAndBelow (index, (int) mappings.size()))
    {
        mappings.erase (mappings.begin() + index);
        save();
    }
}

juce::String MidiController::describe (const Mapping& m)
{
    const juce::String what = m.kind == Kind::cc   ? "CC " + juce::String (m.number)
                            : m.kind == Kind::note ? "Nota " + juce::String (m.number)
                                                   : juce::String ("Pitch bend");
    return what + " (canal " + juce::String (m.channel) + ")";
}

//==============================================================================
// Llega en el hilo MIDI: se pasa al hilo de mensajes (la interfaz no se toca desde otro hilo)
void MidiController::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    std::weak_ptr<bool> weak = alive;
    const auto device = source != nullptr ? source->getName() : juce::String();
    juce::MessageManager::callAsync ([this, weak, device, message]
    {
        if (! weak.expired())
            handleOnMessageThread (device, message);
    });
}

void MidiController::handleOnMessageThread (const juce::String& device, const juce::MidiMessage& m)
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
    deviceActivity[device] = lastActivity;
    const int channel = m.getChannel();

    // Modo aprendizaje: el primer mensaje (que no sea soltar una nota) se asigna al control elegido
    if (learning)
    {
        auto* target = learnTarget.getComponent();
        if (target == nullptr || (kind == Kind::note && ! pressed))
            return;

        mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const Mapping& x)
                        { return x.kind == kind && x.channel == channel && x.number == number
                                 && (x.device == device || x.device.isEmpty()); }),
                        mappings.end());
        const Mapping added { kind, channel, number, target->getComponentID(), device };
        mappings.push_back (added);
        save();

        if (onLearned != nullptr)
            onLearned (target->getComponentID(), describe (added) + (device.isNotEmpty() ? " - " + device : juce::String()));
        return;
    }

    for (auto& map : mappings)
        if (map.kind == kind && map.channel == channel && map.number == number
            && (map.device.isEmpty() || map.device == device))
            if (auto* c = findComponent (map.target))
                apply (*c, kind, value, pressed);
}

void MidiController::apply (juce::Component& target, Kind kind, int value, bool pressed)
{
    // Plato: encoder relativo. Las marcas usan dos formatos:
    //  - complemento a dos: 1..63 adelante, 127..65 atrás (Pioneer, Hercules...)
    //  - centrado en 64: 65.. adelante, 63.. atrás (Numark, Reloop...)
    if (auto* jog = dynamic_cast<JogWheel*> (&target))
    {
        if (kind == Kind::cc && value != 64)
        {
            const bool centred = value >= 40 && value <= 88;
            jog->midiTurn (centred ? value - 64 : (value < 64 ? value : value - 128));
        }
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
juce::File MidiController::userProfileFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("CesarDJ").getChildFile ("Controladoras");
}

// Perfiles del usuario y los que vienen con CesarDJ (carpeta "Controladoras" junto al .exe)
juce::Array<juce::File> MidiController::findProfiles()
{
    juce::Array<juce::File> files;
    const auto builtIn = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("Controladoras");
    for (auto& folder : { userProfileFolder(), builtIn })
        if (folder.isDirectory())
            files.addArray (folder.findChildFiles (juce::File::findFiles, false, "*.xml"));
    return files;
}

std::unique_ptr<juce::XmlElement> MidiController::toXml (const std::vector<Mapping>& list)
{
    auto xml = std::make_unique<juce::XmlElement> ("MIDIMAP");
    for (auto& m : list)
    {
        auto* e = xml->createNewChildElement ("MAP");
        e->setAttribute ("kind", kindName ((int) m.kind));
        e->setAttribute ("channel", m.channel);
        e->setAttribute ("number", m.number);
        e->setAttribute ("target", m.target);
        if (m.device.isNotEmpty())
            e->setAttribute ("device", m.device);
    }
    return xml;
}

bool MidiController::exportProfile (const juce::File& file, const juce::String& device, const juce::String& profileName) const
{
    std::vector<Mapping> list;
    for (auto& m : mappings)
        if (device.isEmpty() || m.device == device || m.device.isEmpty())
            list.push_back ({ m.kind, m.channel, m.number, m.target, {} });

    auto xml = toXml (list);
    xml->setAttribute ("name", profileName);
    xml->setAttribute ("match", device);         // se carga solo al conectar una controladora con este nombre
    xml->setAttribute ("app", "CesarDJ");
    file.getParentDirectory().createDirectory();
    return xml->writeTo (file);
}

int MidiController::importProfile (const juce::File& file, const juce::String& device)
{
    auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("MIDIMAP"))
        return 0;

    if (device.isNotEmpty())
        clearMappingsForDevice (device);

    int count = 0;
    for (auto* e : xml->getChildIterator())
    {
        const auto k = e->getStringAttribute ("kind");
        mappings.push_back ({ k == "cc" ? Kind::cc : k == "note" ? Kind::note : Kind::pitchBend,
                              e->getIntAttribute ("channel"), e->getIntAttribute ("number"),
                              e->getStringAttribute ("target"), device });
        ++count;
    }
    save();
    return count;
}

void MidiController::save()
{
    settings.setValue ("midiMap", toXml (mappings).get());
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
                                  e->getStringAttribute ("target"), e->getStringAttribute ("device") });
        }
}
