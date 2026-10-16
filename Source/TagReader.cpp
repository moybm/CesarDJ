#include "TagReader.h"

namespace
{
    constexpr size_t maxTagBytes = 16 * 1024 * 1024;

    using Bytes = std::vector<juce::uint8>;

    juce::uint32 be32 (const juce::uint8* p)     { return ((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16) | ((juce::uint32) p[2] << 8) | p[3]; }
    juce::uint32 be24 (const juce::uint8* p)     { return ((juce::uint32) p[0] << 16) | ((juce::uint32) p[1] << 8) | p[2]; }
    juce::uint32 le32 (const juce::uint8* p)     { return ((juce::uint32) p[3] << 24) | ((juce::uint32) p[2] << 16) | ((juce::uint32) p[1] << 8) | p[0]; }
    juce::uint32 syncsafe (const juce::uint8* p) { return ((juce::uint32) (p[0] & 0x7f) << 21) | ((juce::uint32) (p[1] & 0x7f) << 14) | ((juce::uint32) (p[2] & 0x7f) << 7) | (p[3] & 0x7f); }

    // Quita la "desincronización" de ID3 (0xFF 0x00 -> 0xFF)
    Bytes unsynchronise (const juce::uint8* data, size_t size)
    {
        Bytes out;
        out.reserve (size);
        for (size_t i = 0; i < size; ++i)
        {
            out.push_back (data[i]);
            if (data[i] == 0xff && i + 1 < size && data[i + 1] == 0x00)
                ++i;
        }
        return out;
    }

    juce::String decodeUtf16 (const juce::uint8* p, size_t size, bool bigEndian)
    {
        juce::String out;
        for (size_t i = 0; i + 1 < size; i += 2)
        {
            juce::uint32 c = bigEndian ? ((juce::uint32) p[i] << 8 | p[i + 1]) : ((juce::uint32) p[i + 1] << 8 | p[i]);
            if (c == 0)
                break;

            if (c >= 0xd800 && c < 0xdc00 && i + 3 < size)
            {
                const juce::uint32 lo = bigEndian ? ((juce::uint32) p[i + 2] << 8 | p[i + 3]) : ((juce::uint32) p[i + 3] << 8 | p[i + 2]);
                c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
                i += 2;
            }
            out += (juce::juce_wchar) c;
        }
        return out;
    }

    // Texto con el byte de codificación de ID3: 0 = Latin-1, 1 = UTF-16 con BOM, 2 = UTF-16BE, 3 = UTF-8
    juce::String decodeText (int encoding, const juce::uint8* p, size_t size)
    {
        switch (encoding)
        {
            case 1:
                if (size >= 2 && p[0] == 0xfe && p[1] == 0xff) return decodeUtf16 (p + 2, size - 2, true);
                if (size >= 2 && p[0] == 0xff && p[1] == 0xfe) return decodeUtf16 (p + 2, size - 2, false);
                return decodeUtf16 (p, size, false);
            case 2:
                return decodeUtf16 (p, size, true);
            case 3:
                return juce::String::fromUTF8 ((const char*) p, (int) strnlen ((const char*) p, size));
            default:
            {
                juce::String out;
                for (size_t i = 0; i < size && p[i] != 0; ++i)
                    out += (juce::juce_wchar) p[i];
                return out;
            }
        }
    }

    // Longitud de un texto terminado en nulo según la codificación (1 o 2 bytes de terminador)
    size_t terminatedLength (int encoding, const juce::uint8* p, size_t size)
    {
        const bool wide = encoding == 1 || encoding == 2;
        for (size_t i = 0; wide ? i + 1 < size : i < size; i += wide ? 2 : 1)
            if (p[i] == 0 && (! wide || p[i + 1] == 0))
                return i;
        return size;
    }

    // "(17)Rock" o "(17)" -> "Rock" / "17": quitar referencias numéricas de ID3v1
    juce::String cleanGenre (juce::String g)
    {
        g = g.trim();
        if (g.startsWithChar ('(') && g.containsChar (')'))
        {
            const auto rest = g.fromFirstOccurrenceOf (")", false, false).trim();
            if (rest.isNotEmpty())
                return rest;
        }
        return g;
    }

    struct Id3Result
    {
        TagInfo info;
        Bytes coverData;
        bool found = false;
    };

    // Busca un ID3v2 al principio del archivo o en un chunk "ID3 " de WAV/AIFF
    Id3Result parseId3v2 (juce::InputStream& in, bool wantCover)
    {
        Id3Result result;
        juce::uint8 header[10];

        const auto start = in.getPosition();
        if (in.read (header, 10) != 10 || header[0] != 'I' || header[1] != 'D' || header[2] != '3')
        {
            in.setPosition (start);
            return result;
        }

        const int version = header[3];
        const int flags = header[5];
        const size_t tagSize = syncsafe (header + 6);
        if (version < 2 || version > 4 || tagSize == 0 || tagSize > maxTagBytes)
            return result;

        Bytes raw (tagSize);
        if ((size_t) in.read (raw.data(), (int) tagSize) != tagSize)
            return result;

        Bytes tag = (flags & 0x80) && version < 4 ? unsynchronise (raw.data(), raw.size()) : raw;
        size_t pos = 0;

        // Cabecera extendida
        if ((flags & 0x40) && version >= 3 && tag.size() >= 4)
            pos += version == 4 ? syncsafe (tag.data()) : be32 (tag.data()) + 4;

        const size_t idLen = version == 2 ? 3 : 4;
        const size_t frameHeader = version == 2 ? 6 : 10;
        result.found = true;

        while (pos + frameHeader <= tag.size())
        {
            const auto* f = tag.data() + pos;
            if (f[0] == 0)
                break;      // relleno

            const juce::String id ((const char*) f, idLen);
            size_t size = version == 2 ? be24 (f + 3) : (version == 4 ? syncsafe (f + 4) : be32 (f + 4));
            const int frameFlags = version == 2 ? 0 : (f[8] << 8 | f[9]);
            pos += frameHeader;

            if (size == 0 || pos + size > tag.size())
                break;

            Bytes frame (tag.begin() + (long) pos, tag.begin() + (long) (pos + size));
            pos += size;

            if (version == 4 && (frameFlags & 0x0002))
                frame = unsynchronise (frame.data(), frame.size());
            if (version == 4 && (frameFlags & 0x0001) && frame.size() > 4)
                frame.erase (frame.begin(), frame.begin() + 4);   // indicador de longitud
            if (frameFlags & (version == 4 ? 0x000c : 0x00c0))
                continue;   // comprimido o cifrado: no soportado

            if (frame.empty())
                continue;

            const int enc = frame[0];
            const auto* body = frame.data() + 1;
            const size_t bodySize = frame.size() - 1;
            auto text = [&] { return decodeText (enc, body, bodySize).trim(); };

            auto& t = result.info;
            if      (id == "TIT2" || id == "TT2") t.title = text();
            else if (id == "TPE1" || id == "TP1") t.artist = text();
            else if (id == "TALB" || id == "TAL") t.album = text();
            else if (id == "TCON" || id == "TCO") t.genre = cleanGenre (text());
            else if (id == "TYER" || id == "TYE" || id == "TDRC") t.year = text().substring (0, 4);
            else if (id == "TKEY" || id == "TKE") t.key = text();
            else if (id == "TBPM" || id == "TBP") t.bpm = text().getDoubleValue();
            else if ((id == "COMM" || id == "COM") && bodySize > 3 && t.comment.isEmpty())
            {
                const auto* p = body + 3;      // idioma
                const size_t rest = bodySize - 3;
                const size_t descLen = terminatedLength (enc, p, rest);
                const size_t skip = descLen + ((enc == 1 || enc == 2) ? 2 : 1);
                if (skip < rest)
                    t.comment = decodeText (enc, p + skip, rest - skip).trim();
            }
            else if (wantCover && result.coverData.empty() && (id == "APIC" || id == "PIC"))
            {
                size_t p = 0;
                if (id == "PIC")
                {
                    p = 3;    // formato de 3 letras
                }
                else
                {
                    while (p < bodySize && body[p] != 0) ++p;   // tipo MIME
                    ++p;
                }
                ++p;          // tipo de imagen
                if (p < bodySize)
                {
                    p += terminatedLength (enc, body + p, bodySize - p) + ((enc == 1 || enc == 2) ? 2 : 1);
                    if (p < bodySize)
                        result.coverData.assign (body + p, body + bodySize);
                }
            }
        }

        return result;
    }

    void parseId3v1 (juce::FileInputStream& in, TagInfo& t)
    {
        if (in.getTotalLength() < 128)
            return;

        juce::uint8 b[128];
        in.setPosition (in.getTotalLength() - 128);
        if (in.read (b, 128) != 128 || b[0] != 'T' || b[1] != 'A' || b[2] != 'G')
            return;

        auto field = [&b] (int offset, int len) { return decodeText (0, b + offset, (size_t) len).trim(); };
        if (t.title.isEmpty())  t.title = field (3, 30);
        if (t.artist.isEmpty()) t.artist = field (33, 30);
        if (t.album.isEmpty())  t.album = field (63, 30);
        if (t.year.isEmpty())   t.year = field (93, 4);
    }

    // FLAC: bloques VORBIS_COMMENT (4) y PICTURE (6)
    bool parseFlac (juce::FileInputStream& in, TagInfo& t, Bytes* cover)
    {
        char magic[4];
        if (in.read (magic, 4) != 4 || memcmp (magic, "fLaC", 4) != 0)
            return false;

        for (int blocks = 0; blocks < 64; ++blocks)
        {
            juce::uint8 h[4];
            if (in.read (h, 4) != 4)
                break;

            const bool last = (h[0] & 0x80) != 0;
            const int type = h[0] & 0x7f;
            const size_t len = be24 (h + 1);

            if ((type == 4 || (type == 6 && cover != nullptr && cover->empty())) && len < maxTagBytes)
            {
                Bytes b (len);
                if ((size_t) in.read (b.data(), (int) len) != len)
                    break;

                if (type == 4)
                {
                    size_t p = 0;
                    auto u32 = [&] { const auto v = p + 4 <= len ? le32 (b.data() + p) : 0u; p += 4; return v; };
                    p += u32();                       // vendor
                    const auto count = u32();
                    for (juce::uint32 i = 0; i < count && p < len; ++i)
                    {
                        const auto l = u32();
                        if (p + l > len)
                            break;
                        const auto entry = juce::String::fromUTF8 ((const char*) b.data() + p, (int) l);
                        p += l;

                        const auto key = entry.upToFirstOccurrenceOf ("=", false, false).toUpperCase();
                        const auto value = entry.fromFirstOccurrenceOf ("=", false, false).trim();
                        if      (key == "TITLE")       t.title = value;
                        else if (key == "ARTIST")      t.artist = value;
                        else if (key == "ALBUM")       t.album = value;
                        else if (key == "GENRE")       t.genre = value;
                        else if (key == "DATE")        t.year = value.substring (0, 4);
                        else if (key == "BPM")         t.bpm = value.getDoubleValue();
                        else if (key == "INITIALKEY")  t.key = value;
                        else if (key == "COMMENT" || key == "DESCRIPTION") t.comment = value;
                    }
                }
                else
                {
                    size_t p = 4;
                    auto skipLen = [&] { if (p + 4 <= len) p += 4 + be32 (b.data() + p); };
                    skipLen();          // MIME
                    skipLen();          // descripción
                    p += 16;            // ancho, alto, profundidad, colores
                    if (p + 4 <= len)
                    {
                        const auto dataLen = be32 (b.data() + p);
                        p += 4;
                        if (p + dataLen <= len)
                            cover->assign (b.begin() + (long) p, b.begin() + (long) (p + dataLen));
                    }
                }
            }
            else
            {
                in.setPosition (in.getPosition() + (juce::int64) len);
            }

            if (last)
                break;
        }
        return true;
    }

    // WAV/AIFF guardan a veces un ID3 dentro de un chunk "id3 " / "ID3 "
    bool seekRiffId3 (juce::FileInputStream& in)
    {
        char id[4];
        in.setPosition (0);
        if (in.read (id, 4) != 4)
            return false;

        const bool riff = memcmp (id, "RIFF", 4) == 0;
        const bool aiff = memcmp (id, "FORM", 4) == 0;
        if (! riff && ! aiff)
            return false;

        in.setPosition (12);
        while (! in.isExhausted())
        {
            juce::uint8 h[8];
            if (in.read (h, 8) != 8)
                return false;
            const auto size = riff ? le32 (h + 4) : be32 (h + 4);
            if (juce::String ((const char*) h, 4).equalsIgnoreCase ("id3 "))
                return true;
            in.setPosition (in.getPosition() + size + (size & 1));
        }
        return false;
    }

    void fillFromFileName (const juce::File& file, TagInfo& t)
    {
        const auto name = file.getFileNameWithoutExtension();
        const int dash = name.indexOf (" - ");

        if (t.title.isEmpty())
            t.title = dash > 0 ? name.substring (dash + 3).trim() : name;
        if (t.artist.isEmpty() && dash > 0)
            t.artist = name.substring (0, dash).trim();
    }

    TagInfo readInternal (const juce::File& file, Bytes* cover)
    {
        TagInfo t;
        juce::FileInputStream in (file);

        if (in.openedOk())
        {
            if (file.hasFileExtension ("flac"))
            {
                parseFlac (in, t, cover);
            }
            else
            {
                auto r = parseId3v2 (in, cover != nullptr);
                if (! r.found && seekRiffId3 (in))
                    r = parseId3v2 (in, cover != nullptr);

                t = r.info;
                if (cover != nullptr)
                    *cover = std::move (r.coverData);

                if (file.hasFileExtension ("mp3"))
                    parseId3v1 (in, t);
            }
        }

        fillFromFileName (file, t);
        return t;
    }
}

TagInfo TagReader::read (const juce::File& file)
{
    return readInternal (file, nullptr);
}

juce::Image TagReader::readCoverArt (const juce::File& file)
{
    Bytes cover;
    readInternal (file, &cover);
    return cover.empty() ? juce::Image() : juce::ImageFileFormat::loadFrom (cover.data(), cover.size());
}
