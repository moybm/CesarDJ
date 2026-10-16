#include "DeckComponent.h"
#include "DJLookAndFeel.h"

using namespace DJColours;

namespace
{
    const char* const fxNames[] = { "FILTRO", "FLANGER", "CORTAR" };
    const double rollBeats[] = { 0.125, 0.25, 0.5, 1.0 };
    const double jumpBeats[] = { -4.0, -1.0, 1.0, 4.0 };

    juce::String formatTime (double seconds, bool tenths)
    {
        seconds = juce::jmax (0.0, seconds);
        const int mins = (int) seconds / 60;
        const double secs = seconds - mins * 60;
        return juce::String (mins) + ":" + (tenths ? juce::String (secs, 1).paddedLeft ('0', 4)
                                                   : juce::String ((int) secs).paddedLeft ('0', 2));
    }

    juce::String beatsToText (double beats)
    {
        return beats >= 1.0 ? juce::String ((int) beats) : "1/" + juce::String (juce::roundToInt (1.0 / beats));
    }
}

//==============================================================================
void PerformancePad::setLook (const juce::String& newText, juce::Colour newColour, bool isLit, bool isEnabled)
{
    if (newText != text || newColour != colour || isLit != lit || isEnabled != enabled)
    {
        text = newText;
        colour = newColour;
        lit = isLit;
        enabled = isEnabled;
        repaint();
    }
}

