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
    MixerComponent (MixerEngine& engine, SamplerEngine& sampler);

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Para el Automix: mueven los controles en pantalla (y con ellos el audio)
    void setCrossfader (float value)                 { crossfader.setValue (value, juce::sendNotificationSync); }
    void setEqLow (int channel, float value)         { strips[(size_t) channel].knobs[3].setValue (value, juce::sendNotificationSync); }
    void setChannelVolume (int channel, float value) { strips[(size_t) channel].volume.setValue (value, juce::sendNotificationSync); }

    // Modo de los mandos: EQ (HIGH / MID / LOW) o STEMS (VOCAL / INSTRU / BEAT)
    void setStemMode (bool shouldUseStems);
    bool isStemMode() const                          { return stemMode; }
    std::function<void (int channel, int stem, float value)> onStemChange;
    std::function<juce::String (int channel)> stemStatus;     // "IA", "IA 40%", "RAPIDO"...
    std::function<void (bool)> onModeChanged;

private:
    struct Strip
    {
        std::array<juce::Slider, 4> knobs;          // GAIN, HIGH, MID, LOW
        std::array<juce::Slider, 3> stemKnobs;      // VOCAL, INSTRU, BEAT (en el sitio de HIGH/MID/LOW)
        std::array<juce::Rectangle<int>, 4> labels;
        juce::Slider volume { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
        HeadphoneButton headphones;
        std::unique_ptr<VuMeter> meter;
    };

    void timerCallback() override;

    MixerEngine& mixer;
    std::array<Strip, MixerEngine::numChannels> strips;
    juce::Slider crossfader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider samplerVolume;
    juce::Rectangle<int> samplerLabel;
    std::unique_ptr<VuMeter> masterMeter;
    juce::Rectangle<int> titleArea, masterLabel, xfLabelA, xfLabelB, statusA, statusB;
    juce::TextButton eqButton { "EQ" }, stemsButton { "STEMS" };
    bool stemMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};
