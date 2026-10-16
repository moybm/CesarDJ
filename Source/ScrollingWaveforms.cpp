#include "ScrollingWaveforms.h"
#include "WaveDrawing.h"

ScrollingWaveforms::ScrollingWaveforms (DeckEngine& deckA, DeckEngine& deckB)
    : decks { &deckA, &deckB }
{
    setOpaque (true);
}

juce::Rectangle<int> ScrollingWaveforms::laneBounds (int index) const
{
    auto area = getLocalBounds();
    const int half = area.getHeight() / 2;
    return index == 0 ? area.removeFromTop (half) : area.withTrimmedTop (half);
}

double ScrollingWaveforms::pixelsPerTrackSecond (int index) const
{
    // Escala por tempo: a más velocidad, más segundos de canción en pantalla
    const double speed = juce::jmax (0.1, decks[(size_t) index]->getSpeed());
    return getWidth() / (visibleSeconds * speed);
}

void ScrollingWaveforms::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0c0e));

    for (int i = 0; i < 2; ++i)
        paintLane (g, i, laneBounds (i));

    // Separador y cabezal central
    g.setColour (juce::Colours::black);
    g.fillRect (0, getHeight() / 2 - 1, getWidth(), 2);

    const float cx = getWidth() * 0.5f;
    g.setColour (juce::Colours::white);
    g.fillRect (cx - 1.0f, 0.0f, 2.0f, (float) getHeight());

    juce::Path arrows;
    arrows.addTriangle (cx - 6.0f, 0.0f, cx + 6.0f, 0.0f, cx, 7.0f);
    arrows.addTriangle (cx - 6.0f, (float) getHeight(), cx + 6.0f, (float) getHeight(), cx, getHeight() - 7.0f);
    g.fillPath (arrows);
}

void ScrollingWaveforms::paintLane (juce::Graphics& g, int index, juce::Rectangle<int> area)
{
    const auto& deck = *decks[(size_t) index];
    const auto* track = deck.getTrack();
    const auto deckColour = DJColours::forDeck (index);

    g.setColour (DJColours::forDeck (index).withAlpha (0.05f));
    g.fillRect (area);

    if (track == nullptr)
    {
        g.setColour (DJColours::textDim.withAlpha (0.6f));
        g.setFont (13.0f);
        g.drawText (deck.isLoading() ? "Cargando..." : juce::String ("Deck ") + (index == 0 ? "A" : "B"),
                    area, juce::Justification::centred);
        return;
    }

    const double position = deck.getPosition();
    const double pps = pixelsPerTrackSecond (index);
    const float cx = area.getWidth() * 0.5f;
    const float centreY = (float) area.getCentreY();
    const float halfH = area.getHeight() * 0.5f - 3.0f;
    auto timeToX = [&] (double t) { return cx + (float) ((t - position) * pps); };

    const double leftTime = position - cx / pps;
    const double rightTime = position + cx / pps;

    // Loop activo (o roll)
    auto drawRegion = [&] (double start, double end, juce::Colour c)
    {
        const float x1 = timeToX (start), x2 = timeToX (end);
        g.setColour (c.withAlpha (0.22f));
        g.fillRect (x1, (float) area.getY(), x2 - x1, (float) area.getHeight());
        g.setColour (c);
        g.fillRect (x1, (float) area.getY(), 2.0f, (float) area.getHeight());
        g.fillRect (x2 - 2.0f, (float) area.getY(), 2.0f, (float) area.getHeight());
    };
    if (deck.isLooping())  drawRegion (deck.getLoopStart(), deck.getLoopEnd(), juce::Colours::limegreen);
    if (deck.isRolling())  drawRegion (deck.getRollStart(), deck.getRollEnd(), juce::Colours::cyan);

    // Beatgrid: línea fina en cada beat, más marcada cada compás (4 beats)
    if (track->hasBeatgrid())
    {
        const double beatLen = track->beatLength();
        const auto firstBeat = (long) std::floor (track->beatAt (juce::jmax (0.0, leftTime)));
        const auto lastBeat = (long) std::ceil (track->beatAt (rightTime));

        for (long b = firstBeat; b <= lastBeat; ++b)
        {
            const double t = track->timeOfBeat ((double) b);
            if (t < 0.0 || t > track->getLength())
                continue;
            const bool bar = (b % 4) == 0;
            g.setColour (juce::Colours::white.withAlpha (bar ? 0.28f : 0.10f));
            g.fillRect (timeToX (t) - 0.5f, (float) area.getY(), bar ? 1.5f : 1.0f, (float) area.getHeight());
        }
        juce::ignoreUnused (beatLen);
    }

    // Forma de onda
    const double binsPerPixel = Track::binsPerSecond / pps;
    for (int x = 0; x < area.getWidth(); x += 2)
    {
        const double t = leftTime + x / pps;
        if (t < 0.0 || t > track->getLength())
            continue;

        const auto first = (long) (t * Track::binsPerSecond);
        const auto bin = WaveDrawing::maxOfRange (track->wave, first, first + (long) std::ceil (binsPerPixel * 2.0));
        const bool past = x < cx;
        WaveDrawing::drawColumn (g, (float) (area.getX() + x), 2.0f, centreY, halfH, bin, past ? 0.55f : 1.0f);
    }

    // Hot cues y cue
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (int i = 0; i < DeckEngine::numHotCues; ++i)
    {
        const double hc = deck.getHotCue (i);
        if (hc < 0.0 || hc < leftTime || hc > rightTime)
            continue;

        const float x = timeToX (hc);
        g.setColour (DJColours::hotCue (i));
        g.fillRect (x - 1.0f, (float) area.getY(), 2.0f, (float) area.getHeight());
        const auto label = juce::Rectangle<float> (x, (float) area.getY(), 16.0f, 14.0f);
        g.fillRect (label);
        g.setColour (juce::Colours::black);
        g.drawText (juce::String (i + 1), label, juce::Justification::centred);
    }

    const float cueX = timeToX (deck.getCuePoint());
    juce::Path cueMark;
    cueMark.addTriangle (cueX - 6.0f, (float) area.getBottom(), cueX + 6.0f, (float) area.getBottom(),
                         cueX, area.getBottom() - 8.0f);
    g.setColour (DJColours::cue);
    g.fillPath (cueMark);

    // Letra del deck
    const auto badge = juce::Rectangle<float> (6.0f, area.getY() + 5.0f, 18.0f, 18.0f);
    g.setColour (deckColour);
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (index == 0 ? "A" : "B", badge, juce::Justification::centred);
}

void ScrollingWaveforms::mouseDown (const juce::MouseEvent& e)
{
    draggingLane = laneBounds (0).contains (e.getPosition()) ? 0 : 1;
    lastDragX = e.position.x;
    decks[(size_t) draggingLane]->scratchStart();
}

void ScrollingWaveforms::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingLane < 0)
        return;

    const float dx = e.position.x - lastDragX;
    lastDragX = e.position.x;
    decks[(size_t) draggingLane]->scratchMove (-dx / pixelsPerTrackSecond (draggingLane));
}

void ScrollingWaveforms::mouseUp (const juce::MouseEvent&)
{
    if (draggingLane >= 0)
        decks[(size_t) draggingLane]->scratchEnd();
    draggingLane = -1;
}

void ScrollingWaveforms::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    visibleSeconds = juce::jlimit (2.0, 32.0, visibleSeconds * (wheel.deltaY > 0 ? 0.85 : 1.0 / 0.85));
}
