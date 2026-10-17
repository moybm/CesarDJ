#pragma once

#include "Library.h"
#include "DeckEngine.h"

class DeckComponent;
class MixerComponent;

// Automix: mezcla sola la cola de Automix de la biblioteca.
//  - Precarga la siguiente canción en el deck libre
//  - Sale donde termina el sonido real de la canción (ajustado a un compás) y entra en el primer beat audible
//  - Iguala el BPM (si la diferencia es razonable), alinea los beats y hace el crossfade con intercambio de graves
class AutomixController : private juce::Timer
{
public:
    AutomixController (Library& library, std::array<DeckEngine*, 2> decks,
                       std::array<DeckComponent*, 2> deckViews, MixerComponent& mixer);

    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const                      { return state != State::off; }
    void setTransitionBeats (int beats)         { transitionBeats = juce::jlimit (4, 64, beats); }
    int getTransitionBeats() const              { return transitionBeats; }
    void mixNow()                               { mixRequested = true; }

    juce::String getStatus() const;
    std::function<void (const juce::String&)> onMessage;     // avisos para la barra de estado

private:
    enum class State { off, starting, playing, mixing };

    void timerCallback() override;
    void startFromQueue();
    void tickStarting();
    void tickPlaying();
    void tickMixing();
    void beginTransition();
    void finishTransition();
    bool takeNextFromQueue();
    bool isReady (int deckIndex) const;
    double mixOutStart() const;                 // segundos de la canción que suena
    bool nextIsSyncable() const;                // ¿se puede igualar el tempo de la siguiente?
    static double mixInPoint (const Track& t);
    static double audibleStart (const Track& t);
    static double audibleEnd (const Track& t);
    void message (const juce::String& text)     { if (onMessage != nullptr) onMessage (text); }

    Library& library;
    std::array<DeckEngine*, 2> decks;
    std::array<DeckComponent*, 2> views;
    MixerComponent& mixer;

    State state = State::off;
    int master = 0;                             // deck que suena
    int transitionBeats = 16;
    bool mixRequested = false;

    juce::File nextFile;
    juce::int64 nextEntryId = 0;
    juce::String nextTitle;
    bool nextLoadRequested = false;
    double startRequestTime = 0.0;

    double transitionStart = 0.0, transitionSeconds = 8.0;
    float crossFrom = 0.0f, crossTo = 1.0f;
    bool bassSwap = false;
};
