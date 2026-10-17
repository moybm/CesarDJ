#include "StemSeparator.h"
#include "TrackAnalyser.h"
#include <juce_dsp/juce_dsp.h>
#if JUCE_MSVC
 #pragma warning (push, 0)
#endif
#include <model.hpp>
#if JUCE_MSVC
 #pragma warning (pop)
#endif
#include <thread>

namespace
{
    constexpr int aiSampleRate = 44100;                 // Demucs trabaja a 44,1 kHz
    constexpr int aiOverlap = (int) (aiSampleRate * 0.75);
    struct Cancelled {};

    // Cambia la frecuencia de muestreo de un búfer estéreo (interpolación de Lagrange)
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double fromRate, double toRate, int outLength)
    {
        juce::AudioBuffer<float> out (in.getNumChannels(), outLength);
        for (int ch = 0; ch < in.getNumChannels(); ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (fromRate / toRate, in.getReadPointer (ch), out.getWritePointer (ch), outLength);
        }
        return out;
    }

    float median (float* v, int n)
    {
        std::nth_element (v, v + n / 2, v + n);
        return v[n / 2];
    }
}

//==============================================================================
// Separación rápida
//==============================================================================
std::unique_ptr<StemData> StemSeparator::separateFast (const Track& track, const std::function<bool()>& shouldStop)
{
    constexpr int fftOrder = 11;
    constexpr int N = 1 << fftOrder;          // 2048
    constexpr int hop = N / 2;                // 50 %: la ventana Hann suma 1 al solapar
    constexpr int bins = N / 2 + 1;
    constexpr int timeMedian = 9, freqMedian = 17;

    const int n = track.numSamples();
    if (n < N * 4)
        return nullptr;

    const float* src[] = { track.audio.getReadPointer (0), track.audio.getReadPointer (1) };
    const int frames = (n + N) / hop + 1;     // con relleno al principio y al final
    auto sampleAt = [&] (int ch, int i) { return i >= 0 && i < n ? src[ch][i] : 0.0f; };

    std::vector<float> window ((size_t) N);
    for (int i = 0; i < N; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / N);

    juce::dsp::FFT fft (fftOrder);

    auto analyse = [&] (int frame, int ch, std::vector<float>& buf)
    {
        const int start = frame * hop - N;
        for (int i = 0; i < N; ++i)
            buf[(size_t) i] = sampleAt (ch, start + i) * window[(size_t) i];
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performRealOnlyForwardTransform (buf.data(), true);
    };

    // 1) Magnitud (mono) de todos los trozos: hace falta para la mediana en el tiempo
    std::vector<float> mag ((size_t) frames * bins);
    {
        std::vector<float> l ((size_t) N * 2), r ((size_t) N * 2);
        for (int f = 0; f < frames; ++f)
        {
            if ((f & 255) == 0 && shouldStop())
                return nullptr;

            analyse (f, 0, l);
            analyse (f, 1, r);
            for (int k = 0; k < bins; ++k)
            {
                const float re = l[(size_t) (2 * k)] + r[(size_t) (2 * k)];
                const float im = l[(size_t) (2 * k + 1)] + r[(size_t) (2 * k + 1)];
                mag[(size_t) f * bins + (size_t) k] = std::sqrt (re * re + im * im);
            }
        }
    }

    // 2) Máscaras y reconstrucción
    auto out = std::make_unique<StemData>();
    out->vocals.setSize (2, n);
    out->beat.setSize (2, n);
    out->vocals.clear();
    out->beat.clear();

    std::vector<float> l ((size_t) N * 2), r ((size_t) N * 2);
    std::vector<float> vl ((size_t) N * 2), vr ((size_t) N * 2), bl ((size_t) N * 2), br ((size_t) N * 2);
    float tmp[32];
    const double binHz = track.sampleRate / N;

    for (int f = 0; f < frames; ++f)
    {
        if ((f & 255) == 0 && shouldStop())
            return nullptr;

        analyse (f, 0, l);
        analyse (f, 1, r);
        const float* m = &mag[(size_t) f * bins];

        for (int k = 0; k < bins; ++k)
        {
            // Armónico = mediana en el tiempo; percusivo = mediana en frecuencia
            int c = 0;
            for (int d = -timeMedian / 2; d <= timeMedian / 2; ++d)
                tmp[c++] = mag[(size_t) juce::jlimit (0, frames - 1, f + d) * bins + (size_t) k];
            const float harm = median (tmp, c);

            c = 0;
            for (int d = -freqMedian / 2; d <= freqMedian / 2; ++d)
                tmp[c++] = m[juce::jlimit (0, bins - 1, k + d)];
            const float perc = median (tmp, c);

            const float p2 = perc * perc, h2 = harm * harm;
            const float maskBeat = p2 / (p2 + h2 + 1.0e-9f);

            // Voz: lo que está en el centro del estéreo, en la banda vocal, y no es percusivo
            const float lr = l[(size_t) (2 * k)], li = l[(size_t) (2 * k + 1)];
            const float rr = r[(size_t) (2 * k)], ri = r[(size_t) (2 * k + 1)];
            const float mid = std::hypot (lr + rr, li + ri);
            const float side = std::hypot (lr - rr, li - ri);
            const float centre = mid / (mid + side + 1.0e-9f);
            const float centreMask = juce::jlimit (0.0f, 1.0f, (centre - 0.78f) / 0.17f);
            const double hz = k * binHz;
            const float band = hz < 120.0 ? 0.0f : hz < 200.0 ? (float) ((hz - 120.0) / 80.0)
                             : hz > 9000.0 ? 0.0f : hz > 6000.0 ? (float) ((9000.0 - hz) / 3000.0) : 1.0f;
            const float maskVocal = (1.0f - maskBeat) * centreMask * centreMask * band;

            vl[(size_t) (2 * k)] = lr * maskVocal;   vl[(size_t) (2 * k + 1)] = li * maskVocal;
            vr[(size_t) (2 * k)] = rr * maskVocal;   vr[(size_t) (2 * k + 1)] = ri * maskVocal;
            bl[(size_t) (2 * k)] = lr * maskBeat;    bl[(size_t) (2 * k + 1)] = li * maskBeat;
            br[(size_t) (2 * k)] = rr * maskBeat;    br[(size_t) (2 * k + 1)] = ri * maskBeat;
        }

        for (auto* b : { &vl, &vr, &bl, &br })
            fft.performRealOnlyInverseTransform (b->data());

        const int start = f * hop - N;
        for (int i = 0; i < N; ++i)
        {
            const int idx = start + i;
            if (idx < 0 || idx >= n)
                continue;
            out->vocals.addSample (0, idx, vl[(size_t) i]);
            out->vocals.addSample (1, idx, vr[(size_t) i]);
            out->beat.addSample (0, idx, bl[(size_t) i]);
            out->beat.addSample (1, idx, br[(size_t) i]);
        }
    }

    out->ai = false;
    return out;
}

