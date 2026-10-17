#include "Automix.h"
#include "DeckComponent.h"
#include "MixerComponent.h"

namespace
{
    double nowMs()                      { return juce::Time::getMillisecondCounterHiRes(); }
    float sideOf (int deck)             { return deck == 0 ? 0.0f : 1.0f; }   // posición del crossfader para ese deck

    juce::String formatTime (double seconds)
    {
        const int s = juce::jmax (0, juce::roundToInt (seconds));
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }
}

AutomixController::AutomixController (Library& lib, std::array<DeckEngine*, 2> d,
                                      std::array<DeckComponent*, 2> v, MixerComponent& m)
    : library (lib), decks (d), views (v), mixer (m)
{
}

//==============================================================================
void AutomixController::setEnabled (bool shouldBeEnabled)
{
    if (shouldBeEnabled == isEnabled())
        return;

    if (! shouldBeEnabled)
    {
        // Si se apaga a mitad de mezcla, se deja el sonido limpio: graves restaurados
        mixer.setEqLow (0, 0.0f);
        mixer.setEqLow (1, 0.0f);
        state = State::off;
        nextFile = {};
        stopTimer();
        return;
    }

    mixRequested = false;
    nextFile = {};
    nextLoadRequested = false;

    // Si ya suena un deck, el Automix sigue desde ahí; si no, empieza con la primera de la cola
    const int playing = decks[0]->isPlaying() ? 0 : (decks[1]->isPlaying() ? 1 : -1);
    if (playing >= 0)
    {
        master = playing;
        mixer.setChannelVolume (master, 1.0f);
        mixer.setCrossfader (sideOf (master));
        state = State::playing;
    }
    else
    {
        startFromQueue();
        if (state == State::off)
            return;
    }

    startTimerHz (20);
}

void AutomixController::startFromQueue()
{
    if (! takeNextFromQueue())
    {
        message (juce::String::fromUTF8 ("La cola de Automix está vacía: añade canciones (clic derecho > Añadir a Automix)"));
        state = State::off;
        return;
    }

    master = 0;
    views[(size_t) master]->setPitchValue (0.0);
    views[(size_t) master]->loadFile (nextFile);
    nextLoadRequested = true;
    startRequestTime = nowMs();
    state = State::starting;
}

bool AutomixController::takeNextFromQueue()
{
    auto next = library.automixNext();
    if (! next.has_value())
        return false;

    nextFile = next->file;
    nextEntryId = next->entryId;
    nextTitle = next->title;
    nextLoadRequested = false;
    return true;
}

bool AutomixController::isReady (int deckIndex) const
{
    const auto& deck = *decks[(size_t) deckIndex];
    const auto* t = deck.getTrack();
    return ! deck.isLoading() && t != nullptr && t->file == nextFile;
}

//==============================================================================
void AutomixController::timerCallback()
{
    switch (state)
    {
        case State::starting: tickStarting(); break;
        case State::playing:  tickPlaying();  break;
        case State::mixing:   tickMixing();   break;
        case State::off:      break;
    }
}

void AutomixController::tickStarting()
{
    if (isReady (master))
    {
        library.removeFromAutomix ({ nextEntryId });
        nextFile = {};
        mixer.setChannelVolume (master, 1.0f);
        mixer.setEqLow (master, 0.0f);
        mixer.setCrossfader (sideOf (master));
        decks[(size_t) master]->play();
        state = State::playing;
        return;
    }

    // Archivo que no se puede abrir: se quita de la cola y se prueba con el siguiente
    if (nowMs() - startRequestTime > 15000.0)
    {
        library.removeFromAutomix ({ nextEntryId });
        startFromQueue();
    }
}

void AutomixController::tickPlaying()
{
    auto& m = *decks[(size_t) master];
    const int other = 1 - master;
    auto& o = *decks[(size_t) other];

    // Siguiente canción: tomarla de la cola y precargarla en el deck libre
    if (nextFile == juce::File() && ! takeNextFromQueue())
    {
        if (! m.isPlaying() && m.getPosition() >= m.getLength() - 0.5)
        {
            message ("Automix terminado: no quedan canciones en la cola");
            setEnabled (false);
        }
        return;
    }

    if (! nextLoadRequested && ! o.isPlaying())
    {
        views[(size_t) other]->setPitchValue (0.0);     // a su velocidad real hasta que se sincronice
        views[(size_t) other]->loadFile (nextFile);
        nextLoadRequested = true;
    }

    // ¿La canción que suena se ha parado sola (fin) o la ha pausado el usuario?
    if (! m.isPlaying())
    {
        if (m.getPosition() >= m.getLength() - 0.5 && isReady (other))
        {
            // Terminó sin mezclar (p. ej. canción muy corta): empezar la siguiente directamente
            library.removeFromAutomix ({ nextEntryId });
            master = other;
            nextFile = {};
            mixer.setChannelVolume (master, 1.0f);
            mixer.setCrossfader (sideOf (master));
            decks[(size_t) master]->play();
        }
        return;     // pausa manual: el Automix espera
    }

    const bool timeToMix = mixRequested || m.getPosition() >= mixOutStart();
    const bool nextReady = isReady (other) && o.getTrack()->analysed.load();

    if (timeToMix && nextReady)
        beginTransition();
    else if (timeToMix && isReady (other) && m.getLength() - m.getPosition() < 3.0)
        beginTransition();          // no da tiempo a esperar el análisis: mezcla sin sincronizar
}

