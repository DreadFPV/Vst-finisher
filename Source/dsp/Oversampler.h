#pragma once

#include <vector>
#include <cmath>
#include "Biquad.h"

namespace fin
{
/** 2x-Stufe (Up + Down) mit linearphasigem FIR (ungerade Länge, Kaiser-Fenster).
    Latenz Up+Down zusammen: (N-1) Samples der höheren Rate. */
class Os2xStage
{
public:
    void design (int numTaps, double cutoff /* rel. zur hohen Rate, 0..0.5 */, double beta)
    {
        N = numTaps | 1;
        h.assign ((size_t) N, 0.0);
        auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 50; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-12 * s) break; } return s; };
        const double ib = i0 (beta);
        double sum = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double t = i - (N - 1) * 0.5;
            const double x = 2.0 * cutoff * t;
            const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPi * x) / (kPi * x);
            const double r = 2.0 * i / (N - 1) - 1.0;
            h[(size_t) i] = 2.0 * cutoff * sinc * i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / ib;
            sum += h[(size_t) i];
        }
        for (auto& v : h) v /= sum;     // DC-Gain 1
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            upHist[c].assign ((size_t) N, 0.0);
            dnHist[c].assign ((size_t) N, 0.0);
        }
        upPos = dnPos = 0;
    }

    /** n Eingangs-Samples -> 2n Ausgangs-Samples. */
    void up (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        const int half = (N + 1) / 2;
        for (int i = 0; i < n; ++i)
        {
            upPos = (upPos == 0) ? half - 1 : upPos - 1;
            upHist[0][(size_t) upPos] = inL[i];
            upHist[1][(size_t) upPos] = inR[i];
            for (int p = 0; p < 2; ++p)
            {
                double aL = 0.0, aR = 0.0;
                int idx = upPos;
                for (int j = 0; p + 2 * j < N; ++j)
                {
                    const double c = h[(size_t) (p + 2 * j)];
                    aL += c * upHist[0][(size_t) idx];
                    aR += c * upHist[1][(size_t) idx];
                    if (++idx == half) idx = 0;
                }
                outL[2 * i + p] = (float) (2.0 * aL);
                outR[2 * i + p] = (float) (2.0 * aR);
            }
        }
    }

    /** 2n Eingangs-Samples -> n Ausgangs-Samples. */
    void down (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            // gerade Phase einfügen, dann filtern (ganzzahlige Latenz), dann ungerade Phase nachschieben
            dnPos = (dnPos == 0) ? N - 1 : dnPos - 1;
            dnHist[0][(size_t) dnPos] = inL[2 * i];
            dnHist[1][(size_t) dnPos] = inR[2 * i];
            double aL = 0.0, aR = 0.0;
            int idx = dnPos;
            for (int k = 0; k < N; ++k)
            {
                aL += h[(size_t) k] * dnHist[0][(size_t) idx];
                aR += h[(size_t) k] * dnHist[1][(size_t) idx];
                if (++idx == N) idx = 0;
            }
            outL[i] = (float) aL;
            outR[i] = (float) aR;
            dnPos = (dnPos == 0) ? N - 1 : dnPos - 1;
            dnHist[0][(size_t) dnPos] = inL[2 * i + 1];
            dnHist[1][(size_t) dnPos] = inR[2 * i + 1];
        }
    }

    int taps() const { return N; }

private:
    int N = 33;
    std::vector<double> h;
    std::vector<double> upHist[2], dnHist[2];
    int upPos = 0, dnPos = 0;
};

/** 4x-Oversampling (2 Stufen). Latenz: kLatency Basis-Samples (ganzzahlig, unabhängig von der Samplerate). */
class Oversampler4x
{
public:
    static constexpr int kTaps1 = 95;   // Stufe 1 (1x->2x): (95-1)/2 = 47 Basis-Samples
    static constexpr int kTaps2 = 33;   // Stufe 2 (2x->4x): (33-1)/4 = 8 Basis-Samples
    static constexpr int kLatency = (kTaps1 - 1) / 2 + (kTaps2 - 1) / 4;

    void prepare (int maxBlock)
    {
        s1.design (kTaps1, 0.235, 9.0);
        s2.design (kTaps2, 0.18, 7.0);
        for (int c = 0; c < 2; ++c)
        {
            b2[c].assign ((size_t) maxBlock * 2, 0.0f);
            b4[c].assign ((size_t) maxBlock * 4, 0.0f);
        }
    }

    void reset() { s1.reset(); s2.reset(); }

    /** Gibt Zeiger auf 4n Samples zurück. */
    void up (const float* L, const float* R, int n)
    {
        s1.up (L, R, b2[0].data(), b2[1].data(), n);
        s2.up (b2[0].data(), b2[1].data(), b4[0].data(), b4[1].data(), 2 * n);
    }

    void down (float* L, float* R, int n)
    {
        s2.down (b4[0].data(), b4[1].data(), b2[0].data(), b2[1].data(), 2 * n);
        s1.down (b2[0].data(), b2[1].data(), L, R, n);
    }

    float* hiL() { return b4[0].data(); }
    float* hiR() { return b4[1].data(); }

private:
    Os2xStage s1, s2;
    std::vector<float> b2[2], b4[2];
};

/** Einfache Stereo-Verzögerung. */
class DelayLine
{
public:
    void prepare (int samples) { len = samples; buf[0].assign ((size_t) len + 1, 0.0f); buf[1].assign ((size_t) len + 1, 0.0f); pos = 0; }
    int length() const { return len; }
    void reset() { std::fill (buf[0].begin(), buf[0].end(), 0.0f); std::fill (buf[1].begin(), buf[1].end(), 0.0f); pos = 0; }

    inline void process (float& l, float& r) noexcept
    {
        if (len == 0) return;
        const float ol = buf[0][(size_t) pos], or_ = buf[1][(size_t) pos];
        buf[0][(size_t) pos] = l; buf[1][(size_t) pos] = r;
        if (++pos >= len) pos = 0;      // Zyklus = len -> Verzögerung exakt len
        l = ol; r = or_;
    }

    void process (float* L, float* R, int n) noexcept { for (int i = 0; i < n; ++i) process (L[i], R[i]); }

private:
    int len = 0, pos = 0;
    std::vector<float> buf[2];
};
} // namespace fin
