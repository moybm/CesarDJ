#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DeckEngine.h"

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
// Salidas 1-2: master. Salidas 3-4 (si la tarjeta las tiene): auriculares con los canales en PFL.
class MixerEngine : public juce::AudioSource
{
public:
    MixerEngine (DeckEngine& deckA, DeckEngine& deckB);

    static constexpr int numChannels = 2;
    ChannelParams& getChannel (int index)   { return channels[(size_t) index].params; }

    std::atomic<float> crossfader { 0.5f };     // 0 = solo A, 1 = solo B
    std::atomic<float> masterVolume { 1.0f };   // 0..1
    std::atomic<float> masterPeakLeft { 0.0f }, masterPeakRight { 0.0f };
    std::atomic<int> outputChannelCount { 2 };   // 4 = hay salida de auriculares

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
    juce::SmoothedValue<float> masterGain { 1.0f };
    juce::AudioBuffer<float> cueBuffer;
    juce::dsp::Limiter<float> limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerEngine)
};