//==============================================================================
// Separación con IA (Demucs v4, demucs.cpp). Basado en el reparto en hilos de
// cli-apps/threaded_inference.hpp de demucs.cpp (MIT), con progreso y cancelación.
//==============================================================================
std::unique_ptr<StemData> StemSeparator::separateAI (const Track& track, const juce::File& modelFile,
                                                     const std::function<bool()>& shouldStop,
                                                     const std::function<void (float)>& progress)
{
    // Un modelo cargado sirve para todos los hilos (solo se lee)
    static juce::CriticalSection modelLock;
    static std::unique_ptr<demucscpp::demucs_model> model;
    {
        const juce::ScopedLock sl (modelLock);
        if (model == nullptr)
        {
            auto m = std::make_unique<demucscpp::demucs_model>();
            if (! demucscpp::load_demucs_model (modelFile.getFullPathName().toStdString(), m.get()))
                return nullptr;
            model = std::move (m);
        }
    }

    // Mezcla a 44,1 kHz
    const int n = track.numSamples();
    const bool needsResample = std::abs (track.sampleRate - aiSampleRate) > 1.0;
    const int total = needsResample ? (int) std::ceil (n * aiSampleRate / track.sampleRate) : n;
    const auto mix = needsResample ? resample (track.audio, track.sampleRate, aiSampleRate, total) : juce::AudioBuffer<float> (track.audio);

    Eigen::MatrixXf audio (2, total);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
            audio (ch, i) = mix.getSample (ch, i);

    // Hilos: núcleos libres (dejando 2 para el audio y la interfaz) y memoria (~1,5 GB por hilo)
    const int cores = juce::SystemStats::getNumPhysicalCpus();
    const int byMemory = juce::SystemStats::getMemorySizeInMegabytes() / 1500;
    const int threads = juce::jlimit (1, 8, juce::jmin (cores - 2, byMemory, total / (aiSampleRate * 10) + 1));
    const int segLen = (int) std::ceil ((double) total / threads);

    std::vector<Eigen::MatrixXf> segments;
    for (int i = 0; i < threads; ++i)
    {
        const int start = i * segLen;
        const int end = juce::jmin (total, start + segLen);
        Eigen::MatrixXf seg = Eigen::MatrixXf::Zero (2, end - start + 2 * aiOverlap);
        for (int j = 0; j < seg.cols(); ++j)
        {
            const int src = start - aiOverlap + j;
            if (src >= 0 && src < total)
                seg.col (j) = audio.col (src);
        }
        segments.push_back (std::move (seg));
    }

    std::vector<Eigen::Tensor3dXf> outs ((size_t) threads);
    std::vector<std::atomic<float>> partial ((size_t) threads);
    for (auto& p : partial) p = 0.0f;
    std::atomic<bool> failed { false };
    std::atomic<int> finished { 0 };

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i)
    {
        pool.emplace_back ([&, i]
        {
            try
            {
                outs[(size_t) i] = demucscpp::demucs_inference (*model, segments[(size_t) i],
                    [&, i] (float p, const std::string&)
                    {
                        if (shouldStop() || failed.load())
                            throw Cancelled();          // corta la inferencia en el siguiente paso
                        partial[(size_t) i] = p;
                    });
            }
            catch (...)
            {
                failed = true;
            }
            ++finished;
        });
    }

    // Progreso global mientras trabajan los hilos
    while (finished.load() < threads)
    {
        float sum = 0.0f;
        for (auto& p : partial) sum += p.load();
        progress (juce::jlimit (0.0f, 0.99f, sum / threads));
        juce::Thread::sleep (300);
    }

    for (auto& t : pool)
        t.join();

    if (failed.load() || shouldStop())
        return nullptr;

    // Unir los trozos con rampas en el solapamiento. Salidas de Demucs: 0 batería, 1 bajo, 2 resto, 3 voz
    juce::AudioBuffer<float> vocals (2, total), beat (2, total);
    vocals.clear();
    beat.clear();
    std::vector<float> weightSum ((size_t) total, 0.0f);

    for (int i = 0; i < threads; ++i)
    {
        const auto& o = outs[(size_t) i];
        const int segStart = i * segLen - aiOverlap;
        const int segCols = (int) o.dimension (2);

        for (int j = 0; j < segCols; ++j)
        {
            const int g = segStart + j;
            if (g < 0 || g >= total)
                continue;

            const float w = (float) juce::jmin (j + 1, segCols - j) / (float) aiOverlap;
            const float weight = juce::jlimit (0.001f, 1.0f, w);
            weightSum[(size_t) g] += weight;
            for (int ch = 0; ch < 2; ++ch)
            {
                vocals.addSample (ch, g, o (3, ch, j) * weight);
                beat.addSample (ch, g, o (0, ch, j) * weight);
            }
        }
    }

    for (int ch = 0; ch < 2; ++ch)
        for (int g = 0; g < total; ++g)
            if (weightSum[(size_t) g] > 0.0f)
            {
                vocals.setSample (ch, g, vocals.getSample (ch, g) / weightSum[(size_t) g]);
                beat.setSample (ch, g, beat.getSample (ch, g) / weightSum[(size_t) g]);
            }

    auto result = std::make_unique<StemData>();
    result->vocals = needsResample ? resample (vocals, aiSampleRate, track.sampleRate, n) : std::move (vocals);
    result->beat = needsResample ? resample (beat, aiSampleRate, track.sampleRate, n) : std::move (beat);
    result->ai = true;
    progress (1.0f);
    return result;
}