void PerformancePad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.5f), 4.0f);

    juce::Colour base = lit ? colour.withMultipliedSaturation (0.85f) : panelLight;
    if (pressed)
        base = lit ? colour.brighter (0.3f) : base.brighter (0.2f);
    if (! enabled)
        base = base.withMultipliedAlpha (0.35f);

    g.setGradientFill (juce::ColourGradient (base.brighter (0.1f), b.getX(), b.getY(),
                                             base.darker (0.2f), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (lit ? colour.brighter (0.5f) : border);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    g.setColour (lit ? juce::Colours::black : (enabled ? DJColours::text : textDim));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void PerformancePad::mouseDown (const juce::MouseEvent& e)
{
    if (! enabled)
        return;
    pressed = true;
    repaint();
    if (onPress != nullptr)
        onPress (true, e.mods);
}

void PerformancePad::mouseUp (const juce::MouseEvent& e)
{
    if (! pressed)
        return;
    pressed = false;
    repaint();
    if (onPress != nullptr)
        onPress (false, e.mods);
}

//==============================================================================
DeckComponent::DeckComponent (int deckIndex, DeckEngine& engine, DeckEngine& otherDeck)
    : index (deckIndex), mirrored (deckIndex == 1), colour (forDeck (deckIndex)),
      deck (engine), other (otherDeck), overview (engine), jog (engine, forDeck (deckIndex))
{
    addAndMakeVisible (overview);
    addAndMakeVisible (jog);

    // ---- Pads
    const char* modeNames[] = { "HOT CUE", "ROLLS", "SALTO", "SAMPLER" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.setColour (juce::TextButton::buttonOnColourId, colour.darker (0.3f));
        b.onClick = [this, i] { setPadMode ((PadMode) i); };
        addAndMakeVisible (b);

        auto& p = pads[(size_t) i];
        p.onPress = [this, i] (bool down, const juce::ModifierKeys& mods) { padPressed (i, down, mods); };
        addAndMakeVisible (p);
    }
    modeButtons[0].setTooltip ("Hot cues: clic = fijar / saltar, clic derecho = borrar");
    modeButtons[1].setTooltip (juce::String::fromUTF8 ("Loop roll mientras mantienes pulsado; al soltar la canción sigue donde estaría"));
    modeButtons[2].setTooltip (juce::String::fromUTF8 ("Saltar beats hacia atrás o adelante"));
    modeButtons[3].setTooltip (juce::String::fromUTF8 ("Sampler: llegará en la Fase 5"));
    modeButtons[3].setEnabled (false);

    // ---- FX
    for (int i = 0; i < 3; ++i)
    {
        auto& k = fxKnobs[(size_t) i];
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRange (i == 0 ? -1.0 : 0.0, 1.0);
        k.setValue (0.0, juce::dontSendNotification);
        k.setDoubleClickReturnValue (true, 0.0);
        k.setColour (juce::Slider::rotarySliderFillColourId, colour);
        if (i == 0)
            DJLookAndFeel::setBipolar (k);
        addAndMakeVisible (k);
    }
    fxKnobs[0].setTooltip ("Izquierda: paso bajo | Derecha: paso alto | Doble clic: centro");
    fxKnobs[1].setTooltip ("Cantidad de flanger");
    fxKnobs[2].setTooltip (juce::String::fromUTF8 ("Corta el audio al ritmo: 1/2, 1/4 o 1/8 de beat según la posición"));
    fxKnobs[0].onValueChange = [this] { deck.fxFilter  = (float) fxKnobs[0].getValue(); };
    fxKnobs[1].onValueChange = [this] { deck.fxFlanger = (float) fxKnobs[1].getValue(); };
    fxKnobs[2].onValueChange = [this] { deck.fxCut     = (float) fxKnobs[2].getValue(); };

    // ---- Loop
    loopHalve.onClick  = [this] { deck.setLoopBeats (deck.getLoopBeats() * 0.5); };
    loopDouble.onClick = [this] { deck.setLoopBeats (deck.getLoopBeats() * 2.0); };
    loopSize.onClick   = [this] { deck.toggleLoop(); };
    loopSize.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2f8f3f));
    loopSize.setTooltip (juce::String::fromUTF8 ("Activa / desactiva un loop de este número de beats"));
    for (auto* b : { &loopHalve, &loopSize, &loopDouble })
        addAndMakeVisible (b);

    // ---- Transporte
    cueButton.setColour (juce::TextButton::textColourOffId, cue);
    cueButton.onClick = [this] { deck.cue(); };
    cueButton.setTooltip ("En pausa: fija el cue | Sonando: vuelve al cue");

    playButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6 II"));
    playButton.setColour (juce::TextButton::buttonOnColourId, play.darker (0.2f));
    playButton.onClick = [this] { deck.togglePlay(); };

    syncButton.setColour (juce::TextButton::buttonOnColourId, colour.darker (0.2f));
    syncButton.onClick = [this] { sync (true); };
    syncButton.setTooltip ("Iguala el BPM del otro deck y alinea los beats");

    // ---- Pitch
    matchButton.onClick = [this] { sync (false); };
    matchButton.setTooltip ("Iguala solo el BPM con el otro deck");

    keyButton.setColour (juce::TextButton::buttonOnColourId, colour.darker (0.2f));
    keyButton.onClick = [this] { deck.setKeyLock (! deck.getKeyLock()); };
    keyButton.setTooltip ("Key lock: cambia el tempo sin cambiar el tono");

    rangeButton.onClick = [this] { cyclePitchRange(); };
    rangeButton.setTooltip ("Rango del pitch");

    pitchSlider.setRange (-pitchRange, pitchRange, 0.01);
    pitchSlider.setValue (0.0, juce::dontSendNotification);
    pitchSlider.setDoubleClickReturnValue (true, 0.0);
    pitchSlider.setColour (juce::Slider::trackColourId, colour.withAlpha (0.7f));
    pitchSlider.setColour (juce::Slider::thumbColourId, colour);
    pitchSlider.onValueChange = [this] { deck.setPitch (pitchSlider.getValue()); };
    pitchSlider.setTooltip ("Pitch / tempo. Doble clic: 0 %");

    for (auto* c : std::initializer_list<juce::Component*> { &cueButton, &playButton, &syncButton,
                                                             &matchButton, &keyButton, &rangeButton, &pitchSlider })
        addAndMakeVisible (c);

    // Ningún control roba el teclado: así los atajos funcionan siempre
    for (auto* c : getChildren())
        c->setWantsKeyboardFocus (false);

    rangeButton.setButtonText (juce::String::fromUTF8 ("\xc2\xb1") + juce::String ((int) pitchRange) + "%");

    deck.onLoadFailed = [] (const juce::File& f)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No se pudo abrir",
                                                juce::String::fromUTF8 ("Formato no soportado o archivo dañado:\n") + f.getFileName());
    };

    setPadMode (PadMode::hotCue);
    startTimerHz (30);
}

DeckComponent::~DeckComponent()
{
    stopTimer();
}

//==============================================================================
void DeckComponent::loadFile (const juce::File& file)
{
    // Protección en directo: no cargar encima de una canción que está sonando
    if (deck.isPlaying())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                juce::String ("Deck ") + (index == 0 ? "A" : "B") + " sonando",
                                                juce::String::fromUTF8 ("Pausa el deck antes de cargar otra canción."));
        return;
    }

    deck.loadFile (file, infoForFile != nullptr ? infoForFile (file) : DeckEngine::TrackInfo());
}

