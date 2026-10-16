#pragma once

#include "DeckEngine.h"

// Vista general de toda la canción, con posición, cue, hot cues y loop. Clic para saltar.
class OverviewWaveform : public juce::Component,
                         private juce::Timer
{
public:
    explicit OverviewWaveform (DeckEngine& engine);

    void paint (juce::Graphics& g) override;
    void resized() override                 { cacheTrack = nullptr; }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void timerCallback() override           { repaint(); }
    void rebuildCache();
    float timeToX (double seconds) const;

    DeckEngine& deck;
    juce::Image cache;
    const Track* cacheTrack = nullptr;
};
