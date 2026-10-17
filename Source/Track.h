#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Un "bin" de forma de onda: amplitud de graves, medios y agudos (0..255)
struct WaveBin
{
    juce::uint8 low = 0, mid = 0, high = 0;
};

// Canción cargada en memoria + su análisis.
// El audio y la forma de onda se rellenan antes de publicar la pista; el BPM llega después (analysed = true).
struct Track
{
    static constexpr double binsPerSecond = 150.0;

    juce::File file;
    juce::String title, artist;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> audio;     // siempre estéreo
    std::vector<WaveBin> wave;

    double bpm = 0.0;                   // BPM original de la canción
    double firstBeat = 0.0;             // segundos del primer beat de la rejilla
    std::atomic<bool> analysed { false };
    std::atomic<int> camelot { -1 };    // tonalidad (ver Camelot.h), -1 = desconocida

    int numSamples() const              { return audio.getNumSamples(); }
    double getLength() const            { return audio.getNumSamples() / sampleRate; }
    bool hasBeatgrid() const            { return analysed.load() && bpm > 0.0; }
    double beatLength() const           { return bpm > 0.0 ? 60.0 / bpm : 0.0; }

    // Posición en beats (puede ser negativa antes del primer beat)
    double beatAt (double seconds) const { return (seconds - firstBeat) / beatLength(); }
    double timeOfBeat (double beat) const { return firstBeat + beat * beatLength(); }

    double nearestBeat (double seconds) const
    {
        return hasBeatgrid() ? timeOfBeat (std::round (beatAt (seconds))) : seconds;
    }
};