void DeckComponent::sync (bool alignPhase)
{
    double newPitch;
    if (! deck.pitchForBpm (other.getEffectiveBpm(), newPitch))
        return;

    setPitchValue (newPitch);

    if (alignPhase && other.isPlaying())
        deck.alignPhaseWith (other);
}

void DeckComponent::setPitchValue (double percent)
{
    // Si el valor no cabe en el rango actual, ampliar el rango
    while (std::abs (percent) > pitchRange && pitchRange < 50.0)
        cyclePitchRange();

    pitchSlider.setValue (juce::jlimit (-pitchRange, pitchRange, percent), juce::sendNotificationSync);
}

void DeckComponent::cyclePitchRange()
{
    pitchRange = pitchRange < 7.0 ? 8.0 : pitchRange < 15.0 ? 16.0 : pitchRange < 49.0 ? 50.0 : 6.0;

    const double current = juce::jlimit (-pitchRange, pitchRange, pitchSlider.getValue());
    pitchSlider.setRange (-pitchRange, pitchRange, 0.01);
    pitchSlider.setValue (current);
    rangeButton.setButtonText (juce::String::fromUTF8 ("\xc2\xb1") + juce::String ((int) pitchRange) + "%");
}

//==============================================================================
void DeckComponent::setPadMode (PadMode mode)
{
    deck.endRoll();
    padMode = mode;

    for (int i = 0; i < 4; ++i)
        modeButtons[(size_t) i].setToggleState (i == (int) mode, juce::dontSendNotification);

    updatePads();
}

void DeckComponent::padPressed (int pad, bool isDown, const juce::ModifierKeys& mods)
{
    switch (padMode)
    {
        case PadMode::hotCue:
            if (isDown)
            {
                if (mods.isPopupMenu() || mods.isShiftDown())
                    deck.deleteHotCue (pad);
                else
                    deck.triggerHotCue (pad);
            }
            break;

        case PadMode::rolls:
            if (isDown) deck.startRoll (rollBeats[pad]);
            else        deck.endRoll();
            break;

        case PadMode::jump:
            if (isDown)
                deck.beatJump (jumpBeats[pad]);
            break;

        case PadMode::sampler:
            break;
    }

    updatePads();
}

void DeckComponent::updatePads()
{
    for (int i = 0; i < 4; ++i)
    {
        auto& p = pads[(size_t) i];

        switch (padMode)
        {
            case PadMode::hotCue:
            {
                const bool set = deck.getHotCue (i) >= 0.0;
                p.setLook (set ? juce::String (i + 1) : "Cue " + juce::String (i + 1), hotCue (i), set, true);
                p.setTooltip ("Clic: fijar / saltar | Clic derecho: borrar");
                break;
            }
            case PadMode::rolls:
                p.setLook (beatsToText (rollBeats[i]), juce::Colour (0xff35b6c9), false, true);
                p.setTooltip ({});
                break;
            case PadMode::jump:
                p.setLook ((jumpBeats[i] > 0 ? "+" : "") + juce::String ((int) jumpBeats[i]), juce::Colour (0xff9b7be0), false, true);
                p.setTooltip ({});
                break;
            case PadMode::sampler:
                p.setLook ("-", panelLight, false, false);
                break;
        }
    }
}

//==============================================================================
void DeckComponent::timerCallback()
{
    playButton.setToggleState (deck.isPlaying(), juce::dontSendNotification);
    loopSize.setToggleState (deck.isLooping(), juce::dontSendNotification);
    loopSize.setButtonText (beatsToText (deck.getLoopBeats()));
    keyButton.setToggleState (deck.getKeyLock(), juce::dontSendNotification);

    // Parpadeo del CUE en pausa (como un CDJ) cuando no estamos en el punto de cue
    const bool paused = ! deck.isPlaying() && deck.getTrack() != nullptr;
    const bool atCue = std::abs (deck.getPosition() - deck.getCuePoint()) < 0.02;
    const bool blink = (juce::Time::getMillisecondCounter() / 500) % 2 == 0;
    cueButton.setColour (juce::TextButton::buttonColourId,
                         paused && (atCue || blink) ? cue.withAlpha (0.35f) : panelLight);

    const bool synced = deck.getEffectiveBpm() > 0.0
                        && std::abs (deck.getEffectiveBpm() - other.getEffectiveBpm()) < 0.01;
    syncButton.setToggleState (synced, juce::dontSendNotification);

    if (padMode == PadMode::hotCue)
        updatePads();

    repaint (headerArea);
    repaint (pitchLabelArea);
}

