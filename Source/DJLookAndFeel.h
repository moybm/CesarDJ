#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace DJColours
{
    const juce::Colour background   { 0xff141519 };
    const juce::Colour panel        { 0xff23262c };
    const juce::Colour panelLight   { 0xff2c3037 };
    const juce::Colour inset        { 0xff17191d };
    const juce::Colour border       { 0xff3a3e46 };
    const juce::Colour text         { 0xffd9dde4 };
    const juce::Colour textDim      { 0xff8a909b };
    const juce::Colour deckA        { 0xff2fa6e8 };
    const juce::Colour deckB        { 0xffe8433f };
    const juce::Colour cue          { 0xffe0912a };
    const juce::Colour play         { 0xff36c26b };

    inline juce::Colour forDeck (int index)     { return index == 0 ? deckA : deckB; }

    inline juce::Colour hotCue (int index)
    {
        static const juce::Colour colours[] = { juce::Colour (0xffe8433f), juce::Colour (0xff36c26b),
                                                juce::Colour (0xffb05cf0), juce::Colour (0xffe8c33a) };
        return colours[index % 4];
    }

    // Forma de onda en 3 bandas: graves azul, medios naranja, agudos blanco
    const juce::Colour waveLow  { 0xff2a7de1 };
    const juce::Colour waveMid  { 0xffe8a33a };
    const juce::Colour waveHigh { 0xfff4f4f4 };
}

// Aspecto de hardware DJ para todos los controles
class DJLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DJLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Marca una propiedad en un slider rotatorio para que el arco salga desde el centro (EQ, filtro...)
    static void setBipolar (juce::Slider& s)    { s.getProperties().set ("bipolar", true); }
};
