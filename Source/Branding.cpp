#include "Branding.h"
#include "BinaryData.h"

juce::String Branding::version()
{
    return JUCE_APPLICATION_VERSION_STRING;
}

std::unique_ptr<juce::Drawable> Branding::createLogo()
{
    return juce::Drawable::createFromImageData (BinaryData::vespa_svg, BinaryData::vespa_svgSize);
}

std::unique_ptr<juce::Drawable> Branding::createMark (juce::Colour colour)
{
    auto d = juce::Drawable::createFromImageData (BinaryData::vespa_mark_svg, BinaryData::vespa_mark_svgSize);
    if (d != nullptr)
        d->replaceColour (juce::Colours::white, colour);
    return d;
}

juce::Image Branding::createSplashImage()
{
    constexpr int w = 640, h = 400;
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1f24), 0.0f, 0.0f,
                                             juce::Colour (0xff050506), 0.0f, (float) h, false));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 14.0f, 1.0f);

    if (auto logo = createLogo())
        logo->drawWithin (g, juce::Rectangle<float> (220.0f, 40.0f, 200.0f, 200.0f), juce::RectanglePlacement::centred, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
    g.drawText (appName, 0, 252, w, 56, juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("Software DJ  -  v" + version(), 0, 308, w, 24, juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Cargando...", 0, h - 44, w, 20, juce::Justification::centred);
    return img;
}

juce::File Branding::dataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (appName);
}

void Branding::migrateOldData()
{
    const auto newFolder = dataFolder();
    const auto oldFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MiDJ");

    if (newFolder.isDirectory() || ! oldFolder.isDirectory())
        return;

    newFolder.createDirectory();
    for (const auto& entry : juce::RangedDirectoryIterator (oldFolder, false, "*", juce::File::findFiles))
    {
        const auto f = entry.getFile();
        const auto target = f.getFileName() == "MiDJ.settings" ? juce::String ("CesarDJ.settings") : f.getFileName();
        f.copyFileTo (newFolder.getChildFile (target));
    }
}

void Branding::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, appName + "  v" + version(),
        juce::String::fromUTF8 ("Software DJ para uso personal.\n\n"
                                "Datos (biblioteca, ajustes, MIDI):\n") + dataFolder().getFullPathName()
        + juce::String::fromUTF8 ("\n\nActualizar: instala la versión nueva encima; la biblioteca y los ajustes se conservan.\n\n"
                                  "Hecho con JUCE, SoundTouch (LGPL) y SQLite."));
}
