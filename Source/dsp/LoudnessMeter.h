#pragma once

#include <cmath>
#include <array>
#include <limits>
#include "Biquad.h"

namespace fin
{
/** Lautheitsmessung nach ITU-R BS.1770-4 / EBU R128 (Stereo):
    Momentary (400 ms), Short-Term (3 s), Integrated (gated) und Loudness Range (LRA).
    Allokationsfrei (Histogramme mit 0.1-dB-Auflösung), echtzeitsicher. */
class LoudnessMeter
{
public:
    static constexpr double kSilence = -200.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // Stufe 1: High-Shelf (Kopf-Effekt)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.c = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                        2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        // Stufe 2: RLB-Hochpass
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs), a0 = 1.0 + K / Q + K * K;
            hp.c = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        stepLen = std::max (1, (int) std::lround (fs * 0.1));
        reset();
    }

    void reset()
    {
        shelf.reset(); hp.reset();
        stepPos = 0; stepAcc = 0.0;
        ring.fill (0.0); ringPos = 0; stepCount = 0;
        intH.clear(); lraH.clear();
        mom = st = integ = kSilence; lraVal = 0.0;
        maxMom = maxSt = kSilence;
    }

    /** Verarbeitet n Samples. Gibt die Anzahl abgeschlossener 100-ms-Schritte zurück. */
    int process (const float* L, const float* R, int n) noexcept
    {
        int completed = 0;
        for (int i = 0; i < n; ++i)
        {
            const double l = hp.processD (shelf.processD (L[i], 0), 0);
            const double r = hp.processD (shelf.processD (R[i], 1), 1);
            stepAcc += l * l + r * r;
            if (++stepPos >= stepLen)
            {
                finishStep();
                ++completed;
            }
        }
        return completed;
    }

    double momentary()  const { return mom; }
    double shortTerm()  const { return st; }
    double integrated() const { return integ; }
    double lra()        const { return lraVal; }
    double maxMomentary() const { return maxMom; }
    double maxShortTerm() const { return maxSt; }
    double seconds()    const { return stepCount * 0.1; }

    static double toLufs (double p) { return p > 0.0 ? -0.691 + 10.0 * std::log10 (p) : kSilence; }

private:
    struct Histogram
    {
        static constexpr double lo = -70.0, res = 0.1;
        static constexpr int bins = 760;                 // -70 ... +6 LUFS
        std::array<double, bins> pow {};
        std::array<double, bins> cnt {};
        double total = 0.0;

        void clear() { pow.fill (0.0); cnt.fill (0.0); total = 0.0; }
        static double centre (int b) { return lo + (b + 0.5) * res; }

        void add (double power)
        {
            const double l = toLufs (power);
            if (l < lo) return;                          // absolutes Gate -70 LUFS
            const int b = std::min (bins - 1, (int) ((l - lo) / res));
            pow[b] += power; cnt[b] += 1.0; total += 1.0;
        }

        double meanPower (double threshLufs, double& count) const
        {
            double p = 0.0; count = 0.0;
            for (int b = 0; b < bins; ++b)
                if (cnt[b] > 0.0 && centre (b) >= threshLufs) { p += pow[b]; count += cnt[b]; }
            return count > 0.0 ? p / count : 0.0;
        }
    };

    void finishStep() noexcept
    {
        const double stepPower = stepAcc / stepLen;
        stepAcc = 0.0; stepPos = 0;
        ring[(size_t) ringPos] = stepPower;
        ringPos = (ringPos + 1) % kRing;
        ++stepCount;

        auto meanLast = [this] (int k)
        {
            double s = 0.0;
            const int avail = (int) std::min<long long> (k, stepCount);
            for (int i = 1; i <= avail; ++i)
                s += ring[(size_t) ((ringPos - i + kRing) % kRing)];
            return s / k;   // fehlende Werte zählen als Stille
        };

        const double pM = meanLast (4), pS = meanLast (30);
        mom = toLufs (pM);
        st  = toLufs (pS);
        if (stepCount >= 4)  { intH.add (pM); maxMom = std::max (maxMom, mom); }
        if (stepCount >= 30) { lraH.add (pS); maxSt  = std::max (maxSt, st); }

        // Integrated: relatives Gate -10 LU
        if (intH.total > 0.0)
        {
            double c = 0.0;
            const double rel = toLufs (intH.meanPower (-1000.0, c)) - 10.0;
            integ = toLufs (intH.meanPower (rel, c));
        }

        // LRA: relatives Gate -20 LU, 10. bis 95. Perzentil der Short-Term-Werte
        if (lraH.total > 0.0)
        {
            double c = 0.0;
            const double rel = toLufs (lraH.meanPower (-1000.0, c)) - 20.0;
            double n = 0.0;
            for (int b = 0; b < Histogram::bins; ++b)
                if (Histogram::centre (b) >= rel) n += lraH.cnt[b];
            if (n > 0.0)
            {
                double cum = 0.0, p10 = 0.0, p95 = 0.0;
                bool has10 = false;
                for (int b = 0; b < Histogram::bins; ++b)
                {
                    if (Histogram::centre (b) < rel) continue;
                    cum += lraH.cnt[b];
                    if (! has10 && cum >= 0.10 * n) { p10 = Histogram::centre (b); has10 = true; }
                    if (cum >= 0.95 * n) { p95 = Histogram::centre (b); break; }
                }
                lraVal = std::max (0.0, p95 - p10);
            }
        }
    }

    static constexpr int kRing = 32;
    double fs = 48000.0;
    Biquad shelf, hp;
    int stepLen = 4800, stepPos = 0;
    double stepAcc = 0.0;
    std::array<double, kRing> ring {};
    int ringPos = 0;
    long long stepCount = 0;
    Histogram intH, lraH;
    double mom = kSilence, st = kSilence, integ = kSilence, lraVal = 0.0, maxMom = kSilence, maxSt = kSilence;
};
} // namespace fin
