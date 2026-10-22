#include "FeatureRuntime.h"
#include "Features.h"

namespace
{
    constexpr int ticksPerSecond = 5;

    juce::String formatTime (double seconds, bool withHours)
    {
        const int s = juce::jmax (0, (int) seconds);
        if (withHours)
            return juce::String (s / 3600) + ":" + juce::String ((s / 60) % 60).paddedLeft ('0', 2) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
        return juce::String (s / 60).paddedLeft ('0', 2) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    // Sonoridad aproximada (estilo EBU R128 sin filtro K): bloques de 400 ms con puerta absoluta y relativa
    float measureLoudness (const Track& t)
    {
        const auto& audio = t.audio;
        const int block = juce::jmax (1, (int) (t.sampleRate * 0.4));
        const int numBlocks = audio.getNumSamples() / block;
        if (numBlocks < 1 || audio.getNumChannels() < 2)
            return -100.0f;

        std::vector<double> energies;
        energies.reserve ((size_t) numBlocks);
        const float* l = audio.getReadPointer (0);
        const float* r = audio.getReadPointer (1);
        for (int b = 0; b < numBlocks; ++b)
        {
            double sum = 0.0;
            for (int i = b * block, end = i + block; i < end; ++i)
                sum += (double) l[i] * l[i] + (double) r[i] * r[i];
            energies.push_back (sum / block);
        }

        auto toDb = [] (double e) { return -0.691 + 10.0 * std::log10 (juce::jmax (e, 1.0e-12)); };
        auto gatedMean = [&] (double thresholdDb)
        {
            double sum = 0.0;
            int count = 0;
            for (auto e : energies)
                if (toDb (e) > thresholdDb)
                {
                    sum += e;
                    ++count;
                }
            return count > 0 ? sum / count : 0.0;
        };

        const double absolute = gatedMean (-70.0);
        if (absolute <= 0.0)
            return -100.0f;
        return (float) toDb (gatedMean (toDb (absolute) - 10.0));
    }
}

//==============================================================================
FeatureRuntime::FeatureRuntime (juce::PropertiesFile& s, std::array<DeckEngine*, 2> d, std::array<DeckComponent*, 2> v,
                                MixerEngine& m, MixerComponent& mv, Recorder& r, AutomixController& a)
    : settings (s), decks (d), views (v), mixer (m), mixerView (mv), recorder (r), automix (a)
{
    // Si al arrancar la última sesión no se cerró bien, se ofrecerá recuperarla
    previousExitWasClean = settings.getBoolValue ("sessionClean", true);
    settings.setValue ("sessionClean", false);
    settings.saveIfNeeded();
    startTimerHz (ticksPerSecond);
}

FeatureRuntime::~FeatureRuntime()
{
    stopTimer();
}

void FeatureRuntime::timerCallback()
{
    ++ticks;
    checkSilence();
    checkClipping();
    followTracklist();
    checkAutoGain();

    // Recuperación: dejar cada deck en su punto cuando termine de cargar
    for (int i = 0; i < 2; ++i)
    {
        auto& p = pendingSeeks[(size_t) i];
        if (p.position < 0.0)
            continue;
        const auto* t = decks[(size_t) i]->getTrack();
        if (t != nullptr && ! decks[(size_t) i]->isLoading() && t->file == p.file)
        {
            decks[(size_t) i]->seek (p.position);
            p.position = -1.0;
        }
    }
    if (restoringSince != 0 && juce::Time::getMillisecondCounter() - restoringSince > 60000)
    {
        restoringSince = 0;
        settings.removeValue ("sessionRestoring");       // la restauración ha ido bien
        settings.saveIfNeeded();
    }

    if (ticks % (ticksPerSecond * 5) == 0)
        saveSession();
}

//== Protección anti-desastres =================================================
// Cuánto se oye un deck por el master: fader de canal (curva cuadrática) x crossfader
float FeatureRuntime::audibleGain (int d) const
{
    const float vol = mixer.getChannel (d).volume.load();
    const float x = mixer.crossfader.load();
    const float xf = d == 0 ? std::cos (x * juce::MathConstants<float>::halfPi) : std::sin (x * juce::MathConstants<float>::halfPi);
    return vol * vol * xf;
}

void FeatureRuntime::checkSilence()
{
    if (! Features::on (settings, Features::silenceAlert) || automix.isEnabled())
        return;

    // No tapar un aviso de saturación que se está mostrando
    if (juce::Time::getMillisecondCounter() - lastClipWarning < 4000 && lastClipWarning != 0)
        return;

    const double warnAt = Features::value (settings, Features::silenceSeconds, 20);
    bool anyAudible = false;
    double longest = 0.0;
    for (int i = 0; i < 2; ++i)
    {
        auto* deck = decks[(size_t) i];
        if (! deck->isPlaying() || deck->getTrack() == nullptr || audibleGain (i) < 0.05f)
            continue;
        anyAudible = true;
        longest = juce::jmax (longest, deck->getLength() - deck->getPosition());
    }

    if (anyAudible && longest < warnAt && onStatus != nullptr)
        onStatus (juce::String::fromUTF8 ("\xe2\x9a\xa0  Silencio en ") + juce::String ((int) std::ceil (longest))
                      + juce::String::fromUTF8 (" s: prepara y lanza la siguiente canción"), true, 400);
}

void FeatureRuntime::checkClipping()
{
    const float peak = mixer.preLimiterPeak.exchange (0.0f);
    if (! Features::on (settings, Features::clipAlert))
        return;

    // Más de 0 dB en al menos 3 de los últimos 2 segundos de medidas
    clipHistory[(size_t) clipIndex] = peak > 1.06f;
    clipIndex = (clipIndex + 1) % (int) clipHistory.size();
    const auto clipped = std::count (clipHistory.begin(), clipHistory.end(), true);

    const auto now = juce::Time::getMillisecondCounter();
    if (clipped >= 3 && now - lastClipWarning > 8000 && onStatus != nullptr)
    {
        lastClipWarning = now;
        onStatus (juce::String::fromUTF8 ("\xe2\x9a\xa0  Master saturado: baja el GAIN de los canales o el volumen master"), true, 4000);
    }
}

bool FeatureRuntime::shouldConfirmQuit (juce::String& reason) const
{
    if (! Features::on (settings, Features::confirmQuit))
        return false;

    if (recorder.isRecording())
        reason = juce::String::fromUTF8 ("Hay una grabación en marcha. Si cierras, se guarda hasta este momento.");
    else if (decks[0]->isPlaying() || decks[1]->isPlaying())
        reason = juce::String::fromUTF8 ("Hay música sonando.");
    else
        return false;
    return true;
}

//== Tracklist con tiempos =====================================================
juce::String FeatureRuntime::describe (const Track& t)
{
    if (t.title.isNotEmpty())
        return t.artist.isNotEmpty() ? t.artist + " - " + t.title : t.title;
    return t.file.getFileNameWithoutExtension();
}

void FeatureRuntime::recordingStarted()
{
    tracklist.clear();
    candidate = nullptr;
    lastLogged = nullptr;
    candidateSince = 0.0;
}

// La canción "principal" es la que más se oye. Se apunta cuando lleva 8 s siendo la principal,
// con el minuto en que empezó a serlo (la que ya sonaba al empezar a grabar, en 00:00).
void FeatureRuntime::followTracklist()
{
    if (! recorder.isRecording())
        return;

    const Track* main = nullptr;
    float best = 0.05f;
    for (int i = 0; i < 2; ++i)
        if (decks[(size_t) i]->isPlaying() && decks[(size_t) i]->getTrack() != nullptr && audibleGain (i) > best)
        {
            best = audibleGain (i);
            main = decks[(size_t) i]->getTrack();
        }

    const double now = recorder.getSeconds();
    if (main != candidate)
    {
        candidate = main;
        candidateSince = tracklist.empty() && lastLogged == nullptr && now < 3.0 ? 0.0 : now;
    }

    if (candidate != nullptr && candidate != lastLogged && (now - candidateSince >= 8.0 || (candidateSince == 0.0 && now >= 2.0)))
    {
        lastLogged = candidate;
        tracklist.push_back ({ candidateSince, describe (*candidate) });
    }
}

juce::String FeatureRuntime::recordingStopped (const juce::File& wavFile)
{
    if (! Features::on (settings, Features::tracklist) || tracklist.empty())
        return {};

    const int format = Features::value (settings, Features::tracklistFormat, 0);
    const bool hours = tracklist.back().seconds >= 3600.0;
    juce::StringArray lines;
    for (size_t i = 0; i < tracklist.size(); ++i)
    {
        const auto& e = tracklist[i];
        const auto time = formatTime (i == 0 ? 0.0 : e.seconds, hours);   // YouTube exige que la primera sea 00:00
        if (format == 1)        lines.add (juce::String ((int) i + 1) + ". " + e.text + " (" + time + ")");
        else if (format == 2)   lines.add (e.text);
        else                    lines.add (time + " " + e.text);
    }

    const auto text = lines.joinIntoString ("\r\n");
    wavFile.withFileExtension ("txt").replaceWithText (text, false, false, "\r\n");
    return text;
}

//== Volumen igualado ==========================================================
void FeatureRuntime::checkAutoGain()
{
    for (int i = 0; i < 2; ++i)
    {
        auto* deck = decks[(size_t) i];
        const auto track = deck->getTrackShared();
        if (track == nullptr || deck->isLoading() || track.get() == gainChecked[(size_t) i])
            continue;

        gainChecked[(size_t) i] = track.get();
        if (! Features::on (settings, Features::autoGain))
            continue;

        const float target = Features::autoGainTargetDb (Features::value (settings, Features::autoGainLevel, 1));
        std::weak_ptr<bool> weak = alive;
        juce::Thread::launch ([this, weak, track, i, target]
        {
            const float loudness = measureLoudness (*track);
            juce::MessageManager::callAsync ([this, weak, track, i, target, loudness]
            {
                if (weak.expired() || loudness < -60.0f || decks[(size_t) i]->getTrack() != track.get())
                    return;
                const float gain = juce::jlimit (-12.0f, 12.0f, target - loudness);
                mixerView.setGainDb (i, gain);
                if (onStatus != nullptr)
                    onStatus (juce::String::fromUTF8 ("Volumen igualado (deck ") + (i == 0 ? "A" : "B") + "): GAIN "
                                  + (gain >= 0.0f ? "+" : "") + juce::String (gain, 1) + " dB", false, 2500);
            });
        });
    }
}

//== Recuperación tras un cierre inesperado ====================================
void FeatureRuntime::saveSession()
{
    if (Features::value (settings, Features::recovery, 0) == 0)
    {
        if (settings.containsKey ("sessionState"))
        {
            settings.removeValue ("sessionState");
            settings.saveIfNeeded();
        }
        return;
    }

    juce::XmlElement xml ("SESSION");
    for (int i = 0; i < 2; ++i)
        if (const auto* t = decks[(size_t) i]->getTrack())
        {
            auto* e = xml.createNewChildElement ("DECK");
            e->setAttribute ("index", i);
            e->setAttribute ("file", t->file.getFullPathName());
            e->setAttribute ("position", decks[(size_t) i]->getPosition());
            e->setAttribute ("title", describe (*t));
        }
    if (recorder.isRecording())
        xml.createNewChildElement ("RECORDING")->setAttribute ("file", recorder.getFile().getFullPathName());

    settings.setValue ("sessionState", &xml);
    settings.saveIfNeeded();
}

void FeatureRuntime::markCleanExit()
{
    settings.setValue ("sessionClean", true);
    settings.removeValue ("sessionRestoring");
    settings.saveIfNeeded();
}

void FeatureRuntime::startupRecovery()
{
    const int mode = Features::value (settings, Features::recovery, 0);
    auto state = settings.getXmlValue ("sessionState");
    if (mode == 0 || previousExitWasClean || state == nullptr)
        return;

    // Lo que se puede recuperar
    juce::StringArray items;
    for (auto* e : state->getChildWithTagNameIterator ("DECK"))
        if (juce::File (e->getStringAttribute ("file")).existsAsFile())
            items.add ("Deck " + juce::String (e->getIntAttribute ("index") == 0 ? "A" : "B") + ": "
                       + e->getStringAttribute ("title") + "  (" + formatTime (e->getDoubleAttribute ("position"), false) + ")");
    if (auto* rec = state->getChildByName ("RECORDING"))
        if (juce::File (rec->getStringAttribute ("file")).existsAsFile())
            items.add (juce::String::fromUTF8 ("Grabación interrumpida: ") + juce::File (rec->getStringAttribute ("file")).getFileName());
    if (items.isEmpty())
        return;

    // Si la última restauración no llegó a terminar bien, se pregunta siempre (evita bucles de cierres)
    const bool failedBefore = settings.getBoolValue ("sessionRestoring", false);
    if (mode == 2 && ! failedBefore)
    {
        restoreSession (*state);
        return;
    }

    std::shared_ptr<juce::XmlElement> shared (state.release());
    std::weak_ptr<bool> weak = alive;
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, juce::String::fromUTF8 ("Recuperar la sesión"),
        juce::String::fromUTF8 ("CesarDJ no se cerró bien la última vez. ¿Quieres recuperar cómo estaba?\n\n")
            + items.joinIntoString ("\n")
            + juce::String::fromUTF8 ("\n\nLas canciones quedan en pausa en el mismo punto."),
        "Recuperar", "No", nullptr,
        juce::ModalCallbackFunction::create ([this, weak, shared] (int r)
        {
            if (weak.expired())
                return;
            if (r == 1)
                restoreSession (*shared);
            else
            {
                settings.removeValue ("sessionState");
                settings.saveIfNeeded();
            }
        }));
}

