#include "JogWheel.h"
#include "DJLookAndFeel.h"
#include "Branding.h"
#include "TagReader.h"

namespace
{
    constexpr double secondsPerRevolution = 1.8;     // 33 1/3 rpm
    constexpr float scratchZone = 0.78f;             // fracción del radio que hace scratch
}

JogWheel::JogWheel (DeckEngine& engine, juce::Colour deckColour)
    : deck (engine), colour (deckColour)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Centro: scratch  |  Aro exterior: frenar / empujar  |  Rueda: ajuste fino");
    startTimerHz (30);
}

void JogWheel::resized()
{
    renderPlatter();
    renderCover();
}

namespace
{
    constexpr float coverZone = 0.62f;               // radio de la carátula, en fracción del vinilo

    // Carátula: la del archivo (ID3, FLAC...) o una imagen de portada en la misma carpeta
    juce::Image findCover (const juce::File& file)
    {
        auto image = TagReader::readCoverArt (file);
        if (image.isValid())
            return image;

        for (auto* name : { "cover", "folder", "front", "album", "albumart" })
            for (auto* ext : { ".jpg", ".jpeg", ".png" })
            {
                const auto f = file.getSiblingFile (juce::String (name) + ext);
                if (f.existsAsFile())
                    if (auto img = juce::ImageFileFormat::loadFrom (f); img.isValid())
                        return img;
            }
        return {};
    }
}

// Al cargar otra canción se lee su carátula en segundo plano (los archivos grandes tardan un poco)
void JogWheel::checkTrackChanged()
{
    const auto track = deck.getTrackShared();
    if (track.get() == coverTrack)
        return;

    coverTrack = track.get();
    coverSource = {};
    coverImage = {};
    if (track == nullptr)
        return;

    std::weak_ptr<bool> weak = alive;
    const auto file = track->file;
    const void* id = coverTrack;
    juce::Thread::launch ([this, weak, file, id]
    {
        auto image = findCover (file);
        juce::MessageManager::callAsync ([this, weak, id, image]
        {
            if (weak.expired() || id != coverTrack)
                return;
            coverSource = image;
            renderCover();
        });
    });
}

// Se escala y recorta una sola vez; al pintar solo se gira
void JogWheel::renderCover()
{
    coverImage = {};
    const int size = juce::jmin (getWidth(), getHeight());
    if (! coverSource.isValid() || size <= 0)
        return;

    const float scale = juce::Component::getApproximateScaleFactorForComponent (this);
    const float r = (size - 4.0f) * 0.5f * scratchZone * coverZone;
    const int px = juce::jmax (8, juce::roundToInt (r * 2.0f * scale));

    coverImage = juce::Image (juce::Image::ARGB, px, px, true);
    juce::Graphics g (coverImage);
    const auto bounds = juce::Rectangle<float> ((float) px, (float) px);

    juce::Path circle;
    circle.addEllipse (bounds);
    g.reduceClipRegion (circle);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (coverSource, bounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);

    // Borde y agujero central, como un vinilo de verdad
    const float s = (float) px;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (bounds.reduced (s * 0.006f), s * 0.012f);
    g.setColour (juce::Colour (0xff0c0d0f));
    g.fillEllipse (bounds.withSizeKeepingCentre (s * 0.075f, s * 0.075f));
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (bounds.withSizeKeepingCentre (s * 0.075f, s * 0.075f), s * 0.006f);
}

