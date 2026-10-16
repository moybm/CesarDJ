#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Sampler de 8 huecos (los pads 1-4 del deck A son los huecos 0-3 y los del deck B los 4-7).
// Trae 8 sonidos sintetizados de serie; cada hueco se puede cambiar por un archivo propio.
// La interfaz manda órdenes por una cola sin bloqueos; el hilo de audio mezcla las voces en el master.
class SamplerEngine
{
public:
    static constexpr int numSlots = 8;

    struct SlotInfo
    {
        juce::String name;
        bool loop = false;
        juce::File file;            // vacío = sonido de serie
    };

    SamplerEngine();

    //== Hilo de mensajes =========================================================
    bool loadFile (int slot, const juce::File& file, juce::AudioFormatManager& formats);
    void restoreDefault (int slot);
    void setLoop (int slot, bool shouldLoop);
    void trigger (int slot);                    // desde el principio (en bucle: alterna play/stop)
    void stop (int slot);
    bool isPlaying (int slot) const             { return playing[(size_t) slot].load(); }
    const SlotInfo& getInfo (int slot) const    { return infos[(size_t) slot]; }

    std::atomic<float> volume { 0.8f };

    //== Hilo de audio ============================================================
    void prepare (double sampleRate);
    void addTo (juce::AudioBuffer<float>& out, int startSample, int numSamples);

private:
    struct Sample
    {
        juce::AudioBuffer<float> audio;     // estéreo
        double sampleRate = 44100.0;
    };

    enum class Cmd { setSample, trigger, stop, setLoop };
    struct Command { Cmd type; int slot; Sample* sample = nullptr; bool flag = false; };

    void push (const Command& c);
    void setSample (int slot, std::shared_ptr<Sample> s, const SlotInfo& info);
    static std::shared_ptr<Sample> makeBuiltIn (int slot, juce::String& name);

    // Hilo de mensajes
    std::array<std::shared_ptr<Sample>, numSlots> owned, retired;
    std::array<SlotInfo, numSlots> infos;

    // Compartido
    juce::AbstractFifo fifo { 128 };
    std::array<Command, 128> commands;
    std::array<std::atomic<bool>, numSlots> playing;

    // Hilo de audio
    struct Voice
    {
        Sample* sample = nullptr;
        double pos = 0.0;
        bool active = false, loop = false;
        int fadeOut = -1;                       // >= 0: apagándose (evita clics)
    };
    std::array<Voice, numSlots> voices;
    double deviceRate = 44100.0;
    juce::SmoothedValue<float> gain { 0.8f };
};
