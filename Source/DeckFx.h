#pragma once

#include <juce_dsp/juce_dsp.h>

// Efectos de un deck, en serie: FILTRO -> FLANGER -> CORTAR
//  - Filtro:  -1 = paso bajo cerrado, 0 = sin efecto, +1 = paso alto cerrado
//  - Flanger: 0 = sin efecto, 1 = máximo
//  - Cortar:  0 = sin efecto; el valor elige la división (1/2, 1/4, 1/8 de beat), sincronizado con el beatgrid
class DeckFx
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // beatPhase: posición en beats al inicio del bloque; beatIncrement: beats por muestra (0 si no hay beatgrid)
    void process (float* left, float* right, int numSamples,
                  float filter, float flanger, float cut,
                  double beatPhase, double beatIncrement);

private:
    void processFilter (float* left, float* right, int numSamples, float amount);
    void processFlanger (float* left, float* right, int numSamples, float amount);
    void processCut (float* left, float* right, int numSamples, float amount, double beatPhase, double beatIncrement);

    double sampleRate = 44100.0;

    juce::dsp::StateVariableTPTFilter<float> lowpass, highpass;
    juce::SmoothedValue<float> filterAmount;

    static constexpr int flangerBufferSize = 4096;
    std::array<std::array<float, flangerBufferSize>, 2> flangerBuffer {};
    int flangerWrite = 0;
    double lfoPhase = 0.0;
    juce::SmoothedValue<float> flangerAmount;

    float cutGain = 1.0f;
};
