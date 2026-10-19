#pragma once

#include <juce_core/juce_core.h>
#include <set>

// Empareja canciones de una lista externa (Spotify, CSV, M3U...) con las de la biblioteca.
// Ignora mayúsculas, acentos, "feat.", y coletillas como "(Remastered 2011)" o "- Radio Edit";
// las versiones distintas ("Remix", "Extended Mix"...) se mantienen distintas.
namespace TrackMatcher
{
    struct Key
    {
        std::set<juce::String> title, artist;
        double duration = 0.0;
    };

    inline juce::String foldAccents (const juce::String& s)
    {
        static const char* const from[] = { "á","à","ä","â","ã","é","è","ë","ê","í","ì","ï","î","ó","ò","ö","ô","õ","ú","ù","ü","û","ñ","ç","ø","å" };
        static const char* const to[]   = { "a","a","a","a","a","e","e","e","e","i","i","i","i","o","o","o","o","o","u","u","u","u","n","c","o","a" };
        auto r = s.toLowerCase();
        for (size_t i = 0; i < std::size (from); ++i)
            r = r.replace (juce::String::fromUTF8 (from[i]), to[i]);
        return r.replace ("&", " and ");
    }

    // ¿Esta coletilla solo describe la edición (no cambia la canción)?
    inline bool isEditionNote (const juce::String& s)
    {
        for (auto* w : { "remaster", "radio edit", "single version", "album version", "original mix", "explicit",
                         "clean", "mono", "stereo", "bonus track", "deluxe", "edit)", "version)" })
            if (s.contains (w))
                return true;
        return s.trim() == "edit" || s.trim() == "radio" || s.trim() == "original";
    }

    inline std::set<juce::String> tokens (const juce::String& s)
    {
        std::set<juce::String> out;
        juce::String current;
        for (auto c : s)
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                current += c;
            else if (current.isNotEmpty())
            {
                out.insert (current);
                current.clear();
            }
        }
        if (current.isNotEmpty())
            out.insert (current);
        out.erase ("the");
        return out;
    }

    inline juce::String cleanTitle (juce::String t)
    {
        t = foldAccents (t);

        // "(feat. X)", "[feat. X]", " feat. X", " ft. X"
        for (auto* f : { "(feat", "[feat", "(ft.", "[ft.", " feat.", " feat ", " ft. ", " featuring " })
            if (t.contains (f))
                t = t.upToFirstOccurrenceOf (f, false, true);

        // Coletillas de edición entre paréntesis/corchetes
        for (int guard = 0; guard < 4; ++guard)
        {
            const int open = t.indexOfAnyOf ("([");
            if (open < 0)
                break;
            const int close = t.indexOfAnyOf (")]", open);
            if (close < 0)
                break;
            const auto inside = t.substring (open + 1, close);
            if (! isEditionNote (inside + ")"))
                break;
            t = t.substring (0, open) + t.substring (close + 1);
        }

        // "Título - Remastered 2011", "Título - Radio Edit"
        const int dash = t.lastIndexOf (" - ");
        if (dash > 0 && isEditionNote (t.substring (dash + 3) + ")"))
            t = t.substring (0, dash);

        return t.trim();
    }

    inline Key makeKey (const juce::String& title, const juce::String& artist, double durationSeconds)
    {
        Key k;
        k.title = tokens (cleanTitle (title));
        k.artist = tokens (foldAccents (artist));
        k.duration = durationSeconds;
        return k;
    }

    inline double jaccard (const std::set<juce::String>& a, const std::set<juce::String>& b)
    {
        if (a.empty() || b.empty())
            return 0.0;
        int common = 0;
        for (auto& x : a)
            common += b.count (x) > 0 ? 1 : 0;
        return (double) common / (double) (a.size() + b.size() - (size_t) common);
    }

    // 0..1. 'wanted' = canción de la lista externa; 'have' = canción de la biblioteca
    inline double score (const Key& wanted, const Key& have)
    {
        const double t = jaccard (wanted.title, have.title);
        if (t < 0.6)
            return 0.0;

        // Artista: cuántas palabras del artista buscado aparecen en el artista (o el título) de la biblioteca
        double a = 0.5;
        if (! wanted.artist.empty() && ! (have.artist.empty() && have.title.empty()))
        {
            int found = 0;
            for (auto& w : wanted.artist)
                found += (have.artist.count (w) > 0 || have.title.count (w) > 0) ? 1 : 0;
            a = (double) found / (double) wanted.artist.size();
        }

        double s = 0.7 * t + 0.3 * a;
        if (wanted.duration > 0.0 && have.duration > 0.0 && std::abs (wanted.duration - have.duration) > 10.0)
            s *= 0.85;      // misma canción con otra duración suele ser otra versión
        return s;
    }

    constexpr double matchThreshold = 0.8;
}
