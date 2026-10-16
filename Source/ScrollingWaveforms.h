#pragma once

#include "DeckEngine.h"

// Formas de onda grandes de ambos decks que se desplazan bajo un cabezal fijo central.
// Se escalan con el tempo de cada deck para que dos canciones sincronizadas muestren los beats alineados.
//  - Rueda del ratón: zoom
//  - Arrastrar sobre una onda: scratch
class ScrollingWaveforms : public juce::Component
{
public:
    ScrollingWaveforms (DeckEngine& deckA, DeckEngine& deckB);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void paintLane (juce::Graphics& g, int index, juce::Rectangle<int> area);
    juce::Rectangle<int> laneBounds (int index) const;
    double pixelsPerTrackSecond (int index) const;

    std::array<DeckEngine*, 2> decks;
    double visibleSeconds = 8.0;
    int draggingLane = -1;
    float lastDragX = 0.0f;
    juce::VBlankAttachment vblank { this, [this] { repaint(); } };
};
