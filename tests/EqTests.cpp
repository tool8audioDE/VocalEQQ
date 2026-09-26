#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>

#include "TestSignals.h"
#include "dsp/Fft.h"
#include "dsp/ResonanceEq.h"

using namespace vocaleqq;
using Catch::Matchers::WithinAbs;

TEST_CASE ("FFT: vorwärts und rückwärts ergibt das Original", "[fft]")
{
    Fft fft (1024);
    std::vector<std::complex<float>> data (1024);
    const auto x = test::noise (1024, 1.0f);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = { x[i], 0.5f * x[(i + 7) % x.size()] };

    auto original = data;
    fft.forward (data.data());
    fft.inverse (data.data());

    for (size_t i = 0; i < data.size(); ++i)
        REQUIRE (std::abs (data[i] - original[i]) < 1.0e-5f);
}

TEST_CASE ("EQ aus: Ausgang ist der um die Latenz verzögerte Eingang", "[eq]")
{
    constexpr double sr = 44100.0;
    ResonanceEq eq;
    eq.prepare (sr, 1);
    eq.setEnabled (false);
    eq.reset();

    const auto input = test::noise (44100, 0.5f);
    auto output = input;
    test::run (eq, output);

    const int latency = eq.getLatencySamples();
    REQUIRE (latency == 4096);

    float maxError = 0.0f;
    for (int i = latency; i < static_cast<int> (output.size()); ++i)
        maxError = std::max (maxError, std::abs (output[static_cast<size_t> (i)] - input[static_cast<size_t> (i - latency)]));

    REQUIRE (maxError < 1.0e-4f);
}

TEST_CASE ("EQ: Hochpass und Low-Shelf wie im Vorbild", "[eq]")
{
    constexpr double sr = 44100.0;
    constexpr int n = 88200;

    auto levelChange = [&] (double frequency, float lowCutDb)
    {
        ResonanceEq eq;
        eq.prepare (sr, 1);
        eq.setStrength (0.0f);        // nur die feste Tonbalance
        eq.setLowCutDb (lowCutDb);
        eq.reset();

        const auto input = test::sine (frequency, sr, n);
        auto output = input;
        test::run (eq, output);

        const int lat = eq.getLatencySamples();
        return test::toneDb (output, n / 2, n, frequency, sr) - test::toneDb (input, n / 2 - lat, n - lat, frequency, sr);
    };

    // Eine Oktave unter 100 Hz: −12 dB Hochpass, Shelf aus.
    REQUIRE_THAT (levelChange (50.0, 0.0f), WithinAbs (-12.0, 0.5));
    // Mitten bleiben unberührt.
    REQUIRE_THAT (levelChange (1000.0, 3.0f), WithinAbs (0.0, 0.1));
    // 150 Hz beim Shelf −3 dB: −3 · (1 − sigmoid(−4 · log2(150/250) / 1,5)) = −2,63 dB.
    REQUIRE_THAT (levelChange (150.0, 3.0f), WithinAbs (-2.63, 0.3));
}

TEST_CASE ("EQ: eine Resonanz wird abgesenkt, das Umfeld nicht", "[eq]")
{
    // Eine Resonanz ist eine breitere Überhöhung, kein einzelner Ton: Die
    // Erkennung mittelt in dB über eine Sechstel-Oktave, eine schmale
    // Spektrallinie fällt dabei kaum ins Gewicht (genau wie im Vorbild).
    // Hier: Rauschen, dazu dasselbe Rauschen um 3 kHz schmalbandig (Güte 15)
    // gefiltert und kräftig angehoben. Das Python-Vorbild senkt so ein
    // Signal um 4,4 dB bei 3 kHz ab, das Umfeld bei 1 kHz um 1,8 dB (die
    // riesige Spitze zieht die Oktav-Basis dort mit hoch). Eine breite,
    // mäßige Überhöhung (Güte 6, sechsfach) senkt es nur um 0,9 dB ab —
    // die Portierung ebenso (gemessen 2026-09-26).
    constexpr double sr = 44100.0;
    constexpr int n = 88200;

    auto input = test::noise (n, 0.05f, 3);
    const auto band = test::bandpass (input, 3000.0, 15.0, sr);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] += 20.0f * band[i];

    auto render = [&] (float strength)
    {
        ResonanceEq eq;
        eq.prepare (sr, 1);
        eq.setStrength (strength);
        eq.setLowCutDb (0.0f);
        eq.reset();
        auto output = input;
        test::run (eq, output);
        return output;
    };

    const auto neutral = render (0.0f);
    const auto full = render (1.0f);

    auto bandLevel = [&] (const std::vector<float>& x, double frequency)
    {
        const auto b = test::bandpass (x, frequency, 6.0, sr);
        return test::rmsDb (b, n / 2, n);
    };

    const float reduction = bandLevel (neutral, 3000.0) - bandLevel (full, 3000.0);
    REQUIRE (reduction > 3.0f);
    REQUIRE (reduction < 6.5f);

    // Das Umfeld wird deutlich weniger angefasst als die Resonanz.
    const float around = bandLevel (neutral, 1000.0) - bandLevel (full, 1000.0);
    REQUIRE (reduction - around > 2.0f);
}

TEST_CASE ("EQ: Stereo wird gemeinsam erkannt und getrennt ausgegeben", "[eq]")
{
    constexpr double sr = 44100.0;
    constexpr int n = 44100;

    ResonanceEq eq;
    eq.prepare (sr, 2);
    eq.setEnabled (false);
    eq.reset();

    const auto left = test::sine (440.0, sr, n);
    const auto right = test::noise (n, 0.3f);
    auto l = left, r = right;

    for (int start = 0; start < n; start += 512)
    {
        float* ptrs[] { l.data() + start, r.data() + start };
        eq.process (ptrs, 2, std::min (512, n - start));
    }

    const int lat = eq.getLatencySamples();
    float maxError = 0.0f;
    for (int i = lat; i < n; ++i)
    {
        maxError = std::max (maxError, std::abs (l[static_cast<size_t> (i)] - left[static_cast<size_t> (i - lat)]));
        maxError = std::max (maxError, std::abs (r[static_cast<size_t> (i)] - right[static_cast<size_t> (i - lat)]));
    }
    REQUIRE (maxError < 1.0e-4f);
}
