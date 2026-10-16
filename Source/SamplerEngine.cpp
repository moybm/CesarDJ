#include "SamplerEngine.h"

namespace
{
    constexpr double synthRate = 44100.0;
    constexpr int fadeLength = 256;
    const double twoPi = juce::MathConstants<double>::twoPi;

    // Generador de ruido determinista (los sonidos salen siempre iguales)
    struct Noise
    {
        juce::uint32 state = 22222;
        float next() { state = state * 1664525u + 1013904223u; return (float) (state >> 8) / 8388608.0f - 1.0f; }
    };

    std::vector<float> render (double seconds, const std::function<float (double t, int i)>& fn)
    {
        std::vector<float> out ((size_t) (seconds * synthRate));
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = fn ((double) i / synthRate, (int) i);
        return out;
    }

    float saw (double phase)    { return (float) (2.0 * (phase - std::floor (phase)) - 1.0); }
    float env (double t, double attack, double length)
    {
        if (t < attack) return (float) (t / attack);
        return (float) juce::jmax (0.0, 1.0 - (t - attack) / juce::jmax (1.0e-6, length - attack));
    }

    // Normaliza a -1 dB
    void normalise (std::vector<float>& v)
    {
        float peak = 1.0e-6f;
        for (auto x : v) peak = juce::jmax (peak, std::abs (x));
        const float g = 0.89f / peak;
        for (auto& x : v) x *= g;
    }
}

//==============================================================================
std::shared_ptr<SamplerEngine::Sample> SamplerEngine::makeBuiltIn (int slot, juce::String& name)
{
    Noise noise;
    std::vector<float> s;

    switch (slot)
    {
        case 0:   // Bocina (air horn): tres toques, acorde de dientes de sierra
        {
            name = "Bocina";
            const double blasts[][2] = { { 0.0, 0.17 }, { 0.22, 0.17 }, { 0.44, 0.75 } };
            s = render (1.25, [&] (double t, int)
            {
                float v = 0.0f;
                for (auto& b : blasts)
                    if (t >= b[0] && t < b[0] + b[1])
                    {
                        const double lt = t - b[0];
                        const double vib = 1.0 + 0.006 * std::sin (twoPi * 6.0 * lt);
                        const float e = (float) juce::jmin (1.0, lt / 0.012) * (float) juce::jmin (1.0, (b[0] + b[1] - t) / 0.03);
                        v += e * (saw (440.0 * vib * lt) + 0.8f * saw (554.4 * vib * lt) + 0.6f * saw (659.3 * vib * lt));
                    }
                return v;
            });
            break;
        }
        case 1:   // Sirena
        {
            name = "Sirena";
            double phase = 0.0;
            s = render (2.4, [&] (double t, int)
            {
                phase += (850.0 + 450.0 * std::sin (twoPi * 1.6 * t)) / synthRate;
                const double x = twoPi * phase;
                return env (t, 0.05, 2.4) * (float) (std::sin (x) + 0.3 * std::sin (3.0 * x));
            });
            break;
        }
        case 2:   // Láser: tres disparos con caída de tono
        {
            name = juce::String::fromUTF8 ("Láser");
            double phase = 0.0;
            s = render (0.9, [&] (double t, int)
            {
                const double lt = std::fmod (t, 0.3);
                if (lt < 1.0e-4) phase = 0.0;
                phase += 2400.0 * std::exp (-lt * 9.0) / synthRate;
                return (float) std::exp (-lt * 7.0) * (std::sin (twoPi * phase) > 0.0 ? 0.7f : -0.7f);
            });
            break;
        }
        case 3:   // Riser: ruido que sube + tono ascendente (4 s)
        {
            name = "Riser";
            double phase = 0.0;
            float lp = 0.0f;
            s = render (4.0, [&] (double t, int)
            {
                const double k = t / 4.0;
                phase += (180.0 * std::pow (12.0, k)) / synthRate;
                const float a = (float) (0.15 + 0.85 * k * k);
                lp += (noise.next() - lp) * (float) (0.02 + 0.6 * k);
                return a * (0.6f * lp + 0.35f * saw (phase));
            });
            break;
        }
        case 4:   // Bombo
        {
            name = "Bombo";
            double phase = 0.0;
            s = render (0.6, [&] (double t, int)
            {
                phase += (48.0 + 140.0 * std::exp (-t * 35.0)) / synthRate;
                const float click = t < 0.004 ? noise.next() * 0.4f : 0.0f;
                return (float) (std::sin (twoPi * phase) * std::exp (-t * 6.5)) + click;
            });
            break;
        }
        case 5:   // Palmas: tres ráfagas de ruido filtrado
        {
            name = "Palmas";
            float hp = 0.0f, prev = 0.0f;
            s = render (0.45, [&] (double t, int)
            {
                float e = 0.0f;
                for (double start : { 0.0, 0.011, 0.023 })
                    if (t >= start && t < start + 0.01)
                        e = juce::jmax (e, (float) std::exp (-(t - start) * 300.0));
                if (t >= 0.034)
                    e = juce::jmax (e, (float) std::exp (-(t - 0.034) * 16.0));
                const float n = noise.next();
                hp = 0.75f * (hp + n - prev);          // paso alto
                prev = n;
                return e * hp;
            });
            break;
        }
        case 6:   // Rebobinar: barrido descendente con ruido de cinta
        {
            name = "Rebobinar";
            double phase = 0.0;
            s = render (1.1, [&] (double t, int)
            {
                const double k = t / 1.1;
                phase += (2600.0 * std::pow (0.05, k)) / synthRate;
                const float wobble = (float) (0.6 + 0.4 * std::sin (twoPi * (18.0 - 12.0 * k) * t));
                return env (t, 0.02, 1.1) * wobble * (0.6f * saw (phase) + 0.25f * noise.next());
            });
            break;
        }
        default:  // Explosión: ruido grave que se apaga
        {
            name = juce::String::fromUTF8 ("Explosión");
            float lp1 = 0.0f, lp2 = 0.0f;
            s = render (2.2, [&] (double t, int)
            {
                const float cutoff = (float) (0.002 + 0.25 * std::exp (-t * 3.0));
                lp1 += (noise.next() - lp1) * cutoff;
                lp2 += (lp1 - lp2) * cutoff;
                return (float) std::exp (-t * 1.8) * lp2 * (t < 0.01 ? (float) (t / 0.01) : 1.0f);
            });
            break;
        }
    }

    normalise (s);

    auto sample = std::make_shared<Sample>();
    sample->sampleRate = synthRate;
    sample->audio.setSize (2, (int) s.size());
    for (int ch = 0; ch < 2; ++ch)
        sample->audio.copyFrom (ch, 0, s.data(), (int) s.size());
    return sample;
}

