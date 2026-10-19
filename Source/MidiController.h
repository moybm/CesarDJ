#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Controladoras MIDI con "MIDI learn":
//  1) Activar el modo aprendizaje
//  2) Clic en un control de la pantalla (botón, mando, fader, pad, plato)
//  3) Mover / pulsar el control de la controladora -> quedan enlazados
// Cualquier control con componentID se puede asignar. Las asignaciones se guardan en la configuración,
// van ligadas a la controladora que las envió y se pueden guardar como perfil (.xml). Al conectar una
// controladora sin asignaciones se busca un perfil para ella y se carga solo.
class MidiController : private juce::MidiInputCallback,
                       private juce::Timer
{
public:
    MidiController (juce::AudioDeviceManager& deviceManager, juce::PropertiesFile& settings, juce::Component& root);
    ~MidiController() override;

    void setLearning (bool shouldLearn);
    bool isLearning() const                         { return learning; }
    void setLearnTarget (juce::Component* c)        { learnTarget = c; }
    juce::Component* getLearnTarget() const         { return learnTarget.getComponent(); }

    bool isMapped (const juce::String& componentId) const;
    void clearMappings();
    void clearMappingsFor (const juce::String& componentId);
    int getNumMappings() const                      { return (int) mappings.size(); }
    juce::StringArray getDeviceNames() const;
    juce::uint32 getLastActivity() const            { return lastActivity; }

    //== Para la ventana de Configuración ==========================================
    struct DeviceInfo
    {
        juce::String name;
        bool enabled = true;
        juce::uint32 lastActivity = 0;
        int numMappings = 0;
    };
    std::vector<DeviceInfo> getDevices() const;
    void setDeviceEnabled (const juce::String& name, bool enabled);

    struct MappingInfo
    {
        juce::String target, message, device;
    };
    std::vector<MappingInfo> getMappingList() const;
    void removeMapping (int index);
    void clearMappingsForDevice (const juce::String& device);

    // Perfiles: un archivo .xml con las asignaciones de una controladora
    bool exportProfile (const juce::File& file, const juce::String& device, const juce::String& profileName) const;
    int importProfile (const juce::File& file, const juce::String& device);       // devuelve cuántas se cargaron
    static juce::File userProfileFolder();
    static juce::Array<juce::File> findProfiles();

    std::function<void (const juce::String& componentId, const juce::String& description)> onLearned;
    std::function<void (const juce::String& message)> onStatus;                   // controladora detectada, perfil cargado...

private:
    enum class Kind { cc, note, pitchBend };

    struct Mapping
    {
        Kind kind;
        int channel, number;
        juce::String target;
        juce::String device;            // vacío = cualquier controladora
    };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;
    void handleOnMessageThread (const juce::String& device, const juce::MidiMessage& message);
    void apply (juce::Component& target, Kind kind, int value, bool pressed);
    juce::Component* findComponent (const juce::String& id);
    void timerCallback() override;
    void autoLoadProfile (const juce::String& device);
    void save();
    void load();
    static juce::String describe (const Mapping& m);
    static std::unique_ptr<juce::XmlElement> toXml (const std::vector<Mapping>& list);

    juce::AudioDeviceManager& devices;
    juce::PropertiesFile& settings;
    juce::Component& root;

    std::vector<Mapping> mappings;
    std::map<juce::String, juce::Component::SafePointer<juce::Component>> componentCache;
    bool learning = false;
    juce::Component::SafePointer<juce::Component> learnTarget;
    juce::uint32 lastActivity = 0;
    std::map<juce::String, juce::uint32> deviceActivity;
    juce::StringArray disabledDevices, knownDevices;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiController)
};
