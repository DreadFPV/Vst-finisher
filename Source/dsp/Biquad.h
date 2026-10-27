#pragma once

#include <cmath>
#include <complex>
#include <algorithm>

namespace fin
{
constexpr double kPi = 3.14159265358979323846;

/** RBJ-Cookbook-Koeffizienten (normalisiert auf a0 = 1). */
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    static double clampFreq (double fs, double f) { return std::clamp (f, 1.0, fs * 0.49); }

    static BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    static BiquadCoeffs lowpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    static BiquadCoeffs highpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    static BiquadCoeffs peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    static BiquadCoeffs lowShelf (double fs, double f, double gainDb, double q = 0.7071)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = 2.0 * kPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (A * ((A + 1) - (A - 1) * c + 2 * sA * al),
                          2 * A * ((A - 1) - (A + 1) * c),
                          A * ((A + 1) - (A - 1) * c - 2 * sA * al),
                          (A + 1) + (A - 1) * c + 2 * sA * al,
                          -2 * ((A - 1) + (A + 1) * c),
                          (A + 1) + (A - 1) * c - 2 * sA * al);
    }

    static BiquadCoeffs highShelf (double fs, double f, double gainDb, double q = 0.7071)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = 2.0 * kPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (A * ((A + 1) + (A - 1) * c + 2 * sA * al),
                          -2 * A * ((A - 1) + (A + 1) * c),
                          A * ((A + 1) + (A - 1) * c - 2 * sA * al),
                          (A + 1) - (A - 1) * c + 2 * sA * al,
                          2 * ((A - 1) - (A + 1) * c),
                          (A + 1) - (A - 1) * c - 2 * sA * al);
    }

    /** Betrag in dB bei Frequenz f (für die EQ-Kurve in der GUI). */
    double magnitudeDb (double f, double fs) const
    {
        const double w = 2.0 * kPi * f / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto h = (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
        return 20.0 * std::log10 (std::max (std::abs (h), 1e-12));
    }

    bool operator== (const BiquadCoeffs& o) const
    {
        return b0 == o.b0 && b1 == o.b1 && b2 == o.b2 && a1 == o.a1 && a2 == o.a2;
    }
};

/** Stereo-Biquad, Transposed Direct Form II, double-Präzision intern. */
struct Biquad
{
    BiquadCoeffs c;
    double z1[2] {}, z2[2] {};

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }

    inline float process (float xf, int ch) noexcept
    {
        const double x = xf;
        const double y = c.b0 * x + z1[ch];
        z1[ch] = c.b1 * x - c.a1 * y + z2[ch];
        z2[ch] = c.b2 * x - c.a2 * y;
        return (float) y;
    }

    inline double processD (double x, int ch) noexcept
    {
        const double y = c.b0 * x + z1[ch];
        z1[ch] = c.b1 * x - c.a1 * y + z2[ch];
        z2[ch] = c.b2 * x - c.a2 * y;
        return y;
    }
};
} // namespace fin
