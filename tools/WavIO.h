#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

/** Minimaler WAV-Leser und -Schreiber für das Offline-Werkzeug.

    Leser aus Keyy, Schreiber aus Voxx, beides als Kopie. Bewusst ohne JUCE:
    Das CLI soll sich in Sekunden bauen lassen. Unterstützt wird, was aus
    DAWs und Sample-Packs herausfällt — PCM 8/16/24/32 Bit und 32-Bit-Float,
    beliebige Kanalzahl, auch im Extensible-Format.

    Gegenüber Voxx ergänzt: WAVE_FORMAT_EXTENSIBLE (dort steht das
    eigentliche Format im Subformat; 32-Bit-Float wurde sonst als Ganzzahl
    gelesen), 8 Bit, und Pfade als std::filesystem::path, damit unter
    Windows auch Dateinamen mit Umlauten öffnen.
*/
namespace vocaleqq::wav
{

struct AudioFile
{
    std::vector<std::vector<float>> channels;   ///< deinterleaved
    double sampleRate = 44100.0;

    int getNumChannels() const { return static_cast<int> (channels.size()); }
    int getNumSamples()  const { return channels.empty() ? 0 : static_cast<int> (channels[0].size()); }
};

namespace detail
{
    inline uint32_t readU32 (const unsigned char* p) { return static_cast<uint32_t> (p[0]) | (static_cast<uint32_t> (p[1]) << 8) | (static_cast<uint32_t> (p[2]) << 16) | (static_cast<uint32_t> (p[3]) << 24); }
    inline uint16_t readU16 (const unsigned char* p) { return static_cast<uint16_t> (static_cast<uint16_t> (p[0]) | (static_cast<uint16_t> (p[1]) << 8)); }
}

/** @returns leere channels-Liste bei Fehler; error enthält dann den Grund. */
inline AudioFile load (const std::filesystem::path& path, std::string& error)
{
    AudioFile result;

    std::ifstream file (path, std::ios::binary);
    if (! file)
    {
        error = "Datei nicht lesbar";
        return result;
    }

    std::vector<unsigned char> bytes ((std::istreambuf_iterator<char> (file)), std::istreambuf_iterator<char>());

    if (bytes.size() < 44 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
    {
        error = "Keine gueltige WAV-Datei";
        return result;
    }

    uint16_t format = 0, numChannels = 0, bitsPerSample = 0;
    size_t dataOffset = 0, dataSize = 0;

    // Chunks durchlaufen statt feste Offsets annehmen: DAWs schreiben gern
    // zusätzliche Chunks (LIST, fact, bext) zwischen fmt und data.
    size_t pos = 12;
    while (pos + 8 <= bytes.size())
    {
        const char* id = reinterpret_cast<const char*> (bytes.data() + pos);
        const uint32_t size = detail::readU32 (bytes.data() + pos + 4);
        const size_t body = pos + 8;

        if (std::memcmp (id, "fmt ", 4) == 0 && body + 16 <= bytes.size())
        {
            format        = detail::readU16 (bytes.data() + body);
            numChannels   = detail::readU16 (bytes.data() + body + 2);
            result.sampleRate = detail::readU32 (bytes.data() + body + 4);
            bitsPerSample = detail::readU16 (bytes.data() + body + 14);

            // WAVE_FORMAT_EXTENSIBLE: Das eigentliche Format steht in den
            // ersten zwei Bytes des Subformat-GUIDs.
            if (format == 0xFFFE && size >= 40 && body + 26 <= bytes.size())
                format = detail::readU16 (bytes.data() + body + 24);
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            dataOffset = body;
            dataSize   = std::min (static_cast<size_t> (size), bytes.size() - body);
        }

        pos = body + size + (size & 1u);   // Chunks sind auf gerade Länge gepaddet
    }

    if (numChannels == 0 || dataSize == 0)
    {
        error = "fmt- oder data-Chunk fehlt";
        return result;
    }

    if (format != 1 && format != 3)
    {
        error = "Nicht unterstuetztes WAV-Format " + std::to_string (format) + " (nur PCM und Float)";
        return result;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0)
    {
        error = "Unbekannte Bittiefe";
        return result;
    }

    const size_t numFrames = dataSize / (static_cast<size_t> (bytesPerSample) * numChannels);
    result.channels.assign (numChannels, std::vector<float> (numFrames, 0.0f));

    const unsigned char* p = bytes.data() + dataOffset;

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        for (uint16_t ch = 0; ch < numChannels; ++ch)
        {
            float value = 0.0f;

            if (format == 3 && bitsPerSample == 32)          // IEEE float
            {
                std::memcpy (&value, p, 4);
            }
            else if (format == 3 && bitsPerSample == 64)
            {
                double d = 0.0;
                std::memcpy (&d, p, 8);
                value = static_cast<float> (d);
            }
            else if (format == 1 && bitsPerSample == 8)      // vorzeichenlos
            {
                value = (static_cast<float> (p[0]) - 128.0f) / 128.0f;
            }
            else if (format == 1 && bitsPerSample == 16)
            {
                const auto s = static_cast<int16_t> (detail::readU16 (p));
                value = static_cast<float> (s) / 32768.0f;
            }
            else if (format == 1 && bitsPerSample == 24)
            {
                const int32_t s = (static_cast<int32_t> (p[0]) << 8) | (static_cast<int32_t> (p[1]) << 16) | (static_cast<int32_t> (p[2]) << 24);
                value = static_cast<float> (s >> 8) / 8388608.0f;
            }
            else if (format == 1 && bitsPerSample == 32)
            {
                const auto s = static_cast<int32_t> (detail::readU32 (p));
                value = static_cast<float> (s) / 2147483648.0f;
            }
            else
            {
                error = "Nicht unterstuetzte Bittiefe: " + std::to_string (bitsPerSample);
                result.channels.clear();
                return result;
            }

            result.channels[ch][frame] = value;
            p += bytesPerSample;
        }
    }

    return result;
}

namespace detail
{
    inline void writeU32 (std::ostream& s, uint32_t v)
    {
        const unsigned char b[4] { static_cast<unsigned char> (v), static_cast<unsigned char> (v >> 8),
                                   static_cast<unsigned char> (v >> 16), static_cast<unsigned char> (v >> 24) };
        s.write (reinterpret_cast<const char*> (b), 4);
    }