void FeatureRuntime::restoreSession (const juce::XmlElement& state)
{
    settings.setValue ("sessionRestoring", true);
    settings.saveIfNeeded();
    restoringSince = juce::Time::getMillisecondCounter();

    for (auto* e : state.getChildWithTagNameIterator ("DECK"))
    {
        const int i = juce::jlimit (0, 1, e->getIntAttribute ("index"));
        const juce::File f (e->getStringAttribute ("file"));
        if (! f.existsAsFile())
            continue;
        pendingSeeks[(size_t) i] = { f, e->getDoubleAttribute ("position") };
        views[(size_t) i]->loadFile (f);
    }

    if (auto* rec = state.getChildByName ("RECORDING"))
    {
        const juce::File f (rec->getStringAttribute ("file"));
        if (f.existsAsFile())
        {
            const bool ok = repairWav (f);
            juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                juce::String::fromUTF8 ("Grabación interrumpida"),
                ok ? juce::String::fromUTF8 ("La grabación que estaba en marcha se ha reparado y se puede escuchar:\n") + f.getFullPathName()
                   : juce::String::fromUTF8 ("No se ha podido reparar la grabación:\n") + f.getFullPathName());
        }
    }

    if (onStatus != nullptr)
        onStatus (juce::String::fromUTF8 ("Sesión recuperada: las canciones están en pausa en el mismo punto"), false, 5000);
}

