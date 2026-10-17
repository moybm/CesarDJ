#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <SoundTouch.h>
#include "Track.h"
#include "DeckFx.h"

// Motor de audio de un deck.
//
// La interfaz (hilo de mensajes) nunca toca el estado del hilo de audio directamente:
// envía comandos por una cola sin bloqueos (play, seek, loops...) o escribe valores atómicos (pitch, FX).
// El hilo de audio publica su posición en atómicos para que la interfaz la dibuje.
class DeckEngine : public juce::AudioSource
{
public:
    static constexpr int numHotCues = 4;

    // Datos ya conocidos de una canción (de la biblioteca): se aplican al cargarla
    struct TrackInfo
    {
        bool hasAnalysis = false;
        double bpm = 0.0, firstBeat = 0.0;
        double cuePoint = 0.0;
        std::array<double, numHotCues> hotCues { -1.0, -1.0, -1.0, -1.0 };
        int camelot = -1;                   // tonalidad ya conocida (-1 = calcularla)
    };

    explicit DeckEngine (juce::AudioFormatManager& formats);
    ~DeckEngine() override;

    //== Carga (asíncrona: decodifica y analiza en segundo plano) ================
    void loadFile (const juce::File& file, const TrackInfo& knownInfo);
    void loadFile (const juce::File& file)          { loadFile (file, TrackInfo()); }
    bool isLoading() const                          { return loading; }
    const Track* getTrack() const                   { return track.get(); }
    std::function<void()> onTrackChanged;           // carga terminada, análisis terminado o error
    std::function<void (const juce::File&)> onLoadFailed;
    std::function<void (const Track&)> onAnalysed;     // BPM recién calculado (para guardarlo)
    std::function<void()> onCuesChanged;               // cue o hot cues modificados
    std::function<void()> onStartedPlaying;            // primera vez que suena la canción cargada

    //== Transporte =================================================================
    void play();
    void pause();
    void togglePlay();
    bool isPlaying() const                          { return reportedPlaying.load(); }
    void cue();
    double getCuePoint() const                      { return cuePoint; }
    void seek (double seconds);
    double getPosition() const;                     // segundos (interpolado para dibujar fluido)
    double getLength() const                        { return track != nullptr ? track->getLength() : 0.0; }

    //== Tempo ======================================================================
    void setPitch (double percent);
    double getPitch() const                         { return pitchPercent; }
    double getSpeed() const                         { return 1.0 + pitchPercent / 100.0; }
    double getEffectiveBpm() const;                 // BPM original x velocidad
    void setKeyLock (bool shouldLock)               { keyLock = shouldLock; }
    bool getKeyLock() const                         { return keyLock.load(); }
    void setBend (double factor)                    { bend = factor; }   // empujar/frenar el plato (1 = nada)

    // Pitch (%) necesario para igualar un BPM; devuelve false si no hay beatgrid
    bool pitchForBpm (double targetBpm, double& pitchOut) const;
    // Alinea la fase de los beats con otro deck (salto de menos de medio beat)
    void alignPhaseWith (const DeckEngine& other);

    int getCamelot() const                          { return track != nullptr ? track->camelot.load() : -1; }

    //== Edición del beatgrid (se guarda en la biblioteca vía onAnalysed) =============
    void setBeatgrid (double bpm, double firstBeat);
    void applyTapTempo (double bpm);                // mantiene en su sitio el beat más cercano
    void reanalyseBeatgrid();

    //== Hot cues ===================================================================
    void triggerHotCue (int index);
    void deleteHotCue (int index);
    double getHotCue (int index) const              { return hotCues[(size_t) index]; }

    //== Loops, rolls y saltos =======================================================
    void setLoopBeats (double beats);
    double getLoopBeats() const                     { return loopBeats; }
    void toggleLoop();
    bool isLooping() const                          { return looping; }
    double getLoopStart() const                     { return loopStart; }
    double getLoopEnd() const                       { return loopEnd; }
    void startRoll (double beats);
    void endRoll();
    bool isRolling() const                          { return rolling; }
    double getRollStart() const                     { return rollStart; }
    double getRollEnd() const                       { return rollEnd; }
    void beatJump (double beats);

