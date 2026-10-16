#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Vúmetro estéreo vertical. Lee (y reinicia) los picos que deja el hilo de audio.
class VuMeter : public juce::Component,
                private juce::Timer
{
public:
    VuMeter (std::atomic<float>& left, std::atomic<float>& right)
        : peakLeft (left), peakRight (right)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const float barWidth = (area.getWidth() - 2.0f) / 2.0f;

        drawBar (g, area.removeFromLeft (barWidth), levelLeft);
        area.removeFromLeft (2.0f);
        drawBar (g, area, levelRight);
    }

private:
    void timerCallback() override
    {
        // Sube al instante y baja suave
        levelLeft  = juce::jmax (peakLeft.exchange (0.0f),  levelLeft  * 0.85f);
        levelRight = juce::jmax (peakRight.exchange (0.0f), levelRight * 0.85f);
        repaint();
    }

    static void drawBar (juce::Graphics& g, juce::Rectangle<float> bar, float level)
    {
        g.setColour (juce::Colour (0xff0b0c0f));
        g.fillRect (bar);

        constexpr int segments = 24;
        constexpr float minDb = -48.0f, maxDb = 3.0f;
        const float db = juce::Decibels::gainToDecibels (level, minDb);
        const int lit = juce::roundToInt ((db - minDb) / (maxDb - minDb) * segments);
        const float segH = bar.getHeight() / (float) segments;

        for (int i = 0; i < lit; ++i)
        {
            const float segDb = minDb + (maxDb - minDb) * (float) i / (float) segments;
            g.setColour (segDb > 0.0f ? juce::Colours::red
                       : segDb > -9.0f ? juce::Colours::yellow
                                       : juce::Colour (0xff35c759));
            g.fillRect (bar.getX(), bar.getBottom() - (float) (i + 1) * segH + 1.0f, bar.getWidth(), segH - 1.0f);
        }
    }

    std::atomic<float>& peakLeft;
    std::atomic<float>& peakRight;
    float levelLeft = 0.0f, levelRight = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VuMeter)
};
