#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DeckEngine.h"

// Plato del deck.
//  - Centro: scratch (el audio sigue al plato como un vinilo)
//  - Aro exterior: empujar / frenar (pitch bend) para cuadrar a mano
//  - Rueda del ratón: empujón fino
class JogWheel : public juce::Component,
                 public juce::SettableTooltipClient,
                 private juce::Timer
{
public:
    JogWheel (DeckEngine& engine, juce::Colour deckColour);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Plato de una controladora MIDI (encoder relativo): sonando empuja/frena, en pausa recorre la canción
    void midiTurn (int delta);

private:
    void timerCallback() override;
    float angleAt (juce::Point<float> p) const;
    void renderPlatter();

    DeckEngine& deck;
    juce::Colour colour;
    juce::Image platterImage;
    juce::VBlankAttachment vblank { this, [this] { repaint(); } };

    enum class Mode { none, scratch, bend } mode = Mode::none;
    float lastAngle = 0.0f;
    double lastMoveTime = 0.0;
    double wheelBendUntil = 0.0;
};
