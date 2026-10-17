#include "DeckEngine.h"
#include "TrackAnalyser.h"
#include "StemSeparator.h"

namespace
{
    constexpr int maxChunk = 2048;
    constexpr double fallbackBeatLength = 0.5;    // 120 BPM si la canción aún no está analizada
}

DeckEngine::DeckEngine (juce::AudioFormatManager& formats)
    : formatManager (formats)
{
    hotCues.fill (-1.0);
}

DeckEngine::~DeckEngine()
{
    aliveToken.reset();
    ++loadGeneration;                          // cancela cualquier carga en curso
    loaderPool.removeAllJobs (true, 10000);
}

//==============================================================================
// Carga
//==============================================================================
void DeckEngine::loadFile (const juce::File& file, const TrackInfo& knownInfo)
{
    const int generation = ++loadGeneration;
    loading = true;
    if (onTrackChanged != nullptr)
        onTrackChanged();

    std::weak_ptr<bool> alive = aliveToken;

    loaderPool.addJob ([this, file, generation, alive, knownInfo]
    {
        auto cancelled = [this, generation] { return generation != loadGeneration.load(); };

        auto newTrack = TrackAnalyser::loadTrack (formatManager, file, cancelled);
        if (cancelled())
            return;

        // Si la biblioteca ya conoce el BPM, no hace falta analizar: la canción llega lista
        if (newTrack != nullptr && knownInfo.hasAnalysis && knownInfo.bpm > 0.0)
        {
            newTrack->bpm = knownInfo.bpm;
            newTrack->firstBeat = knownInfo.firstBeat;
            newTrack->analysed = true;
        }

        juce::MessageManager::callAsync ([this, newTrack, generation, alive, file, knownInfo]
        {
            if (alive.expired() || generation != loadGeneration.load())
                return;

            if (newTrack == nullptr && onLoadFailed != nullptr)
                onLoadFailed (file);

            trackLoaded (newTrack, knownInfo);
        });

        if (newTrack == nullptr)
            return;

        // La canción ya se puede reproducir; ahora lo que falte: BPM/beatgrid y tonalidad...
        const bool needGrid = ! newTrack->analysed.load();
        const bool needKey = knownInfo.camelot < 0;
        if (! needKey)
            newTrack->camelot = knownInfo.camelot;

        if (needGrid)
            TrackAnalyser::detectBeatgrid (*newTrack, cancelled);
        if (! cancelled() && needKey)
            newTrack->camelot = TrackAnalyser::detectKey (*newTrack, cancelled);
        if (cancelled())
            return;

        newTrack->analysed = true;
        const bool analysedNow = needGrid || needKey;

        // ...y los stems: los de IA si ya están en caché; si no, los rápidos (la IA los mejora después)
        if (auto cached = StemSeparator::loadCache (newTrack->file, newTrack->numSamples()))
            newTrack->setStems (std::move (cached));
        else if (auto fast = StemSeparator::separateFast (*newTrack, cancelled))
            newTrack->setStems (std::move (fast));

        juce::MessageManager::callAsync ([this, generation, alive, newTrack, analysedNow]
        {
            if (alive.expired() || generation != loadGeneration.load())
                return;

            if (analysedNow && onAnalysed != nullptr)
                onAnalysed (*newTrack);
            if (onTrackChanged != nullptr)
                onTrackChanged();
        });
    });
}

void DeckEngine::trackLoaded (std::shared_ptr<Track> newTrack, const TrackInfo& info)
{
    loading = false;

    if (newTrack != nullptr)
    {
        // La pista anterior se libera en el hilo de mensajes, nunca en el de audio
        retiredTrack = std::move (track);
        track = std::move (newTrack);

        cuePoint = juce::jlimit (0.0, track->getLength(), info.cuePoint);
        hotCues = info.hotCues;
        looping = rolling = scratching = false;
        startedPlaying = false;
        reportedPos = 0.0;
        reportedPlaying = false;

        push ({ Cmd::setTrack, 0.0, 0.0, track.get() });
    }

    if (onTrackChanged != nullptr)
        onTrackChanged();
}