//==============================================================================
void DeckComponent::paintSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour (border);
    const int textW = 60;
    const int y = area.getCentreY();
    g.fillRect (area.getX(), y, (area.getWidth() - textW) / 2, 1);
    g.fillRect (area.getCentreX() + textW / 2, y, (area.getWidth() - textW) / 2, 1);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (title, area.withSizeKeepingCentre (textW, area.getHeight()), juce::Justification::centred);
}

void DeckComponent::paintHeader (juce::Graphics& g)
{
    auto area = headerArea;
    g.setColour (inset);
    g.fillRect (area);

    // Letra del deck
    auto badge = mirrored ? area.removeFromRight (26) : area.removeFromLeft (26);
    g.setColour (colour);
    g.fillRect (badge);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText (index == 0 ? "A" : "B", badge, juce::Justification::centred);

    const auto* track = deck.getTrack();
    const double pos = deck.getPosition();
    const double len = deck.getLength();

    // Tiempo restante
    auto timeBox = mirrored ? area.removeFromLeft (104) : area.removeFromRight (104);
    const bool ending = track != nullptr && len - pos < 30.0 && deck.isPlaying();
    g.setColour (ending && (juce::Time::getMillisecondCounter() / 400) % 2 == 0 ? juce::Colours::red : DJColours::text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (track != nullptr ? "-" + formatTime (len - pos, true) : "--:--",
                timeBox.withTrimmedBottom (18).reduced (8, 0), juce::Justification::centredRight);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (track != nullptr ? formatTime (pos, false) + " / " + formatTime (len, false) : "",
                timeBox.withTrimmedTop (timeBox.getHeight() - 20).reduced (8, 0), juce::Justification::centredRight);

    // BPM
    auto bpmBox = mirrored ? area.removeFromLeft (92) : area.removeFromRight (92);
    g.setColour (border);
    g.fillRect (bpmBox.getX(), bpmBox.getY() + 6, 1, bpmBox.getHeight() - 12);
    g.fillRect (bpmBox.getRight() - 1, bpmBox.getY() + 6, 1, bpmBox.getHeight() - 12);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("BPM", bpmBox.reduced (8, 5), juce::Justification::topLeft);

    juce::String bpmText = "---";
    if (track != nullptr)
        bpmText = track->hasBeatgrid() ? juce::String (deck.getEffectiveBpm(), 1) : "...";
    g.setColour (DJColours::text);
    g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    g.drawText (bpmText, bpmBox.reduced (8, 0).withTrimmedTop (12), juce::Justification::centredRight);

    // Título y artista
    auto titleArea = area.reduced (10, 5);
    const auto just = mirrored ? juce::Justification::centredRight : juce::Justification::centredLeft;
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));

    if (deck.isLoading())
    {
        g.setColour (textDim);
        g.drawText ("Cargando...", titleArea, just);
    }
    else if (track == nullptr)
    {
        g.setColour (DJColours::text);
        g.drawText (juce::String::fromUTF8 ("Arrastra una canción a este deck para cargarla"), titleArea, just, true);
    }
    else
    {
        g.setColour (DJColours::text);
        g.drawText (track->title, titleArea.removeFromTop (titleArea.getHeight() * 3 / 5), just, true);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (track->analysed.load() ? track->artist : juce::String ("Analizando BPM..."), titleArea, just, true);
    }
}

void DeckComponent::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    paintHeader (g);

    paintSectionTitle (g, padsTitle, "PADS");
    paintSectionTitle (g, fxTitle, "FX");
    paintSectionTitle (g, loopTitle, "LOOP");

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f));
    for (int i = 0; i < 3; ++i)
        g.drawText (fxNames[i], fxLabels[(size_t) i], juce::Justification::centred);

    const double pitch = deck.getPitch();
    g.setColour (std::abs (pitch) < 0.005 ? textDim : DJColours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ((pitch >= 0 ? "+" : "") + juce::String (pitch, 2) + "%", pitchLabelArea, juce::Justification::centred);

    if (dragHighlight)
    {
        g.setColour (colour.withAlpha (0.15f));
        g.fillRect (getLocalBounds());
        g.setColour (colour);
        g.drawRect (getLocalBounds(), 2);
    }
}

