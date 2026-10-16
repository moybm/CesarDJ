#pragma once

#include <juce_graphics/juce_graphics.h>

// Etiquetas de una canción. Si el archivo no tiene, el título/artista salen del nombre "Artista - Título".
struct TagInfo
{
    juce::String title, artist, album, genre, year, comment, key;
    double bpm = 0.0;
};

// Lector propio de etiquetas (JUCE no las lee): ID3v2.2/2.3/2.4 + ID3v1 (MP3, AIFF, WAV) y Vorbis comments (FLAC)
namespace TagReader
{
    TagInfo read (const juce::File& file);
    juce::Image readCoverArt (const juce::File& file);
}
