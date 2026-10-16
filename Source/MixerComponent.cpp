#include "MixerComponent.h"
#include "DJLookAndFeel.h"

using namespace DJColours;

namespace
{
    const char* const knobNames[] = { "GAIN", "HIGH", "MID", "LOW" };
}

MixerComponent::MixerComponent (MixerEngine& engine)
    : mixer (engine)
{
    for (int i = 0; i < MixerEngine::numChannels; ++i)
    {
        auto& s = strips[(size_t) i];
        auto& p = mixer.getChannel (i);
        std::atomic<float>* targets[] = { &p.trimDb, &p.eqHigh, &p.eqMid, &p.eqLow };

        for (int k = 0; k < 4; ++k)
        {
            auto& knob = s.knobs[(size_t) k];
            knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            knob.setRange (k == 0 ? -12.0 : -1.0, k == 0 ? 12.0 : 1.0);
            knob.setValue (0.0, juce::dontSendNotification);
            knob.setDoubleClickReturnValue (true, 0.0);
            knob.setColour (juce::Slider::rotarySliderFillColourId, forDeck (i));
            DJLookAndFeel::setBipolar (knob);
            auto* target = targets[k];
            knob.onValueChange = [&knob, target] { target->store ((float) knob.getValue()); };
            knob.setTooltip (k == 0 ? "Ganancia de entrada (doble clic: 0 dB)"
                                    : "EQ: a la izquierda elimina la banda (doble clic: plano)");
            addAndMakeVisible (knob);
        }

        s.volume.setRange (0.0, 1.0);
        s.volume.setValue (1.0, juce::dontSendNotification);
        s.volume.setDoubleClickReturnValue (true, 1.0);
        s.volume.setColour (juce::Slider::trackColourId, forDeck (i).withAlpha (0.8f));
        s.volume.setColour (juce::Slider::thumbColourId, forDeck (i));
        s.volume.onValueChange = [&s, &p] { p.volume.store ((float) s.volume.getValue()); };
        addAndMakeVisible (s.volume);

        s.headphones.setClickingTogglesState (true);
        s.headphones.setColour (juce::TextButton::buttonOnColourId, forDeck (i).darker (0.2f));
        s.headphones.onClick = [&s, &p] { p.pfl.store (s.headphones.getToggleState()); };
        addAndMakeVisible (s.headphones);

        s.meter = std::make_unique<VuMeter> (p.peakLeft, p.peakRight);
        addAndMakeVisible (*s.meter);
    }

    masterMeter = std::make_unique<VuMeter> (mixer.masterPeakLeft, mixer.masterPeakRight);
    addAndMakeVisible (*masterMeter);

    crossfader.setRange (0.0, 1.0);
    crossfader.setValue (0.5, juce::dontSendNotification);
    crossfader.setDoubleClickReturnValue (true, 0.5);
    crossfader.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    crossfader.onValueChange = [this] { mixer.crossfader.store ((float) crossfader.getValue()); };
    crossfader.setTooltip ("Crossfader (doble clic: centro)");
    addAndMakeVisible (crossfader);

    for (auto* c : getChildren())
        c->setWantsKeyboardFocus (false);

    startTimerHz (4);
}

void MixerComponent::timerCallback()
{
    // Los auriculares solo funcionan con una salida de 4 canales (1-2 master, 3-4 cascos)
    const bool hasCue = mixer.outputChannelCount.load() >= 4;
    for (auto& s : strips)
        s.headphones.setTooltip (hasCue ? juce::String ("Pre-escucha por auriculares (salidas 3-4)")
                                        : juce::String ("Para usar auriculares elige en Audio... una tarjeta con 4 salidas"));
}

void MixerComponent::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    g.setColour (inset);
    g.fillRect (titleArea);
    g.setColour (DJColours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("MEZCLADOR", titleArea, juce::Justification::centred);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f));
    for (auto& s : strips)
        for (int k = 0; k < 4; ++k)
            g.drawText (knobNames[k], s.labels[(size_t) k], juce::Justification::centred);

    g.drawText ("MASTER", masterLabel, juce::Justification::centred);

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (deckA);
    g.drawText ("A", xfLabelA, juce::Justification::centred);
    g.setColour (deckB);
    g.drawText ("B", xfLabelB, juce::Justification::centred);
}

void MixerComponent::resized()
{
    auto area = getLocalBounds();
    titleArea = area.removeFromTop (28);
    area = area.reduced (10, 10);

    auto xfRow = area.removeFromBottom (40);
    xfLabelA = xfRow.removeFromLeft (20);
    xfLabelB = xfRow.removeFromRight (20);
    crossfader.setBounds (xfRow.reduced (6, 2));
    area.removeFromBottom (10);

    const int faderW = 40, meterW = 12, knobColW = 58;

    auto layoutStrip = [&] (Strip& s, juce::Rectangle<int> faderCol, juce::Rectangle<int> knobCol, bool meterOnRight)
    {
        s.headphones.setBounds (faderCol.removeFromBottom (30).reduced (2, 0));
        faderCol.removeFromBottom (8);
        auto meterArea = meterOnRight ? faderCol.removeFromRight (meterW) : faderCol.removeFromLeft (meterW);
        s.meter->setBounds (meterArea.reduced (0, 4));
        s.volume.setBounds (faderCol);

        const int rowH = juce::jmin (68, knobCol.getHeight() / 4);
        for (int k = 0; k < 4; ++k)
        {
            auto row = knobCol.removeFromTop (rowH);
            s.labels[(size_t) k] = row.removeFromBottom (13);
            s.knobs[(size_t) k].setBounds (row.withSizeKeepingCentre (row.getHeight(), row.getHeight()));
        }
    };

    auto faderA = area.removeFromLeft (faderW + meterW);
    auto faderB = area.removeFromRight (faderW + meterW);
    auto knobsA = area.removeFromLeft (knobColW);
    auto knobsB = area.removeFromRight (knobColW);

    layoutStrip (strips[0], faderA, knobsA, true);
    layoutStrip (strips[1], faderB, knobsB, false);

    // Centro: vúmetro master
    masterLabel = area.removeFromBottom (16);
    masterMeter->setBounds (area.withSizeKeepingCentre (juce::jmin (22, area.getWidth()), area.getHeight()).reduced (0, 4));
}
