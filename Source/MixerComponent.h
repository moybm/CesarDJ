#pragma once

#include "MixerEngine.h"
#include "VuMeter.h"

// Botón con icono de auriculares dibujado
class HeadphoneButton : public juce::Button
{
public:
    HeadphoneButton() : juce::Button ("PFL") {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), highlighted, down);

        const auto b = getLocalBounds().toFloat().withSizeKeepingCentre (18.0f, 16.0f);
        juce::Path band;
        band.addCentredArc (b.getCentreX(), b.getBottom() - 5.0f, 8.0f, 9.0f, 0.0f,
                            -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.setColour (getToggleState() ? juce::Colours::white : juce::Colour (0xffaab0bb));
        g.strokePath (band, juce::PathStrokeType (2.0f));
        g.fillRoundedRectangle (b.getX() - 1.0f, b.getBottom() - 7.0f, 5.0f, 8.0f, 1.5f);
        g.fillRoundedRectangle (b.getRight() - 4.0f, b.getBottom() - 7.0f, 5.0f, 8.0f, 1.5f);
    }
};

// Mezclador central:
//  [fader A + vúmetro] [GAIN/HIGH/MID/LOW A] [master] [GAIN/HIGH/MID/LOW B] [vúmetro + fader B]
//  auriculares bajo cada fader y crossfader abajo.
class MixerComponent : public juce::Component,
                       private juce::Timer
{
public:
    explicit MixerComponent (MixerEngine& engine);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Strip
    {
        std::array<juce::Slider, 4> knobs;          // GAIN, HIGH, MID, LOW
        std::array<juce::Rectangle<int>, 4> labels;
        juce::Slider volume { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
        HeadphoneButton headphones;
        std::unique_ptr<VuMeter> meter;
    };

    void timerCallback() override;

    MixerEngine& mixer;
    std::array<Strip, MixerEngine::numChannels> strips;
    juce::Slider crossfader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<VuMeter> masterMeter;
    juce::Rectangle<int> titleArea, masterLabel, xfLabelA, xfLabelB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};
