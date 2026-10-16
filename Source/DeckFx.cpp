#include "DeckFx.h"

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
}

void DeckFx::process (float* left, float* right, int numSamples,
                      float filter, float flanger, float cut,
                      double beatPhase, double beatIncrement)
{
    processFilter (left, right, numSamples, filter);
    processFlanger (left, right, numSamples, flanger);
    processCut (left, right, numSamples, cut, beatPhase, beatIncrement);
}

void DeckFx::processFilter (float* left, float* right, int numSamples, float amount)
{
    filterAmount.setTargetValue (amount);

    if (! filterAmount.isSmoothing() && std::abs (amount) < 0.02f)
    {
        filterAmount.setCurrentAndTargetValue (amount);
        return;     // sin efecto: no gastar CPU (los filtros conservan su estado)
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