// Un WAV cortado de golpe tiene los tamaños de la cabecera sin actualizar: se recalculan con el tamaño real
bool FeatureRuntime::repairWav (const juce::File& file)
{
    juce::MemoryBlock header;
    {
        juce::FileInputStream in (file);
        if (! in.openedOk() || in.readIntoMemoryBlock (header, 4096) < 12)
            return false;
    }

    juce::FileOutputStream out (file);       // abre sin borrar (posición al final)
    if (! out.openedOk())
        return false;

    const auto* bytes = static_cast<const char*> (header.getData());
    if (std::memcmp (bytes, "RIFF", 4) != 0 || std::memcmp (bytes + 8, "WAVE", 4) != 0)
        return false;

    const juce::int64 size = file.getSize();
    size_t pos = 12;
    while (pos + 8 <= header.getSize())
    {
        const auto chunkSize = (juce::uint32) juce::ByteOrder::littleEndianInt (bytes + pos + 4);
        if (std::memcmp (bytes + pos, "data", 4) == 0)
        {
            const auto dataSize = (juce::uint32) juce::jmin<juce::int64> (0xffffffff, size - (juce::int64) pos - 8);
            const auto riffSize = (juce::uint32) juce::jmin<juce::int64> (0xffffffff, size - 8);
            out.setPosition (4);
            out.writeInt ((int) riffSize);
            out.setPosition ((juce::int64) pos + 4);
            out.writeInt ((int) dataSize);
            out.flush();
            return true;
        }
        pos += 8 + chunkSize + (chunkSize & 1);
    }
    return false;
}