    inline void writeU16 (std::ostream& s, uint16_t v)
    {
        const unsigned char b[2] { static_cast<unsigned char> (v), static_cast<unsigned char> (v >> 8) };
        s.write (reinterpret_cast<const char*> (b), 2);
    }
}

/** Schreibt 24-Bit-PCM — verlustfrei genug für Hörvergleiche und von jedem
    Player und jeder DAW lesbar (anders als Float-WAV).
*/
inline bool save (const std::filesystem::path& path, const AudioFile& audio, std::string& error)
{
    std::ofstream file (path, std::ios::binary);
    if (! file)
    {
        error = "Datei nicht schreibbar: " + path.string();
        return false;
    }

    const uint16_t numChannels = static_cast<uint16_t> (audio.getNumChannels());
    const uint32_t numFrames   = static_cast<uint32_t> (audio.getNumSamples());
    const uint32_t dataSize    = numFrames * numChannels * 3u;

    file.write ("RIFF", 4);
    detail::writeU32 (file, 36u + dataSize);
    file.write ("WAVE", 4);

    file.write ("fmt ", 4);
    detail::writeU32 (file, 16);
    detail::writeU16 (file, 1);              // PCM
    detail::writeU16 (file, numChannels);
    detail::writeU32 (file, static_cast<uint32_t> (audio.sampleRate));
    detail::writeU32 (file, static_cast<uint32_t> (audio.sampleRate) * numChannels * 3u);
    detail::writeU16 (file, static_cast<uint16_t> (numChannels * 3));
    detail::writeU16 (file, 24);

    file.write ("data", 4);
    detail::writeU32 (file, dataSize);

    for (uint32_t frame = 0; frame < numFrames; ++frame)
    {
        for (uint16_t ch = 0; ch < numChannels; ++ch)
        {
            float v = audio.channels[ch][frame];
            v = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);

            const auto s = static_cast<int32_t> (v * 8388607.0f);
            const unsigned char b[3] { static_cast<unsigned char> (s),
                                       static_cast<unsigned char> (s >> 8),
                                       static_cast<unsigned char> (s >> 16) };
            file.write (reinterpret_cast<const char*> (b), 3);
        }
    }

    return true;
}

} // namespace vocaleqq::wav