    //== Scratch ====================================================================
    void scratchStart();
    void scratchMove (double deltaSeconds);
    void scratchEnd();
    bool isScratching() const                       { return scratching; }

    //== Efectos (valores leídos por el hilo de audio) ===============================
    // Filtro fijo + dos huecos con el efecto elegido (ver DeckFx::Type)
    std::atomic<float> fxFilter { 0.0f }, fxAmount2 { 0.0f }, fxAmount3 { 0.0f };
    std::atomic<int> fxType2 { DeckFx::flanger }, fxType3 { DeckFx::cut };

    //== AudioSource ================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    //== Comandos interfaz -> audio ===================================================
    enum class Cmd { setTrack, play, pause, seek, loopOn, loopOff, rollOn, rollOff, scratchOn, scratchOff };
    struct Command { Cmd type; double a = 0.0, b = 0.0; Track* newTrack = nullptr; };

    void push (Command c);
    void processCommands();
    void trackLoaded (std::shared_ptr<Track> newTrack, const TrackInfo& info);
    void cuesChanged()                                  { if (onCuesChanged != nullptr) onCuesChanged(); }
    double beatLengthOrDefault() const;

    //== Hilo de audio =============================================================
    void renderChunk (float* L, float* R, int n);
    void renderScratch (float* L, float* R, int n, double srRatio);
    void renderStretched (float* L, float* R, int n, double srRatio, double speed);
    void readFrames (float* L, float* R, int n, double step);
    void readOne (double step, float& l, float& r);
    void sampleAt (double p, float& l, float& r) const;
    void jumpTo (double newPos);

    juce::AudioFormatManager& formatManager;

    // ---- Estado del hilo de mensajes
    std::shared_ptr<Track> track, retiredTrack;
    bool loading = false;
    bool startedPlaying = false;
    double pitchPercent = 0.0;
    double cuePoint = 0.0;
    std::array<double, numHotCues> hotCues;
    double loopBeats = 4.0;
    bool looping = false, rolling = false, scratching = false;
    double loopStart = 0.0, loopEnd = 0.0, rollStart = 0.0, rollEnd = 0.0;
    double scratchTarget = 0.0;

    juce::ThreadPool loaderPool { 1 };
    std::atomic<int> loadGeneration { 0 };
    std::shared_ptr<bool> aliveToken = std::make_shared<bool> (true);

    // ---- Compartido entre hilos
    juce::AbstractFifo fifo { 256 };
    std::array<Command, 256> commands;
    std::atomic<double> speedAtomic { 1.0 }, bend { 1.0 }, scratchTargetAtomic { 0.0 };
    std::atomic<bool> keyLock { false };
    std::atomic<double> reportedPos { 0.0 }, reportedTime { 0.0 }, reportedSpeed { 0.0 };
    std::atomic<bool> reportedPlaying { false };

    // ---- Estado del hilo de audio
    Track* audioTrack = nullptr;
    double deviceRate = 44100.0;
    double pos = 0.0, slipPos = 0.0;            // en muestras del archivo
    bool aPlaying = false, aLoop = false, aRoll = false, aScratch = false;
    double aLoopStart = 0.0, aLoopEnd = 0.0, aRollStart = 0.0, aRollEnd = 0.0;
    double scratchVel = 0.0;
    double xfPos = 0.0;                         // crossfade anti-clic tras un salto
    int xfRemain = 0;
    static constexpr int xfLength = 96;
    juce::SmoothedValue<float> playGain { 0.0f };

    soundtouch::SoundTouch stretcher;
    bool stretcherInUse = false;
    std::vector<float> tmpL, tmpR, tmpInterleaved;

    DeckFx fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckEngine)
};
