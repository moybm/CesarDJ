#include "TrackAnalyser.h"
#include <juce_dsp/juce_dsp.h>
#include "TagReader.h"
#include "Camelot.h"

namespace
{
    constexpr double maxTrackMinutes = 45.0;

    // Filtro de un polo, suficiente para separar bandas en la forma de onda
    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setCutoff (double hz, double sr)  { a = (float) std::exp (-juce::MathConstants<double>::twoPi * hz / sr); }
        float lowpass (float x)                 { z = x + a * (z - x); return z; }
    };

    void computeWaveform (Track& t)
    {
        const int n = t.numSamples();
        const int samplesPerBin = juce::jmax (1, (int) std::round (t.sampleRate / Track::binsPerSecond));
        const size_t numBins = (size_t) (n / samplesPerBin + 1);

        std::vector<float> lo (numBins), mi (numBins), hi (numBins);

        OnePole lowLp, lowLp2, highLp;
        lowLp.setCutoff (220.0, t.sampleRate);
        lowLp2.setCutoff (220.0, t.sampleRate);
        highLp.setCutoff (2800.0, t.sampleRate);

        const float* L = t.audio.getReadPointer (0);
        const float* R = t.audio.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (L[i] + R[i]);
            const float low = lowLp2.lowpass (lowLp.lowpass (x));
            const float belowHigh = highLp.lowpass (x);
            const float high = x - belowHigh;
            const float mid = belowHigh - low;

            const size_t b = (size_t) (i / samplesPerBin);
            lo[b] = juce::jmax (lo[b], std::abs (low));
            mi[b] = juce::jmax (mi[b], std::abs (mid));
            hi[b] = juce::jmax (hi[b], std::abs (high));
        }

        float peak = 1.0e-6f;
        for (size_t b = 0; b < numBins; ++b)
            peak = juce::jmax (peak, lo[b], mi[b], hi[b]);

        t.wave.resize (numBins);
        const float scale = 255.0f / peak;
        for (size_t b = 0; b < numBins; ++b)
        {
            t.wave[b].low  = (juce::uint8) juce::jlimit (0.0f, 255.0f, lo[b] * scale);
            t.wave[b].mid  = (juce::uint8) juce::jlimit (0.0f, 255.0f, mi[b] * scale * 1.15f);
            t.wave[b].high = (juce::uint8) juce::jlimit (0.0f, 255.0f, hi[b] * scale * 1.4f);
        }
    }

    float interp (const std::vector<float>& v, double i)
    {
        const auto i0 = (size_t) i;
        if (i0 + 1 >= v.size())
            return 0.0f;
        const float f = (float) (i - (double) i0);
        return v[i0] + f * (v[i0 + 1] - v[i0]);
    }

    // Puntuación media de la envolvente de ataques en las posiciones de una rejilla
    float gridScore (const std::vector<float>& env, double period, double phase)
    {
        float sum = 0.0f;
        int count = 0;
        for (double p = phase; p < (double) env.size() - 1; p += period)
        {
            sum += interp (env, p);
            ++count;
        }
        return count > 0 ? sum / (float) count : 0.0f;
    }

    // Mejor fase para un periodo dado (en frames de la envolvente)
    double bestPhase (const std::vector<float>& env, double period, double step, float& scoreOut)
    {
        double best = 0.0;
        scoreOut = -1.0f;
        for (double ph = 0.0; ph < period; ph += step)
        {
            const float s = gridScore (env, period, ph);
            if (s > scoreOut)
            {
                scoreOut = s;
                best = ph;
            }
        }
        return best;
    }
}

//==============================================================================
std::shared_ptr<Track> TrackAnalyser::loadTrack (juce::AudioFormatManager& formats, const juce::File& file,
                                                 const std::function<bool()>& shouldStop)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    const auto maxSamples = (juce::int64) (maxTrackMinutes * 60.0 * reader->sampleRate);
    const int length = (int) juce::jmin (reader->lengthInSamples, maxSamples);

    auto track = std::make_shared<Track>();
    track->file = file;
    track->sampleRate = reader->sampleRate;
    const auto tags = TagReader::read (file);
    track->title = tags.title;
    track->artist = tags.artist;

    track->audio.setSize (2, length);

    // Lectura por trozos para poder cancelar
    constexpr int chunk = 1 << 18;
    for (int start = 0; start < length; start += chunk)
    {
        if (shouldStop())
            return nullptr;

        const int num = juce::jmin (chunk, length - start);
        reader->read (&track->audio, start, num, start, true, true);
    }

    if (reader->numChannels == 1)
        track->audio.copyFrom (1, 0, track->audio, 0, 0, length);

    computeWaveform (*track);
    return track;
}