double AutomixController::mixOutStart() const
{
    const auto& m = *decks[(size_t) master];
    const auto* t = m.getTrack();
    if (t == nullptr)
        return 0.0;

    const double end = audibleEnd (*t);

    // Sin beatgrid, o si la siguiente no se puede sincronizar: fundido de 8 s antes del final
    if (! t->hasBeatgrid() || ! nextIsSyncable())
        return end - 8.0 * m.getSpeed();

    // Empieza N beats antes del final audible, en el primer beat de un compás
    double start = end - transitionBeats * t->beatLength();
    const double bar = std::floor (t->beatAt (start) / 4.0) * 4.0;
    start = t->timeOfBeat (bar);
    return juce::jmax (0.0, start);
}

bool AutomixController::nextIsSyncable() const
{
    const int other = 1 - master;
    if (! isReady (other))
        return true;        // aún no se sabe: se asume que sí

    const auto* tm = decks[(size_t) master]->getTrack();
    const auto* to = decks[(size_t) other]->getTrack();
    if (tm == nullptr || to == nullptr || ! tm->hasBeatgrid() || ! to->hasBeatgrid())
        return false;

    const double ratio = decks[(size_t) master]->getEffectiveBpm() / to->bpm;
    return ratio > 0.9 && ratio < 1.1;
}

//==============================================================================
void AutomixController::beginTransition()
{
    mixRequested = false;

    const int other = 1 - master;
    auto& m = *decks[(size_t) master];
    auto& o = *decks[(size_t) other];
    const auto* tm = m.getTrack();
    const auto* to = o.getTrack();
    if (tm == nullptr || to == nullptr)
        return;

    // Igualar tempo si la diferencia es razonable (±10 %)
    const bool synced = nextIsSyncable();
    views[(size_t) other]->setPitchValue (synced ? (m.getEffectiveBpm() / to->bpm - 1.0) * 100.0 : 0.0);

    mixer.setChannelVolume (other, 1.0f);
    o.seek (mixInPoint (*to));
    o.play();
    if (synced)
        o.alignPhaseWith (m);

    // Duración en tiempo real: N beats al tempo actual (o 8 s si no se puede sincronizar)
    transitionSeconds = synced ? transitionBeats * 60.0 / m.getEffectiveBpm() : 8.0;
    transitionSeconds = juce::jlimit (2.0, 60.0, juce::jmin (transitionSeconds, m.getLength() - m.getPosition()));
    transitionStart = nowMs();
    crossFrom = sideOf (master);
    crossTo = sideOf (other);

    // Intercambio de graves: el bajo de la canción nueva entra cuando sale el de la anterior
    bassSwap = synced;
    if (bassSwap)
        mixer.setEqLow (other, -1.0f);

    library.removeFromAutomix ({ nextEntryId });
    state = State::mixing;
}

void AutomixController::tickMixing()
{
    const double p = juce::jlimit (0.0, 1.0, (nowMs() - transitionStart) / (transitionSeconds * 1000.0));
    const int other = 1 - master;

    mixer.setCrossfader (crossFrom + (crossTo - crossFrom) * (float) p);

    if (bassSwap)
    {
        const float swap = (float) juce::jlimit (0.0, 1.0, (p - 0.45) / 0.1);
        mixer.setEqLow (master, -swap);
        mixer.setEqLow (other, swap - 1.0f);
    }

    if (p >= 1.0)
        finishTransition();
}

void AutomixController::finishTransition()
{
    const int other = 1 - master;

    decks[(size_t) master]->pause();
    mixer.setEqLow (master, 0.0f);
    mixer.setEqLow (other, 0.0f);
    mixer.setCrossfader (crossTo);

    master = other;
    nextFile = {};
    nextLoadRequested = false;
    state = State::playing;
}

//==============================================================================
double AutomixController::audibleStart (const Track& t)
{
    for (size_t i = 0; i < t.wave.size(); ++i)
    {
        const auto& b = t.wave[i];
        if (juce::jmax (b.low, b.mid, b.high) > 25)
            return (double) i / Track::binsPerSecond;
    }
    return 0.0;
}

double AutomixController::audibleEnd (const Track& t)
{
    for (size_t i = t.wave.size(); i-- > 0;)
    {
        const auto& b = t.wave[i];
        if (juce::jmax (b.low, b.mid, b.high) > 25)
            return (double) (i + 1) / Track::binsPerSecond;
    }
    return t.getLength();
}

// Entrada: el primer beat a partir de donde empieza a sonar
double AutomixController::mixInPoint (const Track& t)
{
    const double start = audibleStart (t);
    if (! t.hasBeatgrid())
        return start;
    return juce::jmax (0.0, t.timeOfBeat (std::ceil (t.beatAt (start) - 0.05)));
}

//==============================================================================
juce::String AutomixController::getStatus() const
{
    switch (state)
    {
        case State::off:
            return {};
        case State::starting:
            return "AUTOMIX  -  cargando " + nextTitle + "...";
        case State::mixing:
            return juce::String::fromUTF8 ("AUTOMIX  -  mezclando hacia ") + nextTitle;
        case State::playing:
        {
            if (nextFile == juce::File())
                return juce::String::fromUTF8 ("AUTOMIX  -  última canción de la cola");

            const auto& m = *decks[(size_t) master];
            const double inSeconds = (mixOutStart() - m.getPosition()) / juce::jmax (0.1, m.getSpeed());
            return "AUTOMIX  -  siguiente: " + nextTitle + "  en " + formatTime (inSeconds);
        }
    }
    return {};
}
