#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Controladoras MIDI con "MIDI learn":
//  1) Activar el modo aprendizaje
//  2) Clic en un control de la pantalla (botón, mando, fader, pad, plato)
//  3) Mover / pulsar el control de la controladora -> quedan enlazados
// Cualquier control con componentID se puede asignar. Las asignaciones se guardan en la configuración.
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

    std::function<void (const juce::String& componentId, const juce::String& description)> onLearned;

private:
    enum class Kind { cc, note, pitchBend };

    struct Mapping
    {
        Kind kind;
        int channel, number;
        juce::String target;
    };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;
    void handleOnMessageThread (const juce::MidiMessage& message);
    void apply (juce::Component& target, Kind kind, int value, bool pressed);
    juce::Component* findComponent (const juce::String& id);
    void timerCallback() override;
    void save();
    void load();

    juce::AudioDeviceManager& devices;
    juce::PropertiesFile& settings;
    juce::Component& root;

    std::vector<Mapping> mappings;
    std::map<juce::String, juce::Component::SafePointer<juce::Component>> componentCache;
    bool learning = false;
    juce::Component::SafePointer<juce::Component> learnTarget;
    juce::uint32 lastActivity = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiController)
};