void DeckComponent::resized()
{
    auto area = getLocalBounds();

    headerArea = area.removeFromTop (50);
    auto ov = area.removeFromTop (30);
    if (mirrored) ov.removeFromRight (26); else ov.removeFromLeft (26);
    overview.setBounds (ov);

    area = area.reduced (12, 10);

    // Columnas: [controles] [plato] [pitch]   (en espejo para el deck B)
    const int sideW = juce::jmin (330, area.getWidth() * 2 / 5);
    auto side = mirrored ? area.removeFromRight (sideW) : area.removeFromLeft (sideW);
    auto pitchCol = mirrored ? area.removeFromLeft (66) : area.removeFromRight (66);
    area = area.reduced (14, 0);

    // ---- Controles laterales
    {
        auto s = side;
        padsTitle = s.removeFromTop (18);
        s.removeFromTop (4);

        auto modes = s.removeFromTop (24);
        const int mw = modes.getWidth() / 4;
        for (auto& b : modeButtons)
            b.setBounds (modes.removeFromLeft (mw).reduced (2, 0));

        s.removeFromTop (6);
        auto padRow = s.removeFromTop (juce::jlimit (34, 48, s.getHeight() / 5));
        for (auto& p : pads)
            p.setBounds (padRow.removeFromLeft (mw).reduced (2, 0));

        s.removeFromTop (8);
        fxTitle = s.removeFromTop (18);
        auto fxRow = s.removeFromTop (juce::jlimit (50, 62, s.getHeight() / 3));
        const int fw = fxRow.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto cell = fxRow.removeFromLeft (fw);
            fxLabels[(size_t) i] = cell.removeFromBottom (14);
            fxKnobs[(size_t) i].setBounds (cell.withSizeKeepingCentre (cell.getHeight(), cell.getHeight()));
        }

        s.removeFromTop (6);
        loopTitle = s.removeFromTop (18);
        auto loopRow = s.removeFromTop (34).withSizeKeepingCentre (juce::jmin (220, s.getWidth()), 34);
        const int lw = loopRow.getWidth() / 3;
        loopHalve.setBounds (loopRow.removeFromLeft (lw).reduced (4, 0));
        loopSize.setBounds (loopRow.removeFromLeft (lw).reduced (4, 0));
        loopDouble.setBounds (loopRow.reduced (4, 0));
    }

    // ---- Pitch
    {
        auto p = pitchCol;
        matchButton.setBounds (p.removeFromTop (26));
        p.removeFromTop (4);
        keyButton.setBounds (p.removeFromTop (24));
        rangeButton.setBounds (p.removeFromBottom (22));
        pitchLabelArea = p.removeFromBottom (20);
        pitchSlider.setBounds (p.reduced (0, 4));
    }

    // ---- Plato + transporte
    {
        auto transport = area.removeFromBottom (40);
        area.removeFromBottom (8);
        const int jogSize = juce::jmin (area.getWidth(), area.getHeight());
        jog.setBounds (area.withSizeKeepingCentre (jogSize, jogSize));

        const int bw = juce::jmin (76, (transport.getWidth() - 20) / 3);
        auto row = transport.withSizeKeepingCentre (bw * 3 + 20, transport.getHeight());
        cueButton.setBounds (row.removeFromLeft (bw));
        row.removeFromLeft (10);
        playButton.setBounds (row.removeFromLeft (bw));
        row.removeFromLeft (10);
        syncButton.setBounds (row.removeFromLeft (bw));
    }
}

//==============================================================================
bool DeckComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! files.isEmpty();
}

void DeckComponent::filesDropped (const juce::StringArray& files, int, int)
{
    setDragHighlight (false);
    if (! files.isEmpty())
        loadFile (juce::File (files[0]));
}

bool DeckComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("file:");
}

void DeckComponent::itemDropped (const SourceDetails& details)
{
    setDragHighlight (false);
    loadFile (juce::File (details.description.toString().fromFirstOccurrenceOf ("file:", false, false)
                                                        .upToFirstOccurrenceOf ("|ids:", false, false)));
}