//==============================================================================
// Transporte
//==============================================================================
void DeckEngine::push (Command c)
{
    const auto scope = fifo.write (1);

    if (scope.blockSize1 > 0)
        commands[(size_t) scope.startIndex1] = c;
    else if (scope.blockSize2 > 0)
        commands[(size_t) scope.startIndex2] = c;
}

void DeckEngine::play()
{
    if (track == nullptr)
        return;

    push ({ Cmd::play });
    reportedPlaying = true;

    if (! startedPlaying)
    {
        startedPlaying = true;
        if (onStartedPlaying != nullptr)
            onStartedPlaying();
    }
}

void DeckEngine::pause()
{
    push ({ Cmd::pause });
    reportedPlaying = false;
}

void DeckEngine::togglePlay()
{
    if (isPlaying())
        pause();
    else
        play();
}

// Estilo CDJ: en pausa fija el cue; reproduciendo vuelve al cue y para
void DeckEngine::cue()
{
    if (track == nullptr)
        return;

    if (isPlaying())
    {
        pause();
        seek (cuePoint);
    }
    else
    {
        cuePoint = getPosition();
        cuesChanged();
    }
}

void DeckEngine::seek (double seconds)
{
    if (track == nullptr)
        return;

    seconds = juce::jlimit (0.0, getLength(), seconds);
    push ({ Cmd::seek, seconds });
    reportedPos = seconds;
    reportedTime = juce::Time::getMillisecondCounterHiRes();
}

double DeckEngine::getPosition() const
{
    double p = reportedPos.load();

    if (reportedPlaying.load() || scratching)
        p += (juce::Time::getMillisecondCounterHiRes() - reportedTime.load()) * 0.001 * reportedSpeed.load();

    return juce::jlimit (0.0, getLength(), p);
}

//==============================================================================
// Tempo y sync
//==============================================================================
void DeckEngine::setPitch (double percent)
{
    pitchPercent = percent;
    speedAtomic = getSpeed();
}

double DeckEngine::getEffectiveBpm() const
{
    return track != nullptr && track->hasBeatgrid() ? track->bpm * getSpeed() : 0.0;
}

bool DeckEngine::pitchForBpm (double targetBpm, double& pitchOut) const
{
    if (track == nullptr || ! track->hasBeatgrid() || targetBpm <= 0.0)
        return false;

    pitchOut = (targetBpm / track->bpm - 1.0) * 100.0;
    return true;
}

void DeckEngine::alignPhaseWith (const DeckEngine& other)
{
    if (track == nullptr || other.track == nullptr || ! track->hasBeatgrid() || ! other.track->hasBeatgrid())
        return;

    auto frac = [] (double x) { return x - std::floor (x); };

    const double myPos = getPosition();
    double diff = frac (other.track->beatAt (other.getPosition())) - frac (track->beatAt (myPos));

    if (diff > 0.5)  diff -= 1.0;
    if (diff < -0.5) diff += 1.0;

    seek (myPos + diff * track->beatLength());
}

void DeckEngine::setBeatgrid (double bpm, double firstBeat)
{
    if (track == nullptr || bpm < 30.0 || bpm > 300.0)
        return;

    const double len = 60.0 / bpm;
    track->bpm = bpm;
    track->firstBeat = firstBeat - std::floor (firstBeat / len) * len;
    track->analysed = true;

    if (onAnalysed != nullptr)
        onAnalysed (*track);
}

void DeckEngine::applyTapTempo (double bpm)
{
    if (track == nullptr)
        return;

    // El beat más cercano a la posición actual se queda donde está; el resto se reparte con el BPM nuevo
    const double anchor = track->hasBeatgrid() ? track->nearestBeat (getPosition()) : getPosition();
    setBeatgrid (bpm, anchor);
}

