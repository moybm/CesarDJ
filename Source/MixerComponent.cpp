#include "MixerComponent.h"
#include "DJLookAndFeel.h"

using namespace DJColours;

namespace
{
    const char* const knobNames[] = { "GAIN", "HIGH", "MID", "LOW" };
    const char* const stemNames[] = { "VOCAL", "INSTRU", "BEAT" };
}

MixerComponent::MixerComponent (MixerEngine& engine, SamplerEngine& sampler)
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

        const juce::String id = i == 0 ? "A." : "B.";
        const char* knobIds[] = { "gain", "high", "mid", "low" };
        for (int k = 0; k < 4; ++k)
            s.knobs[(size_t) k].setComponentID (id + knobIds[k]);
        s.volume.setComponentID (id + "volume");
        s.headphones.setComponentID (id + "pfl");

        const char* stemIds[] = { "vocal", "instru", "beat" };
        for (int k = 0; k < 3; ++k)
        {
            auto& knob = s.stemKnobs[(size_t) k];
            knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            knob.setRange (-1.0, 1.0);
            knob.setValue (0.0, juce::dontSendNotification);
            knob.setDoubleClickReturnValue (true, 0.0);
            knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffb05cf0));
            DJLookAndFeel::setBipolar (knob);
            knob.setComponentID (id + stemIds[k]);
            knob.setTooltip (juce::String::fromUTF8 ("Izquierda: quita este stem | Derecha: lo aísla (baja los demás) | Doble clic: normal"));
            knob.onValueChange = [this, &knob, i, k] { if (onStemChange) onStemChange (i, k, (float) knob.getValue()); };
            addChildComponent (knob);
        }

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
    crossfader.setComponentID ("crossfader");
    addAndMakeVisible (crossfader);

    samplerVolume.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    samplerVolume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    samplerVolume.setRange (0.0, 1.0);
    samplerVolume.setValue (sampler.volume.load(), juce::dontSendNotification);
    samplerVolume.setDoubleClickReturnValue (true, 0.8);
    samplerVolume.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe0912a));
    samplerVolume.setTooltip ("Volumen del sampler");
    samplerVolume.setComponentID ("samplerVolume");
    samplerVolume.onValueChange = [this, &sampler] { sampler.volume = (float) samplerVolume.getValue(); };
    addAndMakeVisible (samplerVolume);

    eqButton.setClickingTogglesState (false);
    stemsButton.setClickingTogglesState (false);
    eqButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4a5260));
    stemsButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb05cf0).darker (0.3f));
    eqButton.setTooltip ("Mandos de EQ: HIGH / MID / LOW");
    stemsButton.setTooltip (juce::String::fromUTF8 ("Mandos de stems: VOCAL / INSTRU / BEAT (separación de la canción)"));
    eqButton.onClick = [this] { setStemMode (false); };
    stemsButton.onClick = [this] { setStemMode (true); };
    eqButton.setComponentID ("mixerEq");
    stemsButton.setComponentID ("mixerStems");
    addAndMakeVisible (eqButton);
    addAndMakeVisible (stemsButton);
    setStemMode (false);

    for (auto* c : getChildren())
        c->setWantsKeyboardFocus (false);

    startTimerHz (4);
}

void MixerComponent::setStemMode (bool shouldUseStems)
{
    stemMode = shouldUseStems;
    eqButton.setToggleState (! stemMode, juce::dontSendNotification);
    stemsButton.setToggleState (stemMode, juce::dontSendNotification);

    for (auto& s : strips)
    {
        for (int k = 1; k < 4; ++k)
            s.knobs[(size_t) k].setVisible (! stemMode);
        for (auto& k : s.stemKnobs)
            k.setVisible (stemMode);
    }

    if (onModeChanged != nullptr)
        onModeChanged (stemMode);
    repaint();
}

void MixerComponent::timerCallback()
{
    // Los auriculares solo funcionan con una salida de 4 canales (1-2 master, 3-4 cascos)
    const bool hasCue = mixer.outputChannelCount.load() >= 4;
    for (auto& s : strips)
        s.headphones.setTooltip (hasCue ? juce::String ("Pre-escucha por auriculares (salidas 3-4)")
                                        : juce::String ("Para usar auriculares elige en Audio... una tarjeta con 4 salidas"));

    if (stemMode)
        repaint (titleArea);
}

void MixerComponent::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    g.setColour (inset);
    g.fillRect (titleArea);
    // Estado de los stems de cada canal (IA lista, IA trabajando, separación rápida)
    if (stemMode && stemStatus != nullptr)
    {
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto st = stemStatus (ch);
            g.setColour (st == "IA" ? play : st.startsWith ("IA") ? cue : textDim);
            g.drawText (st, ch == 0 ? statusA : statusB,
                        ch == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f));
    for (auto& s : strips)
        for (int k = 0; k < 4; ++k)
            g.drawText (stemMode && k > 0 ? stemNames[k - 1] : knobNames[k], s.labels[(size_t) k], juce::Justification::centred);

    g.drawText ("MASTER", masterLabel, juce::Justification::centred);
    g.drawText ("SAMPLER", samplerLabel, juce::Justification::centred);

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
    {
        auto t = titleArea.reduced (8, 4);
        auto buttons = t.withSizeKeepingCentre (128, t.getHeight());
        eqButton.setBounds (buttons.removeFromLeft (52));
        stemsButton.setBounds (buttons.withTrimmedLeft (4));
        statusA = t.withRight (buttons.getX() - 60);
        statusB = t.withLeft (buttons.getRight() + 4);
    }
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
            if (k > 0)
                s.stemKnobs[(size_t) (k - 1)].setBounds (s.knobs[(size_t) k].getBounds());
        }
    };

    auto faderA = area.removeFromLeft (faderW + meterW);
    auto faderB = area.removeFromRight (faderW + meterW);
    auto knobsA = area.removeFromLeft (knobColW);
    auto knobsB = area.removeFromRight (knobColW);

    layoutStrip (strips[0], faderA, knobsA, true);
    layoutStrip (strips[1], faderB, knobsB, false);

    // Centro: volumen del sampler y vúmetro master
    {
        auto knobArea = area.removeFromTop (juce::jmin (56, area.getWidth()));
        samplerLabel = area.removeFromTop (14);
        samplerVolume.setBounds (knobArea.withSizeKeepingCentre (knobArea.getHeight(), knobArea.getHeight()));
        area.removeFromTop (8);
    }
    masterLabel = area.removeFromBottom (16);
    masterMeter->setBounds (area.withSizeKeepingCentre (juce::jmin (22, area.getWidth()), area.getHeight()).reduced (0, 4));
}
