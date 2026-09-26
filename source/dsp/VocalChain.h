#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "AutoCompressor.h"
#include "DeEsser.h"
#include "Filters.h"
#include "ResonanceEq.h"

namespace vocaleqq
{

/** Alle Einstellungen der Kette. Die Vorgaben sind die der Python-Oberfläche. */
struct ChainSettings
{
    bool  deEssEnabled = true;
    float deEssReductionDb = 4.5f;   ///< 0..12
    float deEssSensitivity = 0.5f;   ///< 0..1

    bool  eqEnabled = true;
    float eqStrength = 0.7f;         ///< 0..1
    float eqLowCutDb = 3.0f;         ///< 0..8, als positive Zahl

    bool  compEnabled = true;
    float compAmount = 0.7f;         ///< 0..1

    float mix = 1.0f;                ///< 0..1
    bool  autoGain = true;
    bool  bypass = false;
};

/** Der ganze Channel-Strip: De-Ess → EQ → Kompressor, wie run_chain in
    vocaleq/chain.py.

    Danach wie im Vorbild: Mix mit dem trockenen Signal und Abgleich auf die
    Lautheit des Eingangs — damit man beim Vergleich die Bearbeitung hört
    und nicht bloß „lauter“.

    GLEICHE LATENZ IN JEDER STELLUNG
    Die Latenz ist die Summe der drei Stufen und hängt nie an einem
    Schalter. Eine ausgeschaltete Stufe rechnet weiter und lässt nur ihre
    Absenkung ausklingen. So meldet das Plugin der DAW eine feste Zahl, und
    kein Umschalten knackt.

    DER TROCKENE ZWEIG
    ... läuft durch dieselbe Verzögerung und durch denselben Allpass wie die
    Weiche des De-Essers. Sonst lägen nass und trocken um 4 kHz in der Phase
    auseinander, und jeder Mix unter 100 % kämmte.
*/
class VocalChain
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setSettings (const ChainSettings& s) noexcept;

    int getLatencySamples() const noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    ResonanceEq& getEq() noexcept { return eq; }

    // Anzeigewerte, aus jedem Thread lesbar.
    float getDeEssReductionDb() const noexcept { return meterDeEss.load (std::memory_order_relaxed); }
    float getCompReductionDb() const noexcept  { return meterComp.load (std::memory_order_relaxed); }
    float getAutoGainDb() const noexcept       { return meterAutoGain.load (std::memory_order_relaxed); }
    float getCompThresholdDb() const noexcept  { return meterThreshold.load (std::memory_order_relaxed); }
    float getCompRatio() const noexcept        { return meterRatio.load (std::memory_order_relaxed); }

private:
    void processChunk (float* const* channels, int numChannels, int numSamples) noexcept;

    double sampleRate = 44100.0;
    int maxBlock = 512;
    int activeChannels = 2;
    ChainSettings settings;

    DeEsser deEsser;
    ResonanceEq eq;
    AutoCompressor compressor;

    static constexpr int maxChannels = 2;
    std::array<Crossover, maxChannels> dryAllpass;
    std::array<DelayLine, maxChannels> dryDelay;
    std::array<std::vector<float>, maxChannels> dryBuffer;

    // Lautheitsabgleich: langsame Mittel der Leistung von Eingang und
    // Ausgang, nur über Blöcke, in denen gesungen wird.
    double meanSquareIn = 0.0, meanSquareOut = 0.0;
    double blockIn = 0.0, blockOut = 0.0;
    int blockCounter = 0, gainBlockSize = 256;
    float loudnessAlpha = 0.0f, gainSmoothing = 0.0f;
    float autoGain = 1.0f;

    float mixSmoothed = 1.0f, bypassSmoothed = 0.0f, parameterSmoothing = 0.0f;

    std::atomic<float> meterDeEss { 0.0f }, meterComp { 0.0f }, meterAutoGain { 0.0f };
    std::atomic<float> meterThreshold { 0.0f }, meterRatio { 1.0f };
};

} // namespace vocaleqq