void DeckEngine::reanalyseBeatgrid()
{
    if (track == nullptr)
        return;

    auto t = track;
    const int generation = loadGeneration.load();
    std::weak_ptr<bool> alive = aliveToken;

    loaderPool.addJob ([this, t, generation, alive]
    {
        auto cancelled = [this, generation] { return generation != loadGeneration.load(); };
        TrackAnalyser::detectBeatgrid (*t, cancelled);
        if (cancelled())
            return;

        juce::MessageManager::callAsync ([this, t, generation, alive]
        {
            if (! alive.expired() && generation == loadGeneration.load() && onAnalysed != nullptr)
                onAnalysed (*t);
        });
    });
}

double DeckEngine::beatLengthOrDefault() const
{
    return track != nullptr && track->hasBeatgrid() ? track->beatLength() : fallbackBeatLength;
}

//==============================================================================
// Hot cues
//==============================================================================
void DeckEngine::triggerHotCue (int index)
{
    if (track == nullptr)
        return;

    auto& hc = hotCues[(size_t) index];

    if (hc < 0.0)
    {
        hc = getPosition();
        cuesChanged();
    }
    else
    {
        seek (hc);
        play();
    }
}

void DeckEngine::deleteHotCue (int index)
{
    hotCues[(size_t) index] = -1.0;
    cuesChanged();
}

//==============================================================================
// Loops, rolls y saltos
//==============================================================================
void DeckEngine::setLoopBeats (double beats)
{
    loopBeats = juce::jlimit (1.0 / 32.0, 32.0, beats);

    if (looping)
    {
        loopEnd = loopStart + loopBeats * beatLengthOrDefault();
        push ({ Cmd::loopOn, loopStart, loopEnd });
    }
}

void DeckEngine::toggleLoop()
{
    if (track == nullptr)
        return;

    if (looping)
    {
        looping = false;
        push ({ Cmd::loopOff });
        return;
    }

    const double now = getPosition();
    double start = now;

    if (track->hasBeatgrid())
    {
        // Loops de 1 beat o más empiezan en el beat más cercano; los más cortos en su subdivisión
        const double grid = juce::jmin (1.0, loopBeats);
        start = track->timeOfBeat (std::floor (track->beatAt (now) / grid + 0.5) * grid);
        if (start > now + 0.02)
            start -= grid * track->beatLength();
        start = juce::jmax (0.0, start);
    }

    loopStart = start;
    loopEnd = start + loopBeats * beatLengthOrDefault();
    looping = true;
    push ({ Cmd::loopOn, loopStart, loopEnd });
}

void DeckEngine::startRoll (double beats)
{
    if (track == nullptr)
        return;

    const double now = getPosition();
    double start = now;

    if (track->hasBeatgrid())
        start = juce::jmax (0.0, track->timeOfBeat (std::floor (track->beatAt (now) / beats) * beats));

    rollStart = start;
    rollEnd = start + beats * beatLengthOrDefault();
    rolling = true;
    push ({ Cmd::rollOn, rollStart, rollEnd });
}

void DeckEngine::endRoll()
{
    if (! rolling)
        return;

    rolling = false;
    push ({ Cmd::rollOff });
}

void DeckEngine::beatJump (double beats)
{
    if (track == nullptr)
        return;

    const double delta = beats * beatLengthOrDefault();

    if (looping)
    {
        loopStart += delta;
        loopEnd += delta;
        push ({ Cmd::loopOn, loopStart, loopEnd });
    }

    seek (getPosition() + delta);
}

//==============================================================================
// Scratch
//==============================================================================
void DeckEngine::scratchStart()
{
    if (track == nullptr)
        return;

    scratching = true;
    scratchTarget = getPosition();
    scratchTargetAtomic = scratchTarget;
    push ({ Cmd::scratchOn });
}

void DeckEngine::scratchMove (double deltaSeconds)
{
    if (! scratching)
        return;

    scratchTarget = juce::jlimit (0.0, getLength(), scratchTarget + deltaSeconds);
    scratchTargetAtomic = scratchTarget;
}

