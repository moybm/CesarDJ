#pragma once

#include <juce_core/juce_core.h>

// Tonalidades en notación Camelot (rueda armónica de los DJ): 1A..12A (menores) y 1B..12B (mayores).
// Se guardan como un número 0..23 = (número - 1) * 2 + (B ? 1 : 0); -1 = desconocida.
// Dos canciones mezclan bien si tienen el mismo código, el número vecino con la misma letra,
// o el mismo número con la otra letra (relativa mayor/menor).
namespace Camelot
{
    // Número Camelot de cada tónica (0 = Do ... 11 = Si)
    inline int numberFor (int root, bool minor)
    {
        static const int major[] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 };
        static const int minorKeys[] = { 5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10 };
        return minor ? minorKeys[root % 12] : major[root % 12];
    }

    inline int fromRoot (int root, bool minor)          { return (numberFor (root, minor) - 1) * 2 + (minor ? 0 : 1); }
    inline int number (int code)                        { return code / 2 + 1; }
    inline bool isMajor (int code)                      { return (code % 2) == 1; }

    inline juce::String toString (int code)
    {
        return code < 0 ? juce::String() : juce::String (number (code)) + (isMajor (code) ? "B" : "A");
    }

    // Acepta "8A", "08A", "Am", "A minor", "F#m", "Gbmin", "C", "Ebmaj"...
    inline int parse (const juce::String& text)
    {
        const auto s = text.trim().removeCharacters (" ");
        if (s.isEmpty())
            return -1;

        // Camelot directo
        const auto last = juce::CharacterFunctions::toUpperCase (s.getLastCharacter());
        if ((last == 'A' || last == 'B') && s.dropLastCharacters (1).containsOnly ("0123456789"))
        {
            const int n = s.dropLastCharacters (1).getIntValue();
            if (n >= 1 && n <= 12)
                return (n - 1) * 2 + (last == 'B' ? 1 : 0);
        }

        // Nota + alteración + modo
        static const int noteRoots[] = { 9, 11, 0, 2, 4, 5, 7 };    // A B C D E F G
        const auto first = juce::CharacterFunctions::toUpperCase (s[0]);
        if (first < 'A' || first > 'G')
            return -1;

        int root = noteRoots[first - 'A'];
        int i = 1;
        if (s[i] == '#')                    { ++root; ++i; }
        else if (s[i] == 'b' || s[i] == u'♭') { root += 11; ++i; }

        const auto mode = s.substring (i).toLowerCase();
        bool minor;
        if (mode.isEmpty() || mode == "maj" || mode == "major")                         minor = false;
        else if (mode == "m" || mode == "min" || mode == "minor" || mode == "moll")     minor = true;
        else                                                                              return -1;

        return fromRoot ((root + 12) % 12, minor);
    }

    inline bool compatible (int a, int b)
    {
        if (a < 0 || b < 0)
            return false;
        if (a == b)
            return true;

        const int na = number (a), nb = number (b);
        if (isMajor (a) == isMajor (b))
        {
            const int diff = std::abs (na - nb);
            return diff == 1 || diff == 11;
        }
        return na == nb;
    }
}
