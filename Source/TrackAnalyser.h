#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Track.h"

namespace TrackAnalyser
{
    // Decodifica el archivo entero en memoria y calcula la forma de onda en 3 bandas.
    // Devuelve nullptr si el formato no se puede leer. 'shouldStop' permite cancelar.
    std::shared_ptr<Track> loadTrack (juce::AudioFormatManager& formats, const juce::File& file,
                                      const std::function<bool()>& shouldStop);

    // Detecta BPM y fase del beatgrid (tempo constante). Rellena track.bpm y track.firstBeat.
    void detectBeatgrid (Track& track, const std::function<bool()>& shouldStop);

    // Detecta la tonalidad (cromagrama + perfiles de Krumhansl). Devuelve el código Camelot o -1.
    int detectKey (const Track& track, const std::function<bool()>& shouldStop);
}
