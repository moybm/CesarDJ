#include "MixerEngine.h"

namespace
{
    constexpr double smoothingSeconds = 0.02;   // evita "clics" al mover mandos

    // Mando de EQ: -1 = banda eliminada, 0 = plano, +1 = +6 dB
    float eqKnobToGain (float v)
    {
        return v >= 0.0f ? juce::Decibels::decibelsToGain (v * 6.0f)
                         : (1.0f + v) * (1.0f + v);
    }

    void storePeak (std::atomic<float>& peak, float value)
    {
        if (value > peak.load())
            peak.store (value);
    }
}

//==============================================================================
void IsolatorEQ::prepare (double sampleRate, int blockSize, int numChannels)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };

    lowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowSplit.setCutoffFrequency (300.0f);
    highSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highSplit.setCutoffFrequency (4000.0f);
    // Compensa la fase de los graves para que con todo a 0 la suma sea plana
    lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    lowAllpass.setCutoffFrequency (4000.0f);

    for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
    {
        f->prepare (spec);
        f->reset();
    }

    for (auto* g : { &lowGain, &midGain, &highGain })
        g->reset (sampleRate, smoothingSeconds);
}

void IsolatorEQ::setGains (float low, float mid, float high)
{
    lowGain.setTargetValue (low);
    midGain.setTargetValue (mid);
    highGain.setTargetValue (high);
}

void IsolatorEQ::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int numCh = buffer.getNumChannels();
    auto* const* data = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gl = lowGain.getNextValue();
        const float gm = midGain.getNextValue();
        const float gh = highGain.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float low, rest, mid, high;
            lowSplit.processSample (ch, data[ch][i], low, rest);
            highSplit.processSample (ch, rest, mid, high);
            low = lowAllpass.processSample (ch, low);

            data[ch][i] = gl * low + gm * mid + gh * high;
        }
    }
}

//==============================================================================
MixerEngine::MixerEngine (DeckEngine& deckA, DeckEngine& deckB)
{
    channels[0].deck = &deckA;
    channels[1].deck = &deckB;
}

void MixerEngine::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    const int maxBlock = juce::jmax (samplesPerBlockExpected, 8192);

    for (auto& c : channels)
    {
        c.deck->prepareToPlay (samplesPerBlockExpected, sampleRate);
        c.buffer.setSize (2, maxBlock);
        c.eq.prepare (sampleRate, maxBlock, 2);
        c.trimGain.reset (sampleRate, smoothingSeconds);
        c.outGain.reset (sampleRate, smoothingSeconds);
    }

    masterGain.reset (sampleRate, smoothingSeconds);
    cueBuffer.setSize (2, maxBlock);

    limiter.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    limiter.setThreshold (-0.3f);
    limiter.setRelease (100.0f);
    limiter.reset();
}

void MixerEngine::releaseResources()
{
    for (auto& c : channels)
        c.deck->releaseResources();
}

void MixerEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto& out = *bufferToFill.buffer;
    const int start = bufferToFill.startSample;
    const int n = bufferToFill.numSamples;
    const int outChannels = juce::jmin (2, out.getNumChannels());
    const bool hasCueOutput = out.getNumChannels() >= 4;
    outputChannelCount = out.getNumChannels();

    bufferToFill.clearActiveBufferRegion();
    cueBuffer.setSize (2, n, false, false, true);
    cueBuffer.clear();
    bool anyPfl = false;

    // Crossfader de potencia constante: en el centro ambos canales suenan igual sin bajón de volumen
    const float x = crossfader.load();
    const float xfade[numChannels] = { std::cos (x * juce::MathConstants<float>::halfPi),
                                       std::sin (x * juce::MathConstants<float>::halfPi) };

    for (size_t idx = 0; idx < channels.size(); ++idx)
    {
        auto& c = channels[idx];
        auto& p = c.params;

        c.buffer.setSize (2, n, false, false, true);
        c.deck->getNextAudioBlock (juce::AudioSourceChannelInfo (&c.buffer, 0, n));

        c.trimGain.setTargetValue (juce::Decibels::decibelsToGain (p.trimDb.load()));
        c.trimGain.applyGain (c.buffer, n);

        c.eq.setGains (eqKnobToGain (p.eqLow.load()), eqKnobToGain (p.eqMid.load()), eqKnobToGain (p.eqHigh.load()));
        c.eq.process (c.buffer, n);

        // Vúmetro de canal antes del fader (útil para ajustar la ganancia)
        storePeak (p.peakLeft,  c.buffer.getMagnitude (0, 0, n));
        storePeak (p.peakRight, c.buffer.getMagnitude (1, 0, n));

        if (hasCueOutput && p.pfl.load())
        {
            anyPfl = true;
            for (int ch = 0; ch < 2; ++ch)
                cueBuffer.addFrom (ch, 0, c.buffer, ch, 0, n);
        }

        const float vol = p.volume.load();
        c.outGain.setTargetValue (vol * vol * xfade[idx]);
        c.outGain.applyGain (c.buffer, n);

        for (int ch = 0; ch < outChannels; ++ch)
            out.addFrom (ch, start, c.buffer, ch, 0, n);
    }

    const float mv = masterVolume.load();
    masterGain.setTargetValue (mv * mv);
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        for (int ch = 0; ch < outChannels; ++ch)
            out.getWritePointer (ch, start)[i] *= g;
    }

    // Limitador final: evita que la salida sature
    if (outChannels == 2)
    {
        juce::dsp::AudioBlock<float> block (out.getArrayOfWritePointers(), 2, (size_t) start, (size_t) n);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // Auriculares: los canales en PFL, o el master si no hay ninguno
    if (hasCueOutput)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (anyPfl)
                out.copyFrom (2 + ch, start, cueBuffer, ch, 0, n);
            else
                out.copyFrom (2 + ch, start, out, juce::jmin (ch, outChannels - 1), start, n);
        }
    }

    storePeak (masterPeakLeft, out.getMagnitude (0, start, n));
    storePeak (masterPeakRight, out.getMagnitude (outChannels - 1, start, n));
}
