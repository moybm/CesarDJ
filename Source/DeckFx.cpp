#include "DeckFx.h"

juce::String DeckFx::typeName (int type)
{
    switch (type)
    {
        case flanger:    return "FLANGER";
        case cut:        return "CORTAR";
        case echo:       return "ECO";
        case reverb:     return "REVERB";
        case bitcrusher: return "BITCRUSH";
        default:         return {};
    }
}

void DeckFx::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    for (auto* f : { &lowpass, &highpass })
    {
        f->prepare ({ sampleRate, (juce::uint32) maxBlockSize, 2 });
        f->setResonance (1.1f);
    }
    lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    filterAmount.reset (sampleRate, 0.03);
    flangerAmount.reset (sampleRate, 0.03);
    echoAmount.reset (sampleRate, 0.03);
    echoDelay.reset (sampleRate, 0.25);     // cambios de tempo suaves, sin saltos en el eco
    reverbAmount.reset (sampleRate, 0.05);

    const auto echoSize = (size_t) (sampleRate * 4.0);
    echoL.assign (echoSize, 0.0f);
    echoR.assign (echoSize, 0.0f);

    reverbFx.setSampleRate (sampleRate);
    reset();
}

void DeckFx::reset()
{
    lowpass.reset();
    highpass.reset();
    for (auto& ch : flangerBuffer)
        ch.fill (0.0f);
    flangerWrite = 0;
    cutGain = 1.0f;
    std::fill (echoL.begin(), echoL.end(), 0.0f);
    std::fill (echoR.begin(), echoR.end(), 0.0f);
    echoWrite = 0;
    reverbFx.reset();
    reverbTail = 0;
    crushCounter = 0;
}

void DeckFx::process (float* left, float* right, int numSamples,
                      float filter, int type2, float amount2, int type3, float amount3,
                      double beatPhase, double beatIncrement)
{
    processFilter (left, right, numSamples, filter);
    processSlot (type2, left, right, numSamples, amount2, beatPhase, beatIncrement);
    if (type3 != type2)
        processSlot (type3, left, right, numSamples, amount3, beatPhase, beatIncrement);
}

void DeckFx::processSlot (int type, float* left, float* right, int numSamples, float amount,
                          double beatPhase, double beatIncrement)
{
    switch (type)
    {
        case flanger:    processFlanger (left, right, numSamples, amount); break;
        case cut:        processCut (left, right, numSamples, amount, beatPhase, beatIncrement); break;
        case echo:       processEcho (left, right, numSamples, amount, beatIncrement); break;
        case reverb:     processReverb (left, right, numSamples, amount); break;
        case bitcrusher: processBitcrusher (left, right, numSamples, amount); break;
        default: break;
    }
}

//==============================================================================
void DeckFx::processFilter (float* left, float* right, int numSamples, float amount)
{
    filterAmount.setTargetValue (amount);

    if (! filterAmount.isSmoothing() && std::abs (amount) < 0.02f)
    {
        filterAmount.setCurrentAndTargetValue (amount);
        return;     // sin efecto: no gastar CPU
    }

    constexpr int subBlock = 32;
    for (int start = 0; start < numSamples; start += subBlock)
    {
        const int num = juce::jmin (subBlock, numSamples - start);
        const float v = filterAmount.skip (num);

        // Barrido exponencial de la frecuencia de corte
        const float lpCutoff = v < 0.0f ? 20000.0f * std::pow (0.005f, -v) : 20000.0f;
        const float hpCutoff = v > 0.0f ? 20.0f * std::pow (400.0f, v) : 20.0f;
        lowpass.setCutoffFrequency (juce::jmin (lpCutoff, (float) sampleRate * 0.45f));
        highpass.setCutoffFrequency (hpCutoff);

        for (int i = start; i < start + num; ++i)
        {
            left[i]  = highpass.processSample (0, lowpass.processSample (0, left[i]));
            right[i] = highpass.processSample (1, lowpass.processSample (1, right[i]));
        }
    }
}

void DeckFx::processFlanger (float* left, float* right, int numSamples, float amount)
{
    flangerAmount.setTargetValue (amount);
    float* channels[] = { left, right };

    for (int i = 0; i < numSamples; ++i)
    {
        const float a = flangerAmount.getNextValue();

        lfoPhase += 0.18 / sampleRate;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        // Retardo entre 0.6 y 5 ms; el canal derecho va desfasado para dar amplitud estéreo
        for (int ch = 0; ch < 2; ++ch)
        {
            const double phase = lfoPhase + (ch == 0 ? 0.0 : 0.25);
            const double lfo = 0.5 + 0.5 * std::sin (juce::MathConstants<double>::twoPi * phase);
            const double delay = (0.0006 + 0.0044 * lfo) * sampleRate;

            double readPos = flangerWrite - delay;
            if (readPos < 0.0)
                readPos += flangerBufferSize;
            const int i0 = (int) readPos;
            const int i1 = (i0 + 1) % flangerBufferSize;
            const float frac = (float) (readPos - i0);
            auto& buf = flangerBuffer[(size_t) ch];
            const float delayed = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

            const float dry = channels[ch][i];
            buf[(size_t) flangerWrite] = dry + delayed * 0.7f * a;
            channels[ch][i] = (dry + delayed * a) / (1.0f + 0.5f * a);
        }

        flangerWrite = (flangerWrite + 1) % flangerBufferSize;
    }
}

