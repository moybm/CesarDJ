#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Track.h"
#include "DJLookAndFeel.h"

namespace WaveDrawing
{
    // Máximo de los bins [first, last) (para cuando varios bins caen en el mismo píxel)
    inline WaveBin maxOfRange (const std::vector<WaveBin>& wave, long first, long last)
    {
        WaveBin out;
        first = juce::jmax (0L, first);
        last = juce::jmin ((long) wave.size(), juce::jmax (last, first + 1));
        for (long i = first; i < last; ++i)
        {
            out.low  = juce::jmax (out.low,  wave[(size_t) i].low);
            out.mid  = juce::jmax (out.mid,  wave[(size_t) i].mid);
            out.high = juce::jmax (out.high, wave[(size_t) i].high);
        }
        return out;
    }

    // Una columna de forma de onda simétrica, con las 3 bandas superpuestas
    inline void drawColumn (juce::Graphics& g, float x, float width, float centreY, float halfHeight,
                            const WaveBin& bin, float alpha = 1.0f)
    {
        const float lo = bin.low  / 255.0f * halfHeight;
        const float mi = bin.mid  / 255.0f * halfHeight * 0.85f;
        const float hi = bin.high / 255.0f * halfHeight * 0.6f;

        g.setColour (DJColours::waveLow.withMultipliedAlpha (alpha));
        g.fillRect (x, centreY - lo, width, lo * 2.0f);
        g.setColour (DJColours::waveMid.withMultipliedAlpha (0.9f * alpha));
        g.fillRect (x, centreY - mi, width, mi * 2.0f);
        g.setColour (DJColours::waveHigh.withMultipliedAlpha (0.85f * alpha));
        g.fillRect (x, centreY - hi, width, hi * 2.0f);
    }
}
