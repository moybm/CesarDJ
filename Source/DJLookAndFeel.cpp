#include "DJLookAndFeel.h"

using namespace DJColours;

DJLookAndFeel::DJLookAndFeel()
{
    setColourScheme ({ background, panel, panelLight, border, text, panelLight,
                       juce::Colours::white, deckA, text });

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, panelLight);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4a5260));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffb8c0d0));
    setColour (juce::Slider::rotarySliderOutlineColourId, inset);
    setColour (juce::Slider::trackColourId, juce::Colour (0xff4a5260));
    setColour (juce::Slider::backgroundColourId, inset);
    setColour (juce::Slider::thumbColourId, juce::Colour (0xffc9ced8));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, inset);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, deckA);
    setColour (juce::ComboBox::backgroundColourId, inset);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, deckA.withAlpha (0.6f));
    setColour (juce::ListBox::backgroundColourId, inset);
    setColour (juce::TreeView::backgroundColourId, inset);
    setColour (juce::TreeView::selectedItemBackgroundColourId, deckA.withAlpha (0.35f));
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, deckA.withAlpha (0.35f));
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, text);
    setColour (juce::TableHeaderComponent::backgroundColourId, panelLight);
    setColour (juce::TableHeaderComponent::textColourId, textDim);
    setColour (juce::TableHeaderComponent::outlineColourId, border);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4a5260));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0d0e10));
    setColour (juce::TooltipWindow::outlineColourId, border);
    setColour (juce::AlertWindow::backgroundColourId, panel);
}

void DJLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);

    // Arco de valor alrededor del mando
    const float arcRadius = radius - 1.5f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (inset);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - fromAngle) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cuerpo del mando con relieve
    const float knobRadius = radius - 6.0f;
    const auto knob = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (knob.translated (0.0f, 1.5f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4f58), knob.getX(), knob.getY(),
                                             juce::Colour (0xff1f2226), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);

    g.setColour (juce::Colour (0xff121316));
    g.drawEllipse (knob, 1.0f);

    const auto inner = knob.reduced (knobRadius * 0.18f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff383c44), inner.getX(), inner.getY(),
                                             juce::Colour (0xff2a2d33), inner.getRight(), inner.getBottom(), false));
    g.fillEllipse (inner);

    // Indicador
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobRadius + 1.5f, 3.0f, knobRadius * 0.55f, 1.5f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

int DJLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
    return slider.isHorizontal() ? 11 : 10;
}

void DJLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const bool vertical = style == juce::Slider::LinearVertical;
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto accent = slider.findColour (juce::Slider::trackColourId);

    // Ranura + marcas de escala
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    constexpr int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (ticks - 1);
        const bool major = i == 0 || i == ticks / 2 || i == ticks - 1;
        if (vertical)
        {
            const float ty = area.getY() + t * area.getHeight();
            const float len = major ? 10.0f : 6.0f;
            g.fillRect (area.getCentreX() - 9.0f - len, ty - 0.5f, len, 1.0f);
            g.fillRect (area.getCentreX() + 9.0f, ty - 0.5f, len, 1.0f);
        }
        else
        {
            const float tx = area.getX() + t * area.getWidth();
            g.fillRect (tx - 0.5f, area.getCentreY() - 14.0f, 1.0f, major ? 6.0f : 4.0f);
        }
    }

    const auto groove = vertical ? juce::Rectangle<float> (area.getCentreX() - 2.5f, area.getY(), 5.0f, area.getHeight())
                                 : juce::Rectangle<float> (area.getX(), area.getCentreY() - 2.5f, area.getWidth(), 5.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (groove, 2.5f);

    // Zona activa (color del deck) desde abajo o desde el centro en el pitch
    const bool fromCentre = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    if (vertical)
    {
        const float startY = fromCentre ? area.getCentreY() : area.getBottom();
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float> (groove.getX() + 1.0f, juce::jmin (startY, sliderPos),
                                            groove.getWidth() - 2.0f, std::abs (startY - sliderPos)));
    }

    // Tapa del fader con estrías
    const auto cap = vertical ? juce::Rectangle<float> (34.0f, 20.0f).withCentre ({ area.getCentreX(), sliderPos })
                              : juce::Rectangle<float> (20.0f, 32.0f).withCentre ({ sliderPos, area.getCentreY() });

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a606b), cap.getX(), cap.getY(),
                                             juce::Colour (0xff2b2f35), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff15171a));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    for (int i = -2; i <= 2; ++i)
    {
        if (vertical)
            g.fillRect (cap.getX() + 5.0f, cap.getCentreY() + i * 3.0f - 0.5f, cap.getWidth() - 10.0f, 1.0f);
        else
            g.fillRect (cap.getCentreX() + i * 3.0f - 0.5f, cap.getY() + 5.0f, 1.0f, cap.getHeight() - 10.0f);
    }

    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    if (vertical)
        g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 4.0f, 2.0f);
    else
        g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 2.0f, 2.0f, cap.getHeight() - 4.0f);
}

void DJLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                          bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 4.0f;

    auto base = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;
    if (! button.isEnabled())
        base = base.withMultipliedAlpha (0.4f);
    if (isDown)
        base = base.darker (0.25f);
    else if (isHighlighted)
        base = base.brighter (0.12f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), corner);

    g.setGradientFill (juce::ColourGradient (base.brighter (0.08f), bounds.getX(), bounds.getY(),
                                             base.darker (0.12f), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (button.getToggleState() ? base.brighter (0.4f) : border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

juce::Font DJLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.45f), juce::Font::bold);
}