// El vinilo se dibuja una vez en una imagen; luego solo se rota la marca
void JogWheel::renderPlatter()
{
    const int size = juce::jmin (getWidth(), getHeight());
    if (size <= 0)
        return;

    const float scale = juce::Component::getApproximateScaleFactorForComponent (this);
    const int px = juce::roundToInt (size * scale);
    platterImage = juce::Image (juce::Image::ARGB, px, px, true);

    juce::Graphics g (platterImage);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto area = juce::Rectangle<float> ((float) size, (float) size).reduced (2.0f);
    const auto c = area.getCentre();
    const float r = area.getWidth() * 0.5f;

    // Aro metálico
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d4148), area.getX(), area.getY(),
                                             juce::Colour (0xff15171a), area.getRight(), area.getBottom(), false));
    g.fillEllipse (area);

    // Puntos estroboscópicos
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    for (int i = 0; i < 96; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * i / 96.0f;
        const auto p = c + juce::Point<float> (std::sin (a), -std::cos (a)) * (r * 0.92f);
        g.fillEllipse (p.x - 1.2f, p.y - 1.2f, 2.4f, 2.4f);
    }

    // Vinilo con surcos
    const float vinylR = r * scratchZone;
    g.setColour (juce::Colour (0xff0c0d0f));
    g.fillEllipse (c.x - vinylR, c.y - vinylR, vinylR * 2, vinylR * 2);
    for (float gr = vinylR * 0.42f; gr < vinylR - 2.0f; gr += 2.6f)
    {
        g.setColour (juce::Colours::white.withAlpha (gr > vinylR * 0.95f ? 0.08f : 0.035f));
        g.drawEllipse (c.x - gr, c.y - gr, gr * 2, gr * 2, 0.8f);
    }

    // Brillo
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), c.x - vinylR, c.y - vinylR,
                                             juce::Colours::transparentWhite, c.x, c.y, true));
    g.fillEllipse (c.x - vinylR, c.y - vinylR, vinylR * 2, vinylR * 2);

    // Etiqueta central
    const float labelR = vinylR * 0.38f;
    g.setGradientFill (juce::ColourGradient (colour.brighter (0.2f), c.x, c.y - labelR,
                                             colour.darker (0.6f), c.x, c.y + labelR, false));
    g.fillEllipse (c.x - labelR, c.y - labelR, labelR * 2, labelR * 2);
    if (auto mark = Branding::createMark (juce::Colours::black.withAlpha (0.85f)))
        mark->drawWithin (g, juce::Rectangle<float> (labelR * 1.3f, labelR * 1.3f).withCentre (c),
                          juce::RectanglePlacement::centred, 1.0f);
}