void DeckEngine::scratchEnd()
{
    if (! scratching)
        return;

    scratching = false;
    push ({ Cmd::scratchOff });
}

//==============================================================================
// Hilo de audio
//==============================================================================
void DeckEngine::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    deviceRate = sampleRate;

    tmpL.assign (maxChunk, 0.0f);
    tmpR.assign (maxChunk, 0.0f);
    tmpInterleaved.assign (maxChunk * 2, 0.0f);

    stretcher.setSampleRate ((unsigned int) sampleRate);
    stretcher.setChannels (2);
    stretcher.setSetting (SETTING_USE_QUICKSEEK, 0);
    stretcher.clear();
    stretcherInUse = false;

    playGain.reset (sampleRate, 0.004);
    for (auto& g : stemGain)
        g.reset (sampleRate, 0.03);
    fx.prepare (sampleRate, maxChunk);
}

void DeckEngine::processCommands()
{
    const auto scope = fifo.read (fifo.getNumReady());

    scope.forEach ([this] (int index)
    {
        const auto& c = commands[(size_t) index];
        const double sr = audioTrack != nullptr ? audioTrack->sampleRate : 44100.0;
        const bool audible = aPlaying || aScratch || playGain.getCurrentValue() > 0.0f;

        switch (c.type)
        {
            case Cmd::setTrack:
                audioTrack = c.newTrack;
                pos = slipPos = 0.0;
                aPlaying = aLoop = aRoll = aScratch = false;
                xfRemain = 0;
                playGain.setCurrentAndTargetValue (0.0f);
                stretcher.clear();
                fx.reset();
                break;

            case Cmd::play:   aPlaying = audioTrack != nullptr; break;
            case Cmd::pause:  aPlaying = false; break;

            case Cmd::seek:
                if (audible) jumpTo (c.a * sr);
                else         pos = c.a * sr;
                slipPos = pos;
                stretcher.clear();
                break;

            case Cmd::loopOn:
                aLoopStart = c.a * sr;
                aLoopEnd = c.b * sr;
                aLoop = true;
                if (pos >= aLoopEnd)
                    jumpTo (aLoopStart);
                break;

            case Cmd::loopOff:  aLoop = false; break;

            case Cmd::rollOn:
                aRollStart = c.a * sr;
                aRollEnd = c.b * sr;
                slipPos = pos;
                aRoll = true;
                break;

            case Cmd::rollOff:
                if (aRoll)
                {
                    aRoll = false;
                    jumpTo (slipPos);     // "slip": vuelve a donde estaría la canción
                    stretcher.clear();
                }
                break;

            case Cmd::scratchOn:
                aScratch = true;
                scratchVel = 0.0;
                stretcher.clear();
                break;

            case Cmd::scratchOff:
                aScratch = false;
                stretcher.clear();
                break;
        }
    });
}

void DeckEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    processCommands();

    auto* L = info.buffer->getWritePointer (0, info.startSample);
    auto* R = info.buffer->getNumChannels() > 1 ? info.buffer->getWritePointer (1, info.startSample) : nullptr;
    juce::FloatVectorOperations::clear (L, info.numSamples);
    if (R != nullptr)
        juce::FloatVectorOperations::clear (R, info.numSamples);

    if (audioTrack != nullptr && R != nullptr)
        for (int offset = 0; offset < info.numSamples; offset += maxChunk)
            renderChunk (L + offset, R + offset, juce::jmin (maxChunk, info.numSamples - offset));

    // Publicar posición para la interfaz
    double reportSamples = pos;
    if (stretcherInUse && audioTrack != nullptr)
        reportSamples -= (stretcher.numUnprocessedSamples() + stretcher.numSamples() * speedAtomic.load())
                         * (audioTrack->sampleRate / deviceRate);

    const double sr = audioTrack != nullptr ? audioTrack->sampleRate : 44100.0;
    reportedPos = juce::jmax (0.0, reportSamples / sr);
    reportedTime = juce::Time::getMillisecondCounterHiRes();
    reportedPlaying = aPlaying;
    reportedSpeed = aScratch ? scratchVel * deviceRate / sr
                             : (aPlaying ? speedAtomic.load() * bend.load() : 0.0);
}

