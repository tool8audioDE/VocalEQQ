#include "SpectrumView.h"

#include <cmath>

#include "Tool8LookAndFeel.h"

namespace vocaleqq
{

namespace
{
    constexpr float minHz = 20.0f, maxHz = 20000.0f;
    constexpr float floorDb = -90.0f, topDb = -6.0f;
    constexpr float maxShownCutDb = 12.0f;
}

SpectrumView::SpectrumView()
{
    setOpaque (false);
}

float SpectrumView::xForFrequency (float frequency, float width) const noexcept
{
    return std::log2 (frequency / minHz) / std::log2 (maxHz / minHz) * width;
}

void SpectrumView::clear()
{
    hasData = false;
    repaint();
}

void SpectrumView::update (const std::vector<float>& spectrumDb, const std::vector<float>& correctionDb,
                           double sampleRate, int fftSize)
{
    const int width = getWidth();
    if (width <= 0 || spectrumDb.empty() || spectrumDb.size() != correctionDb.size() || fftSize <= 0)
        return;

    if (width != lastWidth)
    {
        lastWidth = width;
        columnFrequencies.resize (static_cast<size_t> (width));
        for (int x = 0; x < width; ++x)
            columnFrequencies[static_cast<size_t> (x)] = minHz * std::pow (maxHz / minHz, static_cast<float> (x) / static_cast<float> (width - 1));
        spectrum.assign (static_cast<size_t> (width), floorDb);
        correction.assign (static_cast<size_t> (width), 0.0f);
    }

    // Ein Vollaussteuerungs-Sinus hat im Hann-Fenster den Betrag N/4 —
    // abgezogen steht das Spektrum ungefähr in dBFS.
    const float offset = 20.0f * std::log10 (static_cast<float> (fftSize) / 4.0f);
    const auto numBins = static_cast<int> (spectrumDb.size());
    const float binsPerHz = static_cast<float> (fftSize / sampleRate);

    for (int x = 0; x < width; ++x)
    {
        // Je Spalte der Bereich bis zur nächsten Spalte: in den Höhen viele
        // Bins (Maximum), in den Tiefen weniger als einer (nächster Bin).
        const float f0 = columnFrequencies[static_cast<size_t> (x)];
        const float f1 = x + 1 < width ? columnFrequencies[static_cast<size_t> (x + 1)] : f0 * 1.01f;
        const int b0 = juce::jlimit (0, numBins - 1, static_cast<int> (std::round (f0 * binsPerHz)));
        const int b1 = juce::jlimit (b0, numBins - 1, static_cast<int> (std::round (f1 * binsPerHz)));

        float s = -1000.0f, c = 0.0f;
        if (b1 > b0)
        {
            for (int b = b0; b <= b1; ++b)
            {
                s = std::max (s, spectrumDb[static_cast<size_t> (b)]);
                c = std::min (c, correctionDb[static_cast<size_t> (b)]);
            }
        }
        else
        {
            // Tiefton: mehrere Spalten je Bin. Linear zwischen den Bins,
            // sonst zeichnet sich jeder Bin als Treppenstufe.
            const float pos = juce::jlimit (0.0f, static_cast<float> (numBins - 1), f0 * binsPerHz);
            const int i0 = std::min (static_cast<int> (pos), numBins - 2);
            const float t = pos - static_cast<float> (i0);
            const auto u = static_cast<size_t> (i0);
            s = spectrumDb[u] + t * (spectrumDb[u + 1] - spectrumDb[u]);
            c = correctionDb[u] + t * (correctionDb[u + 1] - correctionDb[u]);
        }

        // Anzeige glätten: schnell hoch, langsam runter — ruhig, aber
        // Einsätze sind sofort zu sehen.
        auto& sv = spectrum[static_cast<size_t> (x)];
        const float target = juce::jlimit (floorDb, topDb + 12.0f, s - offset);
        sv = target > sv ? 0.5f * sv + 0.5f * target : 0.85f * sv + 0.15f * target;

        auto& cv = correction[static_cast<size_t> (x)];
        cv = 0.6f * cv + 0.4f * c;
    }

    hasData = true;
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    using namespace Tool8Colours;

    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth(), h = bounds.getHeight();

    g.setColour (surface);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    // Frequenz-Raster
    g.setFont (Tool8Fonts::tiny());
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFrequency (f, w);
        g.setColour (surfaceHi);
        g.drawVerticalLine (juce::roundToInt (x), 1.0f, h - 1.0f);

        g.setColour (muted);
        const auto label = f >= 1000.0f ? juce::String (static_cast<int> (f / 1000.0f)) + "k" : juce::String (static_cast<int> (f));
        g.drawText (label, juce::Rectangle<float> (x + 4.0f, h - 16.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
    }

    g.setColour (muted);
    g.drawText ("Spectrum + EQ correction", juce::Rectangle<float> (10.0f, 6.0f, 200.0f, 12.0f),
                juce::Justification::centredLeft);

    if (! hasData || spectrum.size() != static_cast<size_t> (getWidth()))
    {
        g.setFont (Tool8Fonts::label());
        g.drawText (juce::String::fromUTF8 ("Play audio — the spectrum appears here."), bounds,
                    juce::Justification::centred);
        return;
    }

    // Spektrum in den oberen zwei Dritteln, gefüllt wie in der Python-Fassung.
    const float top = h * 0.12f, span = h * 0.56f;
    auto yFor = [&] (float db) { return top + (1.0f - (db - floorDb) / (topDb - floorDb)) * span; };

    juce::Path line, area;
    for (int x = 0; x < getWidth(); ++x)
    {
        const float y = juce::jlimit (top, top + span, yFor (spectrum[static_cast<size_t> (x)]));
        if (x == 0)
            line.startNewSubPath (0.0f, y);
        else
            line.lineTo (static_cast<float> (x), y);
    }

    area = line;
    area.lineTo (w, top + span);
    area.lineTo (0.0f, top + span);
    area.closeSubPath();

    g.setColour (surfaceHi);
    g.fillPath (area);
    g.setColour (muted);
    g.strokePath (line, juce::PathStrokeType (1.0f));

    // Korrektur: Absenkungen als pinke Fläche unter einer Nulllinie.
    const float zero = h * 0.74f, depthScale = h * 0.22f / maxShownCutDb;
    g.setColour (border);
    g.drawHorizontalLine (juce::roundToInt (zero), 0.0f, w);

    juce::Path cutPath;
    cutPath.startNewSubPath (0.0f, zero);
    for (int x = 0; x < getWidth(); ++x)
    {
        const float depth = std::min (-correction[static_cast<size_t> (x)], maxShownCutDb);
        cutPath.lineTo (static_cast<float> (x), zero + std::max (depth, 0.0f) * depthScale);
    }
    cutPath.lineTo (w, zero);
    cutPath.closeSubPath();

    g.setColour (cut.withAlpha (0.35f));
    g.fillPath (cutPath);
    g.setColour (cut);
    g.strokePath (cutPath, juce::PathStrokeType (1.2f));
}

} // namespace vocaleqq
