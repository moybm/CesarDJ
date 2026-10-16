#pragma once

#include "DeckEngine.h"
#include "JogWheel.h"
#include "OverviewWaveform.h"

// Pad de interpretación: avisa al pulsar y al soltar (necesario para los rolls)
class PerformancePad : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    std::function<void (bool isDown, const juce::ModifierKeys&)> onPress;

    void setLook (const juce::String& newText, juce::Colour newColour, bool isLit, bool isEnabled);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    juce::String text;
    juce::Colour colour;
    bool lit = false, enabled = true, pressed = false;
};

// Interfaz completa de un deck. El deck B se dibuja en espejo (controles a la derecha).
class DeckComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::DragAndDropTarget,
                      private juce::Timer
{
public:
    DeckComponent (int deckIndex, DeckEngine& engine, DeckEngine& otherDeck);
    ~DeckComponent() override;

    void loadFile (const juce::File& file);

    // La ventana principal lo conecta con la biblioteca para recuperar BPM y cues ya guardados
    std::function<DeckEngine::TrackInfo (const juce::File&)> infoForFile;
    void sync (bool alignPhase);

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override             { setDragHighlight (true); }
    void fileDragExit (const juce::StringArray&) override                        { setDragHighlight (false); }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails&) override                           { setDragHighlight (true); }
    void itemDragExit (const SourceDetails&) override                            { setDragHighlight (false); }
    void itemDropped (const SourceDetails& details) override;

private:
    enum class PadMode { hotCue, rolls, jump, sampler };

    void timerCallback() override;
    void setPadMode (PadMode mode);
    void padPressed (int pad, bool isDown, const juce::ModifierKeys& mods);
    void updatePads();
    void setPitchValue (double percent);
    void cyclePitchRange();
    void setDragHighlight (bool on)                                               { dragHighlight = on; repaint(); }
    void paintHeader (juce::Graphics& g);
    void paintSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title);

    const int index;
    const bool mirrored;
    const juce::Colour colour;
    DeckEngine& deck;
    DeckEngine& other;

    OverviewWaveform overview;
    JogWheel jog;

    std::array<juce::TextButton, 4> modeButtons;
    std::array<PerformancePad, 4> pads;
    PadMode padMode = PadMode::hotCue;

    std::array<juce::Slider, 3> fxKnobs;
    juce::TextButton loopHalve { "<" }, loopSize, loopDouble { ">" };

    juce::TextButton cueButton { "CUE" }, playButton, syncButton { "SINC." };
    juce::TextButton matchButton { "Emparejar" }, keyButton { "KEY" }, rangeButton;
    juce::Slider pitchSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    double pitchRange = 8.0;

    // Zonas que se pintan a mano
    juce::Rectangle<int> headerArea, padsTitle, fxTitle, loopTitle, pitchLabelArea;
    std::array<juce::Rectangle<int>, 3> fxLabels;

    bool dragHighlight = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckComponent)
};
