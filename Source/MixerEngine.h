#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DeckEngine.h"
#include "SamplerEngine.h"
#include "Recorder.h"

// EQ de 3 bandas tipo "isolator" (como en los mixers DJ): con el mando al mínimo la banda se elimina del todo.
// Divide la señal en graves / medios / agudos con filtros Linkwitz-Riley y vuelve a sumarlas con su ganancia.
class IsolatorEQ
{
public:
    void prepare (double sampleRate, int blockSize, int numChannels);
    void setGains (float low, float mid, float high);
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

private:
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowAllpass;
    juce::SmoothedValue<float> lowGain { 1.0f }, midGain { 1.0f }, highGain { 1.0f };
};

// Parámetros de un canal. La interfaz los escribe y el hilo de audio los lee (por eso son atómicos).
struct ChannelParams
{
    std::atomic<float> trimDb { 0.0f };                          // -12 .. +12 dB
    std::atomic<float> eqHigh { 0.0f }, eqMid { 0.0f }, eqLow { 0.0f }; // -1 (kill) .. 0 .. +1 (+6 dB)
    std::atomic<float> volume { 1.0f };                          // fader de canal 0..1
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };    // nivel medido para el vúmetro
    std::atomic<bool> pfl { false };                             // pre-escucha por auriculares
};

// Mezclador de 2 canales: trim -> EQ -> fader -> crossfader -> master -> limitador
// Salidas configurables (Configuración > Audio): master y auriculares a cualquier par de salidas de la
// tarjeta, o "divisor" (master mono a la izquierda y auriculares mono a la derecha, con un cable en Y).
class MixerEngine : public juce::AudioSource
{
public:
    MixerEngine (DeckEngine& deckA, DeckEngine& deckB, SamplerEngine& sampler, Recorder& recorder);

    static constexpr int numChannels = 2;
    ChannelParams& getChannel (int index)   { return channels[(size_t) index].params; }

    std::atomic<float> crossfader { 0.5f };     // 0 = solo A, 1 = solo B
    std::atomic<float> masterVolume { 1.0f };   // 0..1
    std::atomic<float> masterPeakLeft { 0.0f }, masterPeakRight { 0.0f };
    std::atomic<int> outputChannelCount { 2 };   // salidas abiertas en la tarjeta

    // Enrutado (índice de la primera salida del par: 0 = salidas 1-2, 2 = salidas 3-4...)
    std::atomic<int> masterOut { 0 };
    std::atomic<int> phonesOut { 2 };            // -1 = sin auriculares
    std::atomic<bool> splitMono { false };       // divisor: izquierda = master, derecha = auriculares
    std::atomic<float> phonesVolume { 1.0f };    // 0..1
    std::atomic<float> cueMix { 0.0f };          // 0 = solo pre-escucha, 1 = solo master
    std::atomic<int> testTone { 0 };             // 1 = pitido por el master, 2 = por los auriculares

    bool hasPhonesOutput() const
    {
        const int n = outputChannelCount.load();
        return splitMono.load() ? n >= masterOut.load() + 2
                                : phonesOut.load() >= 0 && n >= phonesOut.load() + 2;
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    struct Channel
    {
        DeckEngine* deck = nullptr;
        ChannelParams params;
        IsolatorEQ eq;
        juce::AudioBuffer<float> buffer;
        juce::SmoothedValue<float> trimGain { 1.0f }, outGain { 1.0f };
    };

    std::array<Channel, numChannels> channels;
    SamplerEngine& sampler;
    Recorder& recorder;
    juce::SmoothedValue<float> masterGain { 1.0f };
    juce::AudioBuffer<float> cueBuffer, masterBuffer;
    juce::SmoothedValue<float> phonesGain { 1.0f };
    int toneTarget = 0, toneRemaining = 0;
    double tonePhase = 0.0, sampleRateHz = 44100.0;
    juce::dsp::Limiter<float> limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerEngine)
};
