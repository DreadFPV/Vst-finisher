#pragma once

#include <cmath>
#include <algorithm>
#include "Biquad.h"

namespace fin
{
/** 4x-Oversampling-True-Peak-Schätzer (Polyphasen-FIR, Kaiser-gefenstertes Sinc), Stereo.
    Gibt pro Eingangssample den höchsten Betrag der 4 interpolierten Werte (beide Kanäle) zurück.
    Verzögerung: kDelay Samples. */
class TruePeakDetector
{
public:
    static constexpr int kPhases = 4;
    static constexpr int kTaps   = 16;   // pro Phase -> 64 Taps gesamt
    static constexpr int kDelay  = 8;    // ~ (64-1)/2/4 Samples

    TruePeakDetector() { design(); reset(); }

    void reset()
    {
        for (auto& ch : hist)
            std::fill (std::begin (ch), std::end (ch), 0.0f);
        pos = 0;
    }

    inline float process (float l, float r) noexcept
    {
        pos = (pos == 0) ? kTaps - 1 : pos - 1;
        hist[0][pos] = l;
        hist[1][pos] = r;

        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int p = 0; p < kPhases; ++p)
            {
                double acc = 0.0;
                int idx = pos;
                for (int j = 0; j < kTaps; ++j)
                {
                    acc += coef[p][j] * hist[ch][idx];
                    if (++idx == kTaps) idx = 0;
                }
                peak = std::max (peak, (float) std::abs (acc));
            }
        }
        return peak;
    }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-12 * sum) break;
        }
        return sum;
    }

    void design()
    {
        constexpr int N = kPhases * kTaps;
        constexpr double beta = 7.0;
        constexpr double fc = 0.5 / kPhases;             // Grenzfrequenz = Nyquist der Basisrate
        const double i0b = besselI0 (beta);
        double h[N];
        for (int i = 0; i < N; ++i)
        {
            const double t = i - (N - 1) * 0.5;
            const double x = 2.0 * fc * t;
            const double sinc = (std::abs (x) < 1e-12) ? 1.0 : std::sin (kPi * x) / (kPi * x);
            const double r = 2.0 * i / (N - 1) - 1.0;
            const double win = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            h[i] = sinc * win;
        }
        for (int p = 0; p < kPhases; ++p)
        {
            double sum = 0.0;
            for (int j = 0; j < kTaps; ++j) sum += h[p + kPhases * j];
            for (int j = 0; j < kTaps; ++j) coef[p][j] = h[p + kPhases * j] / sum; // DC-Gain je Phase = 1
        }
    }

    double coef[kPhases][kTaps] {};
    float hist[2][kTaps] {};
    int pos = 0;
};
} // namespace fin