void JogWheel::paint (juce::Graphics& g)
{
    const int size = juce::jmin (getWidth(), getHeight());
    const auto area = juce::Rectangle<float> ((float) size, (float) size)
                          .withCentre (getLocalBounds().toFloat().getCentre()).reduced (2.0f);
    const auto c = area.getCentre();
    const float r = area.getWidth() * 0.5f;
    const float vinylR = r * scratchZone;

    const double position = deck.getPosition();
    const float rotation = (float) (juce::MathConstants<double>::twoPi * position / secondsPerRevolution);

    if (platterImage.isValid())
    {
        // Solo la parte del vinilo gira; el aro también, da igual porque es simétrico
        g.drawImage (platterImage, area.expanded (2.0f), juce::RectanglePlacement::stretchToFit);
    }

    // Carátula de la canción en el centro, girando con el disco
    if (coverImage.isValid())
    {
        const float coverR = vinylR * coverZone;
        const float k = coverR * 2.0f / (float) coverImage.getWidth();
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImageTransformed (coverImage,
                                juce::AffineTransform::translation (-coverImage.getWidth() * 0.5f, -coverImage.getHeight() * 0.5f)
                                    .scaled (k)
                                    .rotated (rotation)
                                    .translated (c.x, c.y));
    }

    // Marca de giro sobre el vinilo (por fuera de la carátula)
    juce::Path marker;
    marker.addRoundedRectangle (-2.0f, -vinylR + 3.0f, 4.0f, vinylR * (coverImage.isValid() ? 0.3f : 0.5f), 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillPath (marker, juce::AffineTransform::rotation (rotation).translated (c.x, c.y));

    // Aro de progreso de la canción (rojo en los últimos 30 s)
    const double length = deck.getLength();
    if (length > 0.0)
    {
        const float progress = (float) (position / length);
        const bool ending = length - position < 30.0 && deck.isPlaying();
        const bool blink = ending && (juce::Time::getMillisecondCounter() / 400) % 2 == 0;

        const float ringR = r * 0.985f;
        juce::Path bg, ring;
        bg.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (bg, juce::PathStrokeType (3.0f));

        ring.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, progress * juce::MathConstants<float>::twoPi, true);
        g.setColour (ending ? (blink ? juce::Colours::red : juce::Colours::red.withAlpha (0.4f)) : colour);
        g.strokePath (ring, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Halo al tocar el plato
    if (mode != Mode::none)
    {
        g.setColour (colour.withAlpha (0.35f));
        if (mode == Mode::scratch)
            g.drawEllipse (c.x - vinylR, c.y - vinylR, vinylR * 2, vinylR * 2, 3.0f);
        else
            g.drawEllipse (area.reduced (4.0f), 3.0f);
    }
}

float JogWheel::angleAt (juce::Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return std::atan2 (p.y - c.y, p.x - c.x);
}

void JogWheel::mouseDown (const juce::MouseEvent& e)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = juce::jmin (getWidth(), getHeight()) * 0.5f;
    const float dist = e.position.getDistanceFrom (c) / r;

    lastAngle = angleAt (e.position);
    lastMoveTime = juce::Time::getMillisecondCounterHiRes();

    if (dist < scratchZone)
    {
        mode = Mode::scratch;
        deck.scratchStart();
    }
    else
    {
        mode = Mode::bend;
    }
}

void JogWheel::mouseDrag (const juce::MouseEvent& e)
{
    const float angle = angleAt (e.position);
    float delta = angle - lastAngle;
    if (delta > juce::MathConstants<float>::pi)   delta -= juce::MathConstants<float>::twoPi;
    if (delta < -juce::MathConstants<float>::pi)  delta += juce::MathConstants<float>::twoPi;
    lastAngle = angle;

    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jmax (1.0, now - lastMoveTime);
    lastMoveTime = now;

    if (mode == Mode::scratch)
    {
        deck.scratchMove (delta / juce::MathConstants<double>::twoPi * secondsPerRevolution);
    }
    else if (mode == Mode::bend)
    {
        // Velocidad angular -> empujón proporcional (máx. ±30 %)
        const double angularSpeed = delta / dt;     // rad/ms
        deck.setBend (1.0 + juce::jlimit (-0.3, 0.3, angularSpeed * 25.0));
    }
}

void JogWheel::mouseUp (const juce::MouseEvent&)
{
    if (mode == Mode::scratch)
        deck.scratchEnd();

    deck.setBend (1.0);
    mode = Mode::none;
}

void JogWheel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float dir = (wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    deck.setBend (dir > 0.0f ? 1.06 : 0.94);
    wheelBendUntil = juce::Time::getMillisecondCounterHiRes() + 120.0;
}

void JogWheel::midiTurn (int delta)
{
    if (deck.isPlaying())
    {
        deck.setBend (1.0 + juce::jlimit (-0.3, 0.3, delta * 0.02));
        wheelBendUntil = juce::Time::getMillisecondCounterHiRes() + 80.0;
    }
    else
    {
        deck.seek (deck.getPosition() + delta * 0.012);
    }
}

void JogWheel::timerCallback()
{
    checkTrackChanged();

    const double now = juce::Time::getMillisecondCounterHiRes();

    // Soltar el empujón si el ratón deja de moverse o termina el de la rueda
    if (mode == Mode::bend && now - lastMoveTime > 60.0)
        deck.setBend (1.0);

    if (wheelBendUntil > 0.0 && now > wheelBendUntil)
    {
        wheelBendUntil = 0.0;
        if (mode == Mode::none)
            deck.setBend (1.0);
    }
}