//==============================================================================
// Modelo y caché
//==============================================================================
juce::File StemSeparator::findModel()
{
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    return exeDir.getChildFile ("models").getChildFile ("ggml-model-htdemucs-4s-f16.bin");
}

juce::File StemSeparator::cacheFolder()
{
    return juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("CesarDJ").getChildFile ("Stems");
}

namespace
{
    // La caché se identifica por ruta + tamaño + fecha: si el archivo cambia, se recalcula
    juce::File cacheFileFor (const juce::File& audioFile)
    {
        const auto key = audioFile.getFullPathName().toLowerCase() + "|" + juce::String (audioFile.getSize())
                       + "|" + juce::String (audioFile.getLastModificationTime().toMilliseconds());
        return StemSeparator::cacheFolder().getChildFile (juce::String::toHexString (key.hashCode64()) + ".flac");
    }
}

std::unique_ptr<StemData> StemSeparator::loadCache (const juce::File& audioFile, int numSamples)
{
    const auto f = cacheFileFor (audioFile);
    if (! f.existsAsFile())
        return nullptr;

    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::FileInputStream (f), true));
    if (reader == nullptr || reader->numChannels != 4 || std::abs ((int) reader->lengthInSamples - numSamples) > 1)
        return nullptr;

    juce::AudioBuffer<float> all (4, numSamples);
    all.clear();
    reader->read (&all, 0, juce::jmin (numSamples, (int) reader->lengthInSamples), 0, true, true);

    auto s = std::make_unique<StemData>();
    s->vocals.setSize (2, numSamples);
    s->beat.setSize (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
    {
        s->vocals.copyFrom (ch, 0, all, ch, 0, numSamples);
        s->beat.copyFrom (ch, 0, all, ch + 2, 0, numSamples);
    }
    s->ai = true;
    return s;
}