// Ganancia de cada stem a partir de los mandos: a la izquierda se quita, a la derecha se aísla
void DeckEngine::updateStemGains()
{
    audioStems = audioTrack != nullptr ? audioTrack->stems.load() : nullptr;

    float k[numStems];
    for (int s = 0; s < numStems; ++s)
        k[s] = juce::jlimit (-1.0f, 1.0f, stemKnob[s].load());

    bool allUnity = true;
    for (int s = 0; s < numStems; ++s)
    {
        float g = 1.0f + juce::jmin (0.0f, k[s]);
        for (int o = 0; o < numStems; ++o)
            if (o != s)
                g *= 1.0f - juce::jmax (0.0f, k[o]);
        stemGain[(size_t) s].setTargetValue (g);
        allUnity = allUnity && g > 0.999f && ! stemGain[(size_t) s].isSmoothing();
    }

    stemsActive = audioStems != nullptr && ! allUnity;
}

void DeckEngine::renderChunk (float* L, float* R, int n)
{
    updateStemGains();

    const double srRatio = audioTrack->sampleRate / deviceRate;
    const double speed = speedAtomic.load() * bend.load();
    const double blockStartSeconds = pos / audioTrack->sampleRate;

    if (aScratch)
    {
        renderScratch (L, R, n, srRatio);
    }
    else
    {
        playGain.setTargetValue (aPlaying ? 1.0f : 0.0f);

        if (! aPlaying && ! playGain.isSmoothing())
            return;     // en pausa: silencio

        if (keyLock.load())
        {
            renderStretched (L, R, n, srRatio, speed);
        }
        else
        {
            if (stretcherInUse)
            {
                stretcher.clear();
                stretcherInUse = false;
            }
            readFrames (L, R, n, srRatio * speed);
        }

        for (int i = 0; i < n; ++i)
        {
            const float g = playGain.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }
    }

    // Efectos (el "Cortar" necesita la fase del beat)
    double beatPhase = 0.0, beatIncrement = 0.0;
    if (audioTrack->hasBeatgrid())
    {
        beatPhase = audioTrack->beatAt (blockStartSeconds);
        beatIncrement = (aScratch ? 0.0 : speed / deviceRate) / audioTrack->beatLength();
    }

    fx.process (L, R, n, fxFilter.load(), fxType2.load(), fxAmount2.load(), fxType3.load(), fxAmount3.load(),
                beatPhase, beatIncrement);
}

void DeckEngine::renderScratch (float* L, float* R, int n, double srRatio)
{
    // El audio persigue la posición del plato: la velocidad sale de la distancia que falta
    const double target = scratchTargetAtomic.load() * audioTrack->sampleRate;
    const double maxVel = 12.0 * srRatio;
    const double desired = juce::jlimit (-maxVel, maxVel, (target - pos) / n);
    const double startVel = scratchVel;
    const double endVel = startVel + (desired - startVel) * 0.5;

    for (int i = 0; i < n; ++i)
    {
        const double v = startVel + (endVel - startVel) * (i + 1) / n;
        readOne (v, L[i], R[i]);
    }

    scratchVel = endVel;
    playGain.setCurrentAndTargetValue (aPlaying ? 1.0f : 0.0f);
}

