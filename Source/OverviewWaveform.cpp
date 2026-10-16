#include "OverviewWaveform.h"
#include "WaveDrawing.h"

OverviewWaveform::OverviewWaveform (DeckEngine& engine)
    : deck (engine)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (20);
}

float OverviewWaveform::timeToX (double seconds) const
{
    const double length = deck.getLength();
    return length > 0.0 ? (float) (seconds / length * getWidth()) : 0.0f;
}

void OverviewWaveform::rebuildCache()
{
    const auto* track = deck.getTrack();
    cacheTrack = track;

    if (track == nullptr || getWidth() <= 0 || getHeight() <= 0)
    {
        cache = {};
        return;
    }

    const float scale = juce::Component::getApproximateScaleFactorForComponent (this);
    cache = juce::Image (juce::Image::ARGB, juce::roundToInt (getWidth() * scale), juce::roundToInt (getHeight() * scale), true);
    juce::Graphics g (cache);
    g.addTransform (juce::AffineTransform::scale (scale));

    const int w = getWidth();
    const float centre = getHeight() * 0.5f;
    const double binsPerPixel = (double) track->wave.size() / w;

    for (int x = 0; x < w; ++x)
    {
        const auto bin = WaveDrawing::maxOfRange (track->wave, (long) (x * binsPerPixel), (long) ((x + 1) * binsPerPixel));
        WaveDrawing::drawColumn (g, (float) x, 1.0f, centre, centre - 1.0f, bin);
    }
}

void OverviewWaveform::paint (juce::Graphics& g)
{
    g.fillAll (DJColours::inset);

    const auto* track = deck.getTrack();
    if (track == nullptr)
    {
        g.setColour (DJColours::textDim);
        g.setFont (12.0f);
        g.drawText (deck.isLoading() ? "Cargando..." : "", getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (track != cacheTrack)
        rebuildCache();

    g.drawImage (cache, getLocalBounds().toFloat());

    const float h = (float) getHeight();
    const float playX = timeToX (deck.getPosition());

    // Parte ya reproducida, oscurecida
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (0.0f, 0.0f, playX, h);

    if (deck.isLooping())
    {
        const float x1 = timeToX (deck.getLoopStart()), x2 = timeToX (deck.getLoopEnd());
        g.setColour (juce::Colours::limegreen.withAlpha (0.35f));
        g.fillRect (x1, 0.0f, juce::jmax (2.0f, x2 - x1), h);
    }

    for (int i = 0; i < DeckEngine::numHotCues; ++i)
    {
        const double hc = deck.getHotCue (i);
        if (hc >= 0.0)
        {
            g.setColour (DJColours::hotCue (i));
            g.fillRect (timeToX (hc) - 1.0f, 0.0f, 2.0f, h);
        }
    }

    g.setColour (DJColours::cue);
    const float cueX = timeToX (deck.getCuePoint());
    juce::Path tri;
    tri.addTriangle (cueX - 4.0f, 0.0f, cueX + 4.0f, 0.0f, cueX, 5.0f);
    g.fillPath (tri);

    g.setColour (juce::Colours::white);
    g.fillRect (playX - 1.0f, 0.0f, 2.0f, h);
}

void OverviewWaveform::mouseDown (const juce::MouseEvent& e)
{
    if (getWidth() > 0)
        deck.seek (deck.getLength() * juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth()));
}

void OverviewWaveform::mouseDrag (const juce::MouseEvent& e)
{
    mouseDown (e);
}
