#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestSignals.h"
#include "dsp/AutoCompressor.h"
#include "dsp/DeEsser.h"
#include "dsp/Filters.h"

using namespace vocaleqq;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sr = 44100.0;

    /** Leiser tiefer Ton, dazu alle 0,5 s ein 100 ms langes Zischen
        (Rauschen über 5 kHz) — grob das, was ein De-Esser an Gesang sieht.
    */
    std::vector<float> sibilantTake (int numSamples, std::vector<bool>* burstMask = nullptr)
    {
        auto x = test::sine (200.0, sr, numSamples, 0.05f);
        auto hiss = test::noise (numSamples, 0.5f, 7);

        Crossover split;
        split.prepare (5000.0, sr);
        for (auto& v : hiss)
        {
            float low = 0.0f, high = 0.0f;
            split.process (v, low, high);
            v = high;
        }

        const int period = static_cast<int> (0.5 * sr), length = static_cast<int> (0.1 * sr);
        if (burstMask != nullptr)
            burstMask->assign (static_cast<size_t> (numSamples), false);

        for (int i = 0; i < numSamples; ++i)
        {
            if (i % period < length)
            {
                x[static_cast<size_t> (i)] += hiss[static_cast<size_t> (i)];
                if (burstMask != nullptr)
                    (*burstMask)[static_cast<size_t> (i)] = true;
            }
        }
        return x;
    }

    std::vector<float> highBand (const std::vector<float>& x)
    {
        Crossover split;
        split.prepare (4000.0, sr);
        std::vector<float> out (x.size());
        for (size_t i = 0; i < x.size(); ++i)
        {
            float low = 0.0f, high = 0.0f;
            split.process (x[i], low, high);
            out[i] = high;
        }
        return out;
    }
}

TEST_CASE ("Weiche: tief plus hoch lässt den Betrag unverändert", "[filter]")
{
    for (double f : { 500.0, 4000.0, 9000.0 })
    {
        Crossover split;
        split.prepare (4000.0, sr);
        auto x = test::sine (f, sr, 44100);
        const auto original = x;
        for (auto& v : x)
        {
            float low = 0.0f, high = 0.0f;
            split.process (v, low, high);
            v = low + high;
        }
        REQUIRE_THAT (test::rmsDb (x, 22050, 44100), WithinAbs (test::rmsDb (original, 22050, 44100), 0.05));
    }
}

TEST_CASE ("De-Esser senkt Zischlaute ab und lässt den Rest stehen", "[deesser]")
{
    constexpr int n = static_cast<int> (8 * sr);
    std::vector<bool> bursts;
    const auto input = sibilantTake (n, &bursts);

    auto render = [&] (bool enabled)
    {
        DeEsser d;
        d.prepare (sr, 1);
        d.setEnabled (enabled);
        d.setReductionDb (6.0f);
        d.setSensitivity (0.5f);
        d.reset();
        auto out = input;
        test::run (d, out);
        return std::make_pair (out, d.getLatencySamples());
    };

    const auto [off, latency] = render (false);
    const auto [on, latencyOn] = render (true);
    REQUIRE (latency == latencyOn);
    REQUIRE (latency == 1024 + 256);

    // Hochtonband in der Mitte der Zischlaute der letzten vier Sekunden
    // (vorher lernen die Schwellen), einmal an und einmal aus.
    const auto hiOff = highBand (off), hiOn = highBand (on);
    double sumOff = 0.0, sumOn = 0.0, lowOff = 0.0, lowOn = 0.0;
    const int period = static_cast<int> (0.5 * sr);
    for (int i = n / 2; i < n; ++i)
    {
        const int src = i - latency;
        const int phase = src % period;
        if (phase > static_cast<int> (0.02 * sr) && phase < static_cast<int> (0.08 * sr))
        {
            sumOff += hiOff[static_cast<size_t> (i)] * hiOff[static_cast<size_t> (i)];
            sumOn += hiOn[static_cast<size_t> (i)] * hiOn[static_cast<size_t> (i)];
        }
        else if (phase > static_cast<int> (0.25 * sr) && phase < static_cast<int> (0.45 * sr))
        {
            lowOff += off[static_cast<size_t> (i)] * off[static_cast<size_t> (i)];
            lowOn += on[static_cast<size_t> (i)] * on[static_cast<size_t> (i)];
        }
    }

    // Nicht die vollen 6 dB: Auch das Python-Vorbild erkennt nicht jeden
    // Rahmen eines Zischlauts. Auf genau diesem Signal senkt es um 3,9 dB
    // ab, diese Portierung um 4,0 dB (gemessen 2026-09-26).
    const double reduction = 10.0 * std::log10 (sumOff / sumOn);
    REQUIRE (reduction > 3.0);
    REQUIRE (reduction < 6.5);

    // Zwischen den Zischlauten: kein Eingriff.
    REQUIRE (std::abs (10.0 * std::log10 (lowOff / lowOn)) < 0.2);
}

TEST_CASE ("De-Esser: ein Ton ohne Zischlaute bleibt unverändert", "[deesser]")
{
    DeEsser d;
    d.prepare (sr, 1);
    d.reset();

    const auto input = test::sine (300.0, sr, 4 * 44100, 0.3f);
    auto out = input;
    test::run (d, out);

    REQUIRE_THAT (test::rmsDb (out, 88200, 4 * 44100), WithinAbs (test::rmsDb (input, 88200, 4 * 44100), 0.05));
    REQUIRE (d.getCurrentReductionDb() > -0.01f);
}

TEST_CASE ("Kompressor verringert den Abstand zwischen laut und leise", "[comp]")
{
    constexpr int n = static_cast<int> (12 * sr);
    const int half = static_cast<int> (0.5 * sr);

    auto input = test::sine (1000.0, sr, n, 1.0f);
    for (int i = 0; i < n; ++i)
        input[static_cast<size_t> (i)] *= (i / half) % 2 == 0 ? 0.5f : 0.05f;   // −6 / −26 dBFS

    AutoCompressor c;
    c.prepare (sr, 1);
    c.setAmount (1.0f);
    c.reset();

    auto out = input;
    test::run (c, out);

    // Letzter laut/leise-Wechsel, jeweils die zweite Hälfte (nach Attack/Release).
    const int lat = c.getLatencySamples();
    const int loudStart = 20 * half + half / 2 + lat, quietStart = 21 * half + half / 2 + lat;
    const float outSpan = test::rmsDb (out, loudStart, loudStart + half / 2) - test::rmsDb (out, quietStart, quietStart + half / 2);

    REQUIRE (outSpan < 17.0f);   // Eingang: 20 dB
    REQUIRE (c.getRatio() > 1.5f);
}

TEST_CASE ("Kompressor aus: Ausgang ist der verzögerte Eingang", "[comp]")
{
    AutoCompressor c;
    c.prepare (sr, 1);
    c.setEnabled (false);
    c.reset();

    const auto input = test::noise (44100, 0.8f);
    auto out = input;
    test::run (c, out);

    const int lat = c.getLatencySamples();
    REQUIRE (lat == 441);
    for (int i = lat; i < 44100; ++i)
        REQUIRE (out[static_cast<size_t> (i)] == input[static_cast<size_t> (i - lat)]);
}
