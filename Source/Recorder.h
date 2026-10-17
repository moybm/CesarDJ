#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Graba la salida master a WAV de 24 bits.
// El hilo de audio solo copia muestras a un búfer; un hilo aparte las escribe en disco.
class Recorder
{
public:
    Recorder();
    ~Recorder();

    // Carpeta por defecto: Música/CesarDJ Grabaciones
    static juce::File defaultFolder();

    bool start (const juce::File& file, double sampleRate);   // hilo de mensajes
    void stop();
    bool isRecording() const                    { return activeWriter.load() != nullptr; }
    double getSeconds() const                   { return sampleRate > 0.0 ? (double) samplesWritten.load() / sampleRate : 0.0; }
    juce::File getFile() const                  { return file; }

    void write (const float* const* channels, int numSamples);  // hilo de audio

private:
    juce::TimeSliceThread writerThread { "Grabacion" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<juce::int64> samplesWritten { 0 };
    double sampleRate = 0.0;
    juce::File file;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};
