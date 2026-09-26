#include "VocalChain.h"

#include <algorithm>
#include <cmath>

namespace vocaleqq
{

namespace
{
    constexpr float loudnessGateDb = -50.0f;
    constexpr float maxAutoGainDb = 12.0f;
}

void VocalChain::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    maxBlock = std::max (maxBlockSize, 1);
    activeChannels = std::clamp (numChannels, 1, maxChannels);

    deEsser.prepare (sampleRate, activeChannels);
    eq.prepare (sampleRate, activeChannels);
    compressor.prepare (sampleRate, activeChannels);

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        dryAllpass[static_cast<size_t> (ch)].prepare (4000.0, sampleRate);
        dryDelay[static_cast<size_t> (ch)].prepare (getLatencySamples());
        dryBuffer[static_cast<size_t> (ch)].assign (static_cast<size_t> (maxBlock), 0.0f);
    }

    gainBlockSize = std::max (static_cast<int> (sampleRate * 0.005), 16);
    const double blocksPerSecond = sampleRate / gainBlockSize;

    // Drei Sekunden: lang genug, dass der Abgleich nicht der Silbe folgt
    // (das wäre ein zweiter Kompressor), kurz genug, dass er nach einer
    // Reglerbewegung bald wieder stimmt.
    loudnessAlpha = smoothingCoefficient (3.0, blocksPerSecond);
    gainSmoothing = smoothingCoefficient (0.050, sampleRate);
    parameterSmoothing = smoothingCoefficient (0.020, sampleRate);

    reset();
}

void VocalChain::reset()
{
    deEsser.reset();
    eq.reset();
    compressor.reset();

    for (auto& a : dryAllpass) a.reset();
    for (auto& d : dryDelay)   d.reset();

    meanSquareIn = meanSquareOut = blockIn = blockOut = 0.0;
    blockCounter = 0;
    autoGain = 1.0f;
    mixSmoothed = settings.mix;
    bypassSmoothed = settings.bypass ? 1.0f : 0.0f;
}

void VocalChain::setSettings (const ChainSettings& s) noexcept
{
    settings = s;

    deEsser.setEnabled (s.deEssEnabled);
    deEsser.setReductionDb (s.deEssReductionDb);
    deEsser.setSensitivity (s.deEssSensitivity);

    eq.setEnabled (s.eqEnabled);
    eq.setStrength (s.eqStrength);
    eq.setLowCutDb (s.eqLowCutDb);

    compressor.setEnabled (s.compEnabled);
    compressor.setAmount (s.compAmount);
}

int VocalChain::getLatencySamples() const noexcept
{
    return deEsser.getLatencySamples() + eq.getLatencySamples() + compressor.getLatencySamples();
}

void VocalChain::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, activeChannels);
    if (numChannels <= 0)
        return;

    // Hosts dürfen größere Blöcke schicken als angekündigt; dann in Stücken.
    std::array<float*, maxChannels> chunk {};
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = std::min (maxBlock, numSamples - start);
        for (int ch = 0; ch < numChannels; ++ch)
            chunk[static_cast<size_t> (ch)] = channels[ch] + start;

        processChunk (chunk.data(), numChannels, n);
    }

    meterDeEss.store (deEsser.getCurrentReductionDb(), std::memory_order_relaxed);
    meterComp.store (compressor.getCurrentReductionDb(), std::memory_order_relaxed);
    meterAutoGain.store (gainToDb (autoGain), std::memory_order_relaxed);
    meterThreshold.store (compressor.getThresholdDb(), std::memory_order_relaxed);
    meterRatio.store (compressor.getRatio(), std::memory_order_relaxed);
}

void VocalChain::processChunk (float* const* channels, int numChannels, int numSamples) noexcept
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& dry = dryBuffer[static_cast<size_t> (ch)];
        auto& allpass = dryAllpass[static_cast<size_t> (ch)];
        auto& delay = dryDelay[static_cast<size_t> (ch)];

        for (int i = 0; i < numSamples; ++i)
        {
            float low = 0.0f, high = 0.0f;
            allpass.process (channels[ch][i], low, high);
            dry[static_cast<size_t> (i)] = delay.process (low + high);
        }
    }

    deEsser.process (channels, numChannels, numSamples);
    eq.process (channels, numChannels, numSamples);
    compressor.process (channels, numChannels, numSamples);

    const float channelScale = 1.0f / static_cast<float> (numChannels);
    const float mixTarget = std::clamp (settings.mix, 0.0f, 1.0f);
    const float bypassTarget = settings.bypass ? 1.0f : 0.0f;
    const float maxGain = dbToGain (maxAutoGainDb);

    for (int i = 0; i < numSamples; ++i)
    {
        mixSmoothed = parameterSmoothing * mixSmoothed + (1.0f - parameterSmoothing) * mixTarget;
        bypassSmoothed = parameterSmoothing * bypassSmoothed + (1.0f - parameterSmoothing) * bypassTarget;

        float dryMono = 0.0f, wetMono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = dryBuffer[static_cast<size_t> (ch)][static_cast<size_t> (i)];
            const float wet = mixSmoothed * channels[ch][i] + (1.0f - mixSmoothed) * dry;
            channels[ch][i] = wet;
            dryMono += dry;
            wetMono += wet;
        }

        dryMono *= channelScale;
        wetMono *= channelScale;
        blockIn += static_cast<double> (dryMono) * dryMono;
        blockOut += static_cast<double> (wetMono) * wetMono;

        if (++blockCounter >= gainBlockSize)
        {
            const double in = blockIn / gainBlockSize;
            const double out = blockOut / gainBlockSize;
            blockCounter = 0;
            blockIn = blockOut = 0.0;

            // Nur Blöcke mit Gesang: In Pausen ist das Verhältnis Rauschen
            // zu Rauschen und sagt über die Lautheit nichts.
            if (10.0 * std::log10 (in + 1.0e-12) > loudnessGateDb)
            {
                meanSquareIn = loudnessAlpha * meanSquareIn + (1.0 - loudnessAlpha) * in;
                meanSquareOut = loudnessAlpha * meanSquareOut + (1.0 - loudnessAlpha) * out;
            }
        }

        float target = 1.0f;
        if (settings.autoGain && meanSquareOut > 1.0e-12 && meanSquareIn > 1.0e-12)
            target = std::clamp (static_cast<float> (std::sqrt (meanSquareIn / meanSquareOut)), 1.0f / maxGain, maxGain);

        autoGain = gainSmoothing * autoGain + (1.0f - gainSmoothing) * target;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = dryBuffer[static_cast<size_t> (ch)][static_cast<size_t> (i)];
            channels[ch][i] = bypassSmoothed * dry + (1.0f - bypassSmoothed) * channels[ch][i] * autoGain;
        }
    }
}

} // namespace vocaleqq