//==============================================================================
void TrackAnalyser::detectBeatgrid (Track& track, const std::function<bool()>& shouldStop)
{
    // 1) Envolvente de ataques ("spectral flux") a partir de FFTs de 1024 muestras
    constexpr int fftOrder = 10;
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hop = 512;

    const int n = track.numSamples();
    if (n < fftSize * 8)
        return;

    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);

    const int numFrames = (n - fftSize) / hop;
    std::vector<float> env ((size_t) numFrames, 0.0f);
    std::vector<float> buffer ((size_t) fftSize * 2), prevMag ((size_t) fftSize / 2, 0.0f);

    const float* L = track.audio.getReadPointer (0);
    const float* R = track.audio.getReadPointer (1);

    // Solo hasta ~6 kHz: ahí están bombo, caja y hi-hats
    const int maxBin = juce::jmin (fftSize / 2, (int) (6000.0 / track.sampleRate * fftSize));

    for (int f = 0; f < numFrames; ++f)
    {
        if ((f & 1023) == 0 && shouldStop())
            return;

        const int offset = f * hop;
        for (int i = 0; i < fftSize; ++i)
            buffer[(size_t) i] = 0.5f * (L[offset + i] + R[offset + i]);

        window.multiplyWithWindowingTable (buffer.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (buffer.data(), true);

        float flux = 0.0f;
        for (int b = 1; b < maxBin; ++b)
        {
            const float mag = std::log1p (100.0f * buffer[(size_t) b]);
            const float weight = b < maxBin / 8 ? 1.5f : 1.0f;   // más peso a los graves
            flux += weight * juce::jmax (0.0f, mag - prevMag[(size_t) b]);
            prevMag[(size_t) b] = mag;
        }
        env[(size_t) f] = flux;
    }

    // Quitar la tendencia: restar la media local y quedarse con los picos
    {
        std::vector<float> smoothed (env.size());
        constexpr int radius = 16;
        double acc = 0.0;
        for (int i = 0; i < numFrames + radius; ++i)
        {
            if (i < numFrames)            acc += env[(size_t) i];
            if (i - 2 * radius - 1 >= 0)  acc -= env[(size_t) (i - 2 * radius - 1)];
            const int centre = i - radius;
            if (centre >= 0 && centre < numFrames)
                smoothed[(size_t) centre] = (float) (acc / (2 * radius + 1));
        }
        for (size_t i = 0; i < env.size(); ++i)
            env[i] = juce::jmax (0.0f, env[i] - smoothed[i]);
    }

    const double envRate = track.sampleRate / hop;

    // 2) Autocorrelación para una primera estimación del tempo (60..200 BPM),
    //    con preferencia por tempos "bailables" alrededor de 120
    const int minLag = (int) std::floor (envRate * 60.0 / 200.0);
    const int maxLag = (int) std::ceil (envRate * 60.0 / 60.0);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);

    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i + lag < numFrames; ++i)
            sum += env[(size_t) i] * env[(size_t) (i + lag)];
        ac[(size_t) lag] = sum / (numFrames - lag);
    }

    double bestLag = 0.0, bestValue = -1.0;
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
    {
        const double bpm = 60.0 * envRate / lag;
        const double octaves = std::log2 (bpm / 120.0);
        const double weight = std::exp (-0.5 * (octaves / 0.9) * (octaves / 0.9));
        const double v = ac[(size_t) lag] * weight;

        if (v > bestValue && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1])
        {
            bestValue = v;
            // Interpolación parabólica para afinar el pico
            const double a = ac[(size_t) lag - 1], b = ac[(size_t) lag], c = ac[(size_t) lag + 1];
            const double denom = a - 2.0 * b + c;
            bestLag = lag + (std::abs (denom) > 1.0e-12 ? 0.5 * (a - c) / denom : 0.0);
        }
    }

    if (bestLag <= 0.0)
        return;

    double bpm = 60.0 * envRate / bestLag;
    while (bpm < 85.0)   bpm *= 2.0;
    while (bpm >= 175.0) bpm /= 2.0;

    // 3) Búsqueda fina: el tempo (y su fase) cuya rejilla cae mejor sobre los golpes en toda la canción
    double bestBpm = bpm;
    float bestScore = -1.0f;
    for (double cand = bpm - 2.0; cand <= bpm + 2.0; cand += 0.01)
    {
        if (shouldStop())
            return;

        float score;
        bestPhase (env, 60.0 * envRate / cand, 1.0, score);
        if (score > bestScore)
        {
            bestScore = score;
            bestBpm = cand;
        }
    }

    // La mayoría de la música electrónica tiene BPM entero: si la rejilla entera encaja casi igual de bien, usarla
    const double rounded = std::round (bestBpm);
    if (std::abs (bestBpm - rounded) < 0.25)
    {
        float roundedScore;
        bestPhase (env, 60.0 * envRate / rounded, 0.5, roundedScore);
        if (roundedScore >= bestScore * 0.97f)
            bestBpm = rounded;
    }

    float score;
    const double period = 60.0 * envRate / bestBpm;
    const double phaseFrames = bestPhase (env, period, 0.05, score);

    // Frame -> segundos (centro de la ventana de la FFT)
    const double beatLen = 60.0 / bestBpm;
    double first = (phaseFrames * hop + fftSize * 0.5) / track.sampleRate;
    first = std::fmod (first, beatLen);

    track.bpm = bestBpm;
    track.firstBeat = first;
}

