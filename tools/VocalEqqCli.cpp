/** vocaleqq-cli — dieselbe Kette wie das Plugin, offline über eine WAV-Datei.

    Zum Hörvergleich mit dem Python-Vorbild (vocaleq chain) und für Messungen
    ohne DAW. Die Latenz wird ausgeglichen: Die Ausgabe liegt sample-genau
    auf der Eingabe.

      vocaleqq-cli take.wav [take_vocaleqq.wav] [Schalter]

      --no-deess  --deess-db <0..12>  --sensitivity <0..1>
      --no-eq     --strength <0..1>   --low-cut <0..8>
      --no-comp   --amount <0..1>
      --mix <0..1>  --no-auto-gain  --block <Samples>
*/

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "WavIO.h"
#include "dsp/VocalChain.h"

namespace
{
    void usage()
    {
        std::puts ("Aufruf: vocaleqq-cli <eingang.wav> [ausgang.wav] [Schalter]\n"
                   "  --no-deess --deess-db <0..12> --sensitivity <0..1>\n"
                   "  --no-eq --strength <0..1> --low-cut <0..8>\n"
                   "  --no-comp --amount <0..1>\n"
                   "  --mix <0..1> --no-auto-gain --block <Samples>");
    }
}

#ifdef _WIN32
int wmain (int argc, wchar_t** argv)
{
    std::vector<std::filesystem::path> args (argv + 1, argv + argc);
#else
int main (int argc, char** argv)
{
    std::vector<std::filesystem::path> args (argv + 1, argv + argc);
#endif
    using namespace vocaleqq;

    ChainSettings s;
    std::filesystem::path input, output;
    int blockSize = 512;

    for (size_t i = 0; i < args.size(); ++i)
    {
        const std::string a = args[i].string();
        auto next = [&] () -> float
        {
            if (i + 1 >= args.size()) { usage(); std::exit (2); }
            return std::strtof (args[++i].string().c_str(), nullptr);
        };

        if (a == "--no-deess")           s.deEssEnabled = false;
        else if (a == "--deess-db")      s.deEssReductionDb = next();
        else if (a == "--sensitivity")   s.deEssSensitivity = next();
        else if (a == "--no-eq")         s.eqEnabled = false;
        else if (a == "--strength")      s.eqStrength = next();
        else if (a == "--low-cut")       s.eqLowCutDb = next();
        else if (a == "--no-comp")       s.compEnabled = false;
        else if (a == "--amount")        s.compAmount = next();
        else if (a == "--mix")           s.mix = next();
        else if (a == "--no-auto-gain")  s.autoGain = false;
        else if (a == "--block")         blockSize = static_cast<int> (next());
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (input.empty())          input = args[i];
        else if (output.empty())         output = args[i];
        else { usage(); return 2; }
    }

    if (input.empty())
    {
        usage();
        return 2;
    }

    if (output.empty())
        output = input.parent_path() / (input.stem().string() + "_vocaleqq.wav");

    std::string error;
    auto audio = wav::load (input, error);
    if (audio.channels.empty())
    {
        std::fprintf (stderr, "Fehler: %s\n", error.c_str());
        return 1;
    }

    // Die Kette kann Mono und Stereo. Mehr Kanäle: die ersten beiden.
    if (audio.getNumChannels() > 2)
        audio.channels.resize (2);

    const int numChannels = audio.getNumChannels();
    const int numSamples = audio.getNumSamples();

    VocalChain chain;
    chain.prepare (audio.sampleRate, blockSize, numChannels);
    chain.setSettings (s);
    chain.reset();

    const int latency = chain.getLatencySamples();

    // Hinten um die Latenz verlängern, vorne dieselbe Zahl abschneiden.
    for (auto& ch : audio.channels)
        ch.resize (static_cast<size_t> (numSamples + latency), 0.0f);

    std::vector<float*> pointers (static_cast<size_t> (numChannels));
    for (int start = 0; start < numSamples + latency; start += blockSize)
    {
        const int n = std::min (blockSize, numSamples + latency - start);
        for (int ch = 0; ch < numChannels; ++ch)
            pointers[static_cast<size_t> (ch)] = audio.channels[static_cast<size_t> (ch)].data() + start;
        chain.process (pointers.data(), numChannels, n);
    }

    for (auto& ch : audio.channels)
        ch.erase (ch.begin(), ch.begin() + latency);

    if (! wav::save (output, audio, error))
    {
        std::fprintf (stderr, "Fehler: %s\n", error.c_str());
        return 1;
    }

    std::printf ("%s -> %s\n", input.string().c_str(), output.string().c_str());
    std::printf ("  %d Kanaele, %.0f Hz, %.1f s, Latenz %d Samples (%.1f ms)\n",
                 numChannels, audio.sampleRate, numSamples / audio.sampleRate,
                 latency, 1000.0 * latency / audio.sampleRate);
    std::printf ("  Auto-Gain am Ende: %+.1f dB\n", chain.getAutoGainDb());
    return 0;
}
