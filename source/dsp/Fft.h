#pragma once

#include <complex>
#include <vector>

namespace vocaleqq
{

/** Komplexe Radix-2-FFT, vorwärts und rückwärts.

    Selbst geschrieben wie in Keyy: Der DSP-Kern soll ohne Abhängigkeiten
    bauen. Die Größen sind klein und fest (2048 und 4096 Punkte bei 44,1 kHz),
    dafür ist diese schlichte Fassung schnell genug — auch im Audio-Thread,
    weil sie nach dem Konstruktor keinen Speicher mehr anfordert.
*/
class Fft
{
public:
    /** @param size  Zweierpotenz */
    explicit Fft (int size = 2);

    int getSize() const noexcept { return size; }

    /** Vorwärtstransformation an Ort und Stelle, ohne Normierung. */
    void forward (std::complex<float>* data) const noexcept;

    /** Rücktransformation an Ort und Stelle, mit 1/N normiert —
        forward und inverse hintereinander ergeben das Original.
    */
    void inverse (std::complex<float>* data) const noexcept;

private:
    void transform (std::complex<float>* data) const noexcept;

    int size;
    std::vector<std::complex<float>> twiddles;
    std::vector<int> bitReversed;
};

} // namespace vocaleqq