void DeckEngine::renderStretched (float* L, float* R, int n, double srRatio, double speed)
{
    stretcherInUse = true;
    stretcher.setTempo (juce::jlimit (0.25, 4.0, speed));

    constexpr int feed = 256;
    while ((int) stretcher.numSamples() < n)
    {
        readFrames (tmpL.data(), tmpR.data(), feed, srRatio);
        for (int i = 0; i < feed; ++i)
        {
            tmpInterleaved[(size_t) (2 * i)]     = tmpL[(size_t) i];
            tmpInterleaved[(size_t) (2 * i + 1)] = tmpR[(size_t) i];
        }
        stretcher.putSamples (tmpInterleaved.data(), feed);
    }

    const auto received = (int) stretcher.receiveSamples (tmpInterleaved.data(), (unsigned int) n);
    for (int i = 0; i < received; ++i)
    {
        L[i] = tmpInterleaved[(size_t) (2 * i)];
        R[i] = tmpInterleaved[(size_t) (2 * i + 1)];
    }
}

void DeckEngine::readFrames (float* L, float* R, int n, double step)
{
    for (int i = 0; i < n; ++i)
        readOne (step, L[i], R[i]);
}

void DeckEngine::readOne (double step, float& l, float& r)
{
    if (stemsActive)
        sampleAtStems (pos, l, r);
    else
        sampleAt (pos, l, r);

    if (xfRemain > 0)
    {
        float ol, orr;
        if (stemsActive)
            sampleAtStems (xfPos, ol, orr);
        else
            sampleAt (xfPos, ol, orr);
        const float a = (float) xfRemain / (float) xfLength;
        l = l * (1.0f - a) + ol * a;
        r = r * (1.0f - a) + orr * a;
        xfPos += step;
        --xfRemain;
    }

    pos += step;
    if (aRoll)
        slipPos += step;

    const bool loopActive = aRoll || aLoop;
    const double ls = aRoll ? aRollStart : aLoopStart;
    const double le = aRoll ? aRollEnd : aLoopEnd;

    if (loopActive && step > 0.0 && le > ls && pos >= le)
        jumpTo (ls + std::fmod (pos - le, le - ls));

    const double end = (double) audioTrack->numSamples();
    if (pos >= end)
    {
        pos = end;
        if (! aScratch)
            aPlaying = false;
    }
    else if (pos < 0.0)
    {
        pos = 0.0;
    }
}

void DeckEngine::jumpTo (double newPos)
{
    xfPos = pos;
    xfRemain = xfLength;
    pos = newPos;
}

namespace
{
    inline float hermite (const float* d, int k, float t)
    {
        const float xm1 = d[k - 1], x0 = d[k], x1 = d[k + 1], x2 = d[k + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
}

// Mezcla de stems: instrumental = mezcla - voz - percusión, cada uno con su ganancia
void DeckEngine::sampleAtStems (double p, float& l, float& r)
{
    const int num = audioTrack->numSamples();
    const int i = (int) std::floor (p);
    if (i < 1 || i + 2 >= num || audioStems->vocals.getNumSamples() < num)
    {
        sampleAt (p, l, r);
        return;
    }

    const float t = (float) (p - i);
    const float gv = stemGain[vocal].getNextValue();
    const float gi = stemGain[instru].getNextValue();
    const float gb = stemGain[beat].getNextValue();

    float out[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        const float mix = hermite (audioTrack->audio.getReadPointer (ch), i, t);
        const float voc = hermite (audioStems->vocals.getReadPointer (ch), i, t);
        const float bt  = hermite (audioStems->beat.getReadPointer (ch), i, t);
        out[ch] = gi * (mix - voc - bt) + gv * voc + gb * bt;
    }
    l = out[0];
    r = out[1];
}

// Interpolación cúbica (Hermite) entre muestras: suena limpia con cualquier pitch o scratch
void DeckEngine::sampleAt (double p, float& l, float& r) const
{
    const int num = audioTrack->numSamples();
    const int i = (int) std::floor (p);

    if (i < 1 || i + 2 >= num)
    {
        l = r = 0.0f;
        return;
    }

    const float t = (float) (p - i);
    auto hermite = [t] (const float* d, int k)
    {
        const float xm1 = d[k - 1], x0 = d[k], x1 = d[k + 1], x2 = d[k + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    };

    l = hermite (audioTrack->audio.getReadPointer (0), i);
    r = hermite (audioTrack->audio.getReadPointer (1), i);
}
