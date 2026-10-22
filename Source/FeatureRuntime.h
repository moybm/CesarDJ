#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "DeckEngine.h"
#include "DeckComponent.h"
#include "MixerEngine.h"
#include "MixerComponent.h"
#include "Recorder.h"
#include "Automix.h"

// Funciones opcionales de CONFIG > Funciones mientras CesarDJ está abierto:
//  - Alerta de silencio y aviso de saturación del master
//  - Confirmar antes de cerrar si algo suena o se graba
//  - Tracklist con tiempos durante la grabación
//  - Volumen igualado al cargar una canción
//  - Recuperación tras un cierre inesperado (decks en el mismo punto, grabación reparada)
// Cada función mira su ajuste en cada momento: activarla o desactivarla tiene efecto al instante.
class FeatureRuntime : private juce::Timer
{
public:
    FeatureRuntime (juce::PropertiesFile& settings, std::array<DeckEngine*, 2> decks, std::array<DeckComponent*, 2> deckViews,
                    MixerEngine& mixer, MixerComponent& mixerView, Recorder& recorder, AutomixController& automix);
    ~FeatureRuntime() override;

    // Mensajes para la barra superior (warning = en rojo)
    std::function<void (const juce::String& message, bool warning, int milliseconds)> onStatus;

    // Grabación: al empezar y al terminar (devuelve la tracklist, vacía si la función está desactivada)
    void recordingStarted();
    juce::String recordingStopped (const juce::File& wavFile);

    // Cerrar CesarDJ: true si hay que preguntar antes (y por qué)
    bool shouldConfirmQuit (juce::String& reason) const;

    // Al arrancar (con la ventana ya visible) y al cerrar bien
    void startupRecovery();
    void markCleanExit();

    // Repara la cabecera de un WAV que se quedó a medias (cierre inesperado durante la grabación)
    static bool repairWav (const juce::File& file);

private:
    void timerCallback() override;
    void checkSilence();
    void checkClipping();
    void followTracklist();
    void checkAutoGain();
    void saveSession();
    void restoreSession (const juce::XmlElement& state);
    float audibleGain (int deck) const;
    static juce::String describe (const Track& t);

    juce::PropertiesFile& settings;
    std::array<DeckEngine*, 2> decks;
    std::array<DeckComponent*, 2> views;
    MixerEngine& mixer;
    MixerComponent& mixerView;
    Recorder& recorder;
    AutomixController& automix;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    int ticks = 0;
    bool previousExitWasClean = true;

    // Avisos
    juce::uint32 lastClipWarning = 0;
    std::array<bool, 10> clipHistory {};
    int clipIndex = 0;

    // Tracklist
    struct Entry { double seconds; juce::String text; };
    std::vector<Entry> tracklist;
    const Track* candidate = nullptr;
    double candidateSince = 0.0;
    const Track* lastLogged = nullptr;

    // Volumen igualado: última canción medida en cada deck
    std::array<const void*, 2> gainChecked { nullptr, nullptr };

    // Recuperación: dónde dejar cada deck cuando termine de cargar
    struct PendingSeek { juce::File file; double position = -1.0; };
    std::array<PendingSeek, 2> pendingSeeks;
    juce::uint32 restoringSince = 0;
};