//==============================================================================
int TrackAnalyser::detectKey (const Track& track, const std::function<bool()>& shouldStop)
{
    // 1) Mono y a 1/4 de la frecuencia de muestreo (basta para las notas que definen la tonalidad)
    constexpr int decimation = 4;
    const int n = track.numSamples() / decimation;
    const double sr = track.sampleRate / decimation;
    if (n < 16384)
        return -1;

    std::vector<float> mono ((size_t) n);
    const float* L = track.audio.getReadPointer (0);
    const float* R = track.audio.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int k = 0; k < decimation; ++k)
            s += L[i * decimation + k] + R[i * decimation + k];
        mono[(size_t) i] = s / (2.0f * decimation);
    }

    // 2) Cromagrama: energía de cada una de las 12 notas entre La1 (55 Hz) y La6 (1760 Hz)
    constexpr int fftOrder = 13;
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hop = fftSize / 2;

    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> buffer ((size_t) fftSize * 2);

    // A qué nota pertenece cada bin (precalculado)
    std::vector<int> binNote ((size_t) fftSize / 2, -1);
    for (int b = 1; b < fftSize / 2; ++b)
    {
        const double f = b * sr / fftSize;
        if (f >= 55.0 && f <= 1760.0)
            binNote[(size_t) b] = ((int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0)) % 12 + 12) % 12;
    }

    std::array<double, 12> chroma {};
    for (int start = 0; start + fftSize < n; start += hop)
    {
        if (shouldStop())
            return -1;

        std::copy (mono.begin() + start, mono.begin() + start + fftSize, buffer.begin());
        window.multiplyWithWindowingTable (buffer.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (buffer.data(), true);

        std::array<double, 12> frame {};
        double total = 0.0;
        for (int b = 1; b < fftSize / 2; ++b)
            if (binNote[(size_t) b] >= 0)
            {
                const double e = buffer[(size_t) b];      // lineal: los armónicos pesan menos que las fundamentales
                frame[(size_t) binNote[(size_t) b]] += e;
                total += e;
            }

        // Cada trozo pesa igual: los pasajes fuertes no dominan
        if (total > 1.0e-6)
            for (int k = 0; k < 12; ++k)
                chroma[(size_t) k] += frame[(size_t) k] / total;
    }

    // 3) Correlación con los perfiles mayor y menor de Krumhansl-Kessler en las 12 tónicas
    static const double majorProfile[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minorProfile[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    auto correlation = [&chroma] (const double* profile, int root)
    {
        double mc = 0.0, mp = 0.0;
        for (int k = 0; k < 12; ++k) { mc += chroma[(size_t) k]; mp += profile[k]; }
        mc /= 12.0; mp /= 12.0;

        double num = 0.0, dc = 0.0, dp = 0.0;
        for (int k = 0; k < 12; ++k)
        {
            const double c = chroma[(size_t) ((k + root) % 12)] - mc;
            const double p = profile[k] - mp;
            num += c * p; dc += c * c; dp += p * p;
        }
        return dc > 0.0 && dp > 0.0 ? num / std::sqrt (dc * dp) : 0.0;
    };

    int best = -1;
    double bestScore = 0.0;
    for (int root = 0; root < 12; ++root)
        for (int minor = 0; minor < 2; ++minor)
        {
            const double score = correlation (minor ? minorProfile : majorProfile, root);
            if (score > bestScore)
            {
                bestScore = score;
                best = Camelot::fromRoot (root, minor == 1);
            }
        }

    return bestScore > 0.3 ? best : -1;
}
