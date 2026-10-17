#include "Recorder.h"

Recorder::Recorder()
{
    writerThread.startThread();
}

Recorder::~Recorder()
{
    stop();
    writerThread.stopThread (2000);
}

juce::File Recorder::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("CesarDJ Grabaciones");
}

bool Recorder::start (const juce::File& newFile, double rate)
{
    stop();

    if (rate <= 0.0 || ! newFile.getParentDirectory().createDirectory())
        return false;

    newFile.deleteFile();
    auto stream = newFile.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0));
    if (w == nullptr)
        return false;

    stream.release();       // ahora es del writer
    file = newFile;
    sampleRate = rate;
    samplesWritten = 0;

    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, 65536);

    const juce::ScopedLock sl (writerLock);
    activeWriter = writer.get();
    return true;
}

void Recorder::stop()
{
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }

    // Al destruirse, el writer vacía lo pendiente y cierra el archivo
    writer.reset();
}

void Recorder::write (const float* const* channels, int numSamples)
{
    const juce::ScopedLock sl (writerLock);

    if (auto* w = activeWriter.load())
        if (w->write (channels, numSamples))
            samplesWritten += numSamples;
}