bool StemSeparator::saveCache (const juce::File& audioFile, const StemData& stems, double sampleRate)
{
    const auto f = cacheFileFor (audioFile);
    f.getParentDirectory().createDirectory();
    const auto temp = f.getSiblingFile (f.getFileNameWithoutExtension() + ".tmp");
    temp.deleteFile();

    {
        juce::FlacAudioFormat flac;
        auto stream = temp.createOutputStream();
        if (stream == nullptr)
            return false;

        std::unique_ptr<juce::AudioFormatWriter> writer (flac.createWriterFor (stream.get(), sampleRate, 4, 16, {}, 0));
        if (writer == nullptr)
            return false;
        stream.release();

        const float* channels[] = { stems.vocals.getReadPointer (0), stems.vocals.getReadPointer (1),
                                    stems.beat.getReadPointer (0), stems.beat.getReadPointer (1) };
        if (! writer->writeFromFloatArrays (channels, 4, stems.vocals.getNumSamples()))
            return false;
    }

    return temp.moveFileTo (f);     // se publica entero o nada
}

//==============================================================================
// Hilo de fondo
//==============================================================================
StemWorker::StemWorker (juce::AudioFormatManager& f)
    : juce::Thread ("Stems IA"), formats (f), modelFile (StemSeparator::findModel())
{
    if (isAvailable())
        startThread (juce::Thread::Priority::low);
}

StemWorker::~StemWorker()
{
    signalThreadShouldExit();
    notify();
    stopThread (20000);
}

void StemWorker::enqueue (std::shared_ptr<Track> track)
{
    if (track == nullptr || track->hasAiStems() || ! isAvailable())
        return;

    {
        const juce::ScopedLock sl (lock);
        for (auto& t : deckJobs)
            if (t == track)
                return;
        deckJobs.push_back (std::move (track));
    }
    notify();
}

void StemWorker::enqueueFiles (const juce::Array<juce::File>& files)
{
    if (! isAvailable())
        return;
    {
        const juce::ScopedLock sl (lock);
        for (auto& f : files)
            fileJobs.push_back (f);
    }
    notify();
}

int StemWorker::getPendingCount() const
{
    const juce::ScopedLock sl (lock);
    return (int) (deckJobs.size() + fileJobs.size());
}

juce::String StemWorker::getStatusText() const
{
    if (! busy.load())
        return {};

    const juce::ScopedLock sl (lock);
    const int pending = (int) (deckJobs.size() + fileJobs.size());
    juce::String s = "STEMS IA: " + currentTitle + "  " + juce::String (juce::roundToInt (currentProgress.load() * 100.0f)) + "%";
    if (pending > 0)
        s += "  (+" + juce::String (pending) + " en cola)";
    return s;
}

void StemWorker::run()
{
    while (! threadShouldExit())
    {
        std::shared_ptr<Track> track;
        juce::File file;
        {
            const juce::ScopedLock sl (lock);
            if (! deckJobs.empty())
            {
                track = deckJobs.front();
                deckJobs.pop_front();
            }
            else if (! fileJobs.empty())
            {
                file = fileJobs.front();
                fileJobs.pop_front();
            }
        }

        if (track == nullptr && file == juce::File())
        {
            busy = false;
            wait (-1);
            continue;
        }

        auto stop = [this] { return threadShouldExit(); };

        // Canción de un deck que ya se ha descargado (solo la tiene esta cola): no merece la pena
        if (track != nullptr && track.use_count() == 1)
            continue;

        if (track == nullptr)
        {
            // Lote: si ya está en caché no hay nada que hacer; si no, se decodifica
            std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (file));
            if (r == nullptr || StemSeparator::loadCache (file, (int) r->lengthInSamples) != nullptr)
                continue;
            track = TrackAnalyser::loadTrack (formats, file, stop);
            if (track == nullptr)
                continue;
        }

        if (track->hasAiStems())
            continue;

        {
            const juce::ScopedLock sl (lock);
            currentTitle = track->title;
        }
        busy = true;
        currentProgress = 0.0f;

        auto result = StemSeparator::separateAI (*track, modelFile, stop, [this, &track] (float p)
        {
            currentProgress = p;
            track->aiProgress = p;
        });

        track->aiProgress = -1.0f;
        if (result == nullptr)
            continue;

        StemSeparator::saveCache (track->file, *result, track->sampleRate);
        track->setStems (std::move (result));       // cambio en caliente si está sonando
    }
}
