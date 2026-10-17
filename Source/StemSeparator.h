#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Track.h"
#include <deque>

// Separación de stems (voz / instrumental / percusión)
namespace StemSeparator
{
    // Rápida (segundos): percusión por filtros de mediana (HPSS) y voz por extracción del centro estéreo
    std::unique_ptr<StemData> separateFast (const Track& track, const std::function<bool()>& shouldStop);

    // IA (minutos): Demucs v4. 'progress' recibe 0..1. Devuelve nullptr si se cancela o falla.
    std::unique_ptr<StemData> separateAI (const Track& track, const juce::File& modelFile,
                                          const std::function<bool()>& shouldStop,
                                          const std::function<void (float)>& progress);

    juce::File findModel();                                     // models\ junto al .exe
    juce::File cacheFolder();                                   // %LOCALAPPDATA%\CesarDJ\Stems
    std::unique_ptr<StemData> loadCache (const juce::File& audioFile, int numSamples);
    bool saveCache (const juce::File& audioFile, const StemData& stems, double sampleRate);
}

// Hilo de fondo que prepara stems con IA, de uno en uno:
//  - primero las canciones cargadas en los decks (se cambian en caliente al terminar)
//  - después las pedidas en lote desde el navegador (solo se guardan en caché)
class StemWorker : private juce::Thread
{
public:
    explicit StemWorker (juce::AudioFormatManager& formats);
    ~StemWorker() override;

    bool isAvailable() const                    { return modelFile.existsAsFile(); }
    void enqueue (std::shared_ptr<Track> track);    // canción de un deck (prioridad)
    void enqueueFiles (const juce::Array<juce::File>& files);
    juce::String getStatusText() const;
    int getPendingCount() const;

private:
    void run() override;

    juce::AudioFormatManager& formats;
    juce::File modelFile;
    mutable juce::CriticalSection lock;
    std::deque<std::shared_ptr<Track>> deckJobs;
    std::deque<juce::File> fileJobs;
    juce::String currentTitle;
    std::atomic<float> currentProgress { -1.0f };
    std::atomic<bool> busy { false };
};