//==============================================================================
SamplerEngine::SamplerEngine()
{
    for (auto& p : playing)
        p = false;

    for (int i = 0; i < numSlots; ++i)
        restoreDefault (i);
}

void SamplerEngine::push (const Command& c)
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        commands[(size_t) scope.startIndex1] = c;
    else if (scope.blockSize2 > 0)
        commands[(size_t) scope.startIndex2] = c;
}

void SamplerEngine::setSample (int slot, std::shared_ptr<Sample> s, const SlotInfo& info)
{
    const bool loop = infos[(size_t) slot].loop;
    infos[(size_t) slot] = info;
    infos[(size_t) slot].loop = loop;

    // El sample anterior se libera más tarde, nunca en el hilo de audio
    retired[(size_t) slot] = std::move (owned[(size_t) slot]);
    owned[(size_t) slot] = std::move (s);
    playing[(size_t) slot] = false;
    push ({ Cmd::setSample, slot, owned[(size_t) slot].get() });
}

bool SamplerEngine::loadFile (int slot, const juce::File& file, juce::AudioFormatManager& formats)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    // Los samples son cortos: máximo 60 s
    const int length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));

    auto s = std::make_shared<Sample>();
    s->sampleRate = reader->sampleRate;
    s->audio.setSize (2, length);
    reader->read (&s->audio, 0, length, 0, true, true);
    if (reader->numChannels == 1)
        s->audio.copyFrom (1, 0, s->audio, 0, 0, length);

    setSample (slot, std::move (s), { file.getFileNameWithoutExtension(), false, file });
    return true;
}

void SamplerEngine::restoreDefault (int slot)
{
    juce::String name;
    auto s = makeBuiltIn (slot, name);
    setSample (slot, std::move (s), { name, false, {} });
}

void SamplerEngine::setLoop (int slot, bool shouldLoop)
{
    infos[(size_t) slot].loop = shouldLoop;
    push ({ Cmd::setLoop, slot, nullptr, shouldLoop });
}

void SamplerEngine::trigger (int slot)
{
    if (infos[(size_t) slot].loop && isPlaying (slot))
    {
        stop (slot);
        return;
    }

    playing[(size_t) slot] = true;
    push ({ Cmd::trigger, slot });
}

void SamplerEngine::stop (int slot)
{
    playing[(size_t) slot] = false;
    push ({ Cmd::stop, slot });
}

//==============================================================================
void SamplerEngine::prepare (double sampleRate)
{
    deviceRate = sampleRate;
    gain.reset (sampleRate, 0.03);
}

void SamplerEngine::addTo (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    // Órdenes de la interfaz
    const auto scope = fifo.read (fifo.getNumReady());
    scope.forEach ([this] (int index)
    {
        const auto& c = commands[(size_t) index];
        auto& v = voices[(size_t) c.slot];

        switch (c.type)
        {
            case Cmd::setSample: v.sample = c.sample; v.active = false; v.fadeOut = -1; break;
            case Cmd::trigger:   v.pos = 0.0; v.active = v.sample != nullptr; v.fadeOut = -1; break;
            case Cmd::stop:      if (v.active) v.fadeOut = fadeLength; break;
            case Cmd::setLoop:   v.loop = c.flag; break;
        }
    });

    const int outChannels = juce::jmin (2, out.getNumChannels());
    gain.setTargetValue (volume.load());
    const float g = gain.skip (numSamples);

    for (size_t slot = 0; slot < voices.size(); ++slot)
    {
        auto& v = voices[slot];
        if (! v.active || v.sample == nullptr)
            continue;

        const auto& audio = v.sample->audio;
        const int len = audio.getNumSamples();
        const double step = v.sample->sampleRate / deviceRate;
        const float* src[] = { audio.getReadPointer (0), audio.getReadPointer (1) };

        for (int i = 0; i < numSamples; ++i)
        {
            const int i0 = (int) v.pos;
            if (i0 + 1 >= len)
            {
                if (v.loop) { v.pos = 0.0; continue; }
                v.active = false;
                break;
            }

            float fade = 1.0f;
            if (v.fadeOut >= 0)
            {
                fade = (float) v.fadeOut / fadeLength;
                if (--v.fadeOut < 0) { v.active = false; break; }
            }

            const float frac = (float) (v.pos - i0);
            for (int ch = 0; ch < outChannels; ++ch)
            {
                const float s = src[ch][i0] + frac * (src[ch][i0 + 1] - src[ch][i0]);
                out.addSample (ch, startSample + i, s * g * fade);
            }
            v.pos += step;
        }

        if (! v.active)
            playing[slot] = false;
    }
}
