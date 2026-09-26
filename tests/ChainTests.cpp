#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestSignals.h"
#include "dsp/VocalChain.h"

using namespace vocaleqq;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sr = 44100.0;

    ChainSettings allOff()
    {
        ChainSettings s;
        s.deEssEnabled = s.eqEnabled = s.compEnabled = false;
        s.autoGain = false;
        return s;
    }

    std::vector<float> render (const ChainSettings& s, std::vector<float> x, int blockSize = 480, int* latency = nullptr)
    {
        VocalChain chain;
        chain.prepare (sr, blockSize, 1);
        chain.setSettings (s);
        chain.reset();
        if (latency != nullptr)
            *latency = chain.getLatencySamples();
        test::run (chain, x, blockSize);
        return x;
    }
}

TEST_CASE ("Kette: feste Latenz, der Impuls kommt dort an", "[chain]")
{
    std::vector<float> impulse (20000, 0.0f);
    impulse[100] = 1.0f;

    int latency = 0;
    const auto out = render (allOff(), impulse, 480, &latency);
    REQUIRE (latency == 1280 + 4096 + 441);

    // Einzige Veränderung bei allem aus: der Allpass der Weiche um 4 kHz.
    // Er verschmiert den Impuls, die Spitze bleibt aber dicht an der Latenz.
    size_t peak = 0;
    for (size_t i = 0; i < out.size(); ++i)
        if (std::abs (out[i]) > std::abs (out[peak]))
            peak = i;

    REQUIRE (static_cast<int> (peak) >= 100 + latency);
    REQUIRE (static_cast<int> (peak) <= 100 + latency + 24);
}

TEST_CASE ("Kette: nass und trocken sind phasengleich, der Mix kämmt nicht", "[chain]")
{
    const auto input = test::noise (44100, 0.5f);

    auto wet = allOff();
    wet.mix = 1.0f;
    auto dry = allOff();
    dry.mix = 0.0f;

    const auto a = render (wet, input);
    const auto b = render (dry, input);

    float maxError = 0.0f;
    for (size_t i = 8000; i < a.size(); ++i)
        maxError = std::max (maxError, std::abs (a[i] - b[i]));
    REQUIRE (maxError < 1.0e-4f);
}

TEST_CASE ("Kette: das Ergebnis hängt nicht an der Blockgröße", "[chain]")
{
    auto input = test::sine (180.0, sr, 3 * 44100, 0.3f);
    const auto hiss = test::noise (3 * 44100, 0.1f, 5);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] += hiss[i];

    const ChainSettings s;
    const auto a = render (s, input, 64);
    const auto b = render (s, input, 1024);

    float maxError = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        maxError = std::max (maxError, std::abs (a[i] - b[i]));
    REQUIRE (maxError < 1.0e-4f);
}

TEST_CASE ("Kette: Auto-Gain gleicht die Lautheit auf den Eingang an", "[chain]")
{
    // Viel Tiefton, viel Low Cut: Ohne Abgleich wird es deutlich leiser.
    const auto input = test::sine (150.0, sr, 12 * 44100, 0.3f);

    auto s = allOff();
    s.eqEnabled = true;
    s.eqStrength = 0.0f;
    s.eqLowCutDb = 8.0f;

    const auto without = render (s, input);
    s.autoGain = true;
    const auto with = render (s, input);

    const int end = 12 * 44100, start = end - 44100;
    const float inDb = test::rmsDb (input, start - 6000, end - 6000);
    REQUIRE (test::rmsDb (without, start, end) < inDb - 5.0f);
    REQUIRE_THAT (test::rmsDb (with, start, end), WithinAbs (inDb, 0.5));
}

TEST_CASE ("Kette: Bypass gibt den trockenen Zweig aus", "[chain]")
{
    const auto input = test::noise (44100, 0.5f);

    ChainSettings s;
    s.bypass = true;
    auto dry = allOff();
    dry.mix = 0.0f;

    const auto a = render (s, input);
    const auto b = render (dry, input);

    float maxError = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        maxError = std::max (maxError, std::abs (a[i] - b[i]));
    REQUIRE (maxError < 1.0e-6f);
}
