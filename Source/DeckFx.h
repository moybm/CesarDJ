#pragma once

#include <juce_dsp/juce_dsp.h>

// Efectos de un deck, en serie: FILTRO (fijo) -> hueco 2 -> hueco 3
// Los huecos 2 y 3 pueden llevar cualquiera de los efectos de 'Type'.
//  - Filtro:  -1 = paso bajo cerrado, 0 = sin efecto, +1 = paso alto cerrado
//  - Resto:   0 = sin efecto, 1 = máximo
class DeckFx
{
public:
    enum Type { flanger = 0, cut, echo, reverb, bitcrusher, numTypes };
    static juce::String typeName (int type);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // beatPhase: posición en beats al inicio del bloque; beatIncrement: beats por muestra (0 si no hay beatgrid)
    void process (float* left, float* right, int numSamples,
                  float filter, int type2, float amount2, int type3, float amount3,
                  double beatPhase, double beatIncrement);

private:
    void processSlot (int type, float* left, float* right, int numSamples, float amount,
                      double beatPhase, double beatIncrement);
    void processFilter (float* left, float* right, int numSamples, float amount);
    void processFlanger (float* left, float* right, int numSamples, float amount);
    void processCut (float* left, float* right, int numSamples, float amount, double beatPhase, double beatIncrement);
    void processEcho (float* left, float* right, int numSamples, float amount, double beatIncrement);
    void processReverb (float* left, float* right, int numSamples, float amount);
    void processBitcrusher (float* left, float* right, int numSamples, float amount);

    double sampleRate = 44100.0;

    juce::dsp::StateVariableTPTFilter<float> lowpass, highpass;
    juce::SmoothedValue<float> filterAmount;

    static constexpr int flangerBufferSize = 4096;
    std::array<std::array<float, flangerBufferSize>, 2> flangerBuffer {};
    int flangerWrite = 0;
    double lfoPhase = 0.0;
    juce::SmoothedValue<float> flangerAmount;

    float cutGain = 1.0f;

    std::vector<float> echoL, echoR;
    int echoWrite = 0;
    juce::SmoothedValue<float> echoAmount, echoDelay;

    juce::Reverb reverbFx;
    juce::SmoothedValue<float> reverbAmount;
    int reverbTail = 0;             // muestras que se sigue procesando tras bajar el mando (cola)

    float crushHoldL = 0.0f, crushHoldR = 0.0f;
    int crushCounter = 0;
};