void DeckFx::processCut (float* left, float* right, int numSamples, float amount, double beatPhase, double beatIncrement)
{
    if (amount < 0.02f || beatIncrement <= 0.0)
    {
        // Volver a abrir suavemente si se acaba de desactivar
        for (int i = 0; i < numSamples && cutGain < 1.0f; ++i)
        {
            cutGain = juce::jmin (1.0f, cutGain + 0.01f);
            left[i] *= cutGain;
            right[i] *= cutGain;
        }
        return;
    }

    const double division = amount < 0.34f ? 0.5 : (amount < 0.67f ? 0.25 : 0.125);
    const float smoothing = 1.0f - std::exp (-1.0f / (0.0015f * (float) sampleRate));   // ~1.5 ms

    for (int i = 0; i < numSamples; ++i)
    {
        const double beat = beatPhase + beatIncrement * i;
        double frac = std::fmod (beat / division, 1.0);
        if (frac < 0.0)
            frac += 1.0;

        const float target = frac < 0.5 ? 1.0f : 0.0f;
        cutGain += (target - cutGain) * smoothing;
        left[i] *= cutGain;
        right[i] *= cutGain;
    }
}

// Eco de 3/4 de beat (o 375 ms sin beatgrid). El mando sube a la vez la mezcla y la realimentación.
void DeckFx::processEcho (float* left, float* right, int numSamples, float amount, double beatIncrement)
{
    echoAmount.setTargetValue (amount);
    if (! echoAmount.isSmoothing() && amount < 0.01f)
    {
        // Las repeticiones se apagan solas aunque el mando esté a 0
        bool silent = true;
        for (size_t i = 0; i < echoL.size() && silent; i += 997)
            silent = std::abs (echoL[i]) < 1.0e-4f;
        if (silent)
            return;
    }

    const double delaySeconds = beatIncrement > 0.0 ? 0.75 / (beatIncrement * sampleRate) : 0.375;
    echoDelay.setTargetValue ((float) juce::jlimit (0.01, 3.9, delaySeconds) * (float) sampleRate);

    const int size = (int) echoL.size();
    for (int i = 0; i < numSamples; ++i)
    {
        const float a = echoAmount.getNextValue();
        const float delay = echoDelay.getNextValue();
        const float feedback = 0.25f + 0.45f * a;

        float readPos = (float) echoWrite - delay;
        if (readPos < 0.0f)
            readPos += (float) size;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - (float) i0;

        const float dl = echoL[(size_t) i0] + frac * (echoL[(size_t) i1] - echoL[(size_t) i0]);
        const float dr = echoR[(size_t) i0] + frac * (echoR[(size_t) i1] - echoR[(size_t) i0]);

        // Ping-pong: cada repetición cambia de lado
        echoL[(size_t) echoWrite] = left[i] * a + dr * feedback;
        echoR[(size_t) echoWrite] = right[i] * a + dl * feedback;
        left[i] += dl * 0.8f;
        right[i] += dr * 0.8f;

        echoWrite = (echoWrite + 1) % size;
    }
}

void DeckFx::processReverb (float* left, float* right, int numSamples, float amount)
{
    reverbAmount.setTargetValue (amount);

    if (amount >= 0.01f)
        reverbTail = (int) (sampleRate * 4.0);
    else if (reverbTail <= 0 && ! reverbAmount.isSmoothing())
        return;

    reverbTail -= numSamples;

    const float a = reverbAmount.skip (numSamples);
    juce::Reverb::Parameters p;
    p.roomSize = 0.55f + 0.4f * a;
    p.damping = 0.45f;
    p.wetLevel = 0.45f * a;
    p.dryLevel = 1.0f - 0.25f * a;
    p.width = 1.0f;
    reverbFx.setParameters (p);
    reverbFx.processStereo (left, right, numSamples);
}

// Reduce bits (16 -> 4) y frecuencia de muestreo (hasta 1/12) a la vez
void DeckFx::processBitcrusher (float* left, float* right, int numSamples, float amount)
{
    if (amount < 0.02f)
        return;

    const float bits = 16.0f - 12.0f * amount;
    const float levels = std::pow (2.0f, bits) * 0.5f;
    const int hold = 1 + (int) (amount * 11.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        if (crushCounter++ % hold == 0)
        {
            crushHoldL = std::round (left[i] * levels) / levels;
            crushHoldR = std::round (right[i] * levels) / levels;
        }

        left[i]  = left[i]  + (crushHoldL - left[i])  * amount;
        right[i] = right[i] + (crushHoldR - right[i]) * amount;
    }
}
