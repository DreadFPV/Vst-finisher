#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include "Biquad.h"
#include "Settings.h"
#include "TruePeakDetector.h"

namespace fin
{
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g)  { return g > 1e-10f ? 20.0f * std::log10 (g) : -200.0f; }

//==============================================================================
/** 6-Band-Mastering-EQ: Low-Cut (24 dB/Okt), Low-Shelf, 2x Glocke, High-Shelf. */
class Equalizer
{
public:
    void prepare (double sampleRate) { fs = sampleRate; for (auto& b : bands) b.reset(); dirty = true; }
    void reset() { for (auto& b : bands) b.reset(); }

    void update (const Settings& s)
    {
        if (! dirty && same (s)) return;
        last = s; dirty = false;
        bands[0].c = BiquadCoeffs::highpass (fs, s.hpFreq, 0.54119610);
        bands[1].c = BiquadCoeffs::highpass (fs, s.hpFreq, 1.3065630);
        bands[2].c = BiquadCoeffs::lowShelf (fs, s.lsFreq, s.lsGain);
        bands[3].c = BiquadCoeffs::peak (fs, s.p1Freq, s.p1Q, s.p1Gain);
        bands[4].c = BiquadCoeffs::peak (fs, s.p2Freq, s.p2Q, s.p2Gain);
        bands[5].c = BiquadCoeffs::highShelf (fs, s.hsFreq, s.hsGain);
        hpOn = s.hpOn;
    }

    void process (float* L, float* R, int n) noexcept
    {
        for (int b = hpOn ? 0 : 2; b < 6; ++b)
        {
            if (b >= 2 && isFlat (b)) continue;
            auto& bq = bands[(size_t) b];
            for (int i = 0; i < n; ++i) { L[i] = bq.process (L[i], 0); R[i] = bq.process (R[i], 1); }
        }
    }

    /** Gesamtfrequenzgang (für die GUI). */
    static double responseDb (const Settings& s, double f, double sampleRate)
    {
        double db = 0.0;
        if (s.hpOn)
            db += BiquadCoeffs::highpass (sampleRate, s.hpFreq, 0.54119610).magnitudeDb (f, sampleRate)
                + BiquadCoeffs::highpass (sampleRate, s.hpFreq, 1.3065630).magnitudeDb (f, sampleRate);
        db += BiquadCoeffs::lowShelf (sampleRate, s.lsFreq, s.lsGain).magnitudeDb (f, sampleRate);
        db += BiquadCoeffs::peak (sampleRate, s.p1Freq, s.p1Q, s.p1Gain).magnitudeDb (f, sampleRate);
        db += BiquadCoeffs::peak (sampleRate, s.p2Freq, s.p2Q, s.p2Gain).magnitudeDb (f, sampleRate);
        db += BiquadCoeffs::highShelf (sampleRate, s.hsFreq, s.hsGain).magnitudeDb (f, sampleRate);
        return db;
    }

private:
    bool isFlat (int b) const
    {
        switch (b)
        {
            case 2: return last.lsGain == 0.0f;
            case 3: return last.p1Gain == 0.0f;
            case 4: return last.p2Gain == 0.0f;
            case 5: return last.hsGain == 0.0f;
            default: return false;
        }
    }

    bool same (const Settings& s) const
    {
        return s.hpOn == last.hpOn && s.hpFreq == last.hpFreq && s.lsFreq == last.lsFreq && s.lsGain == last.lsGain
            && s.p1Freq == last.p1Freq && s.p1Gain == last.p1Gain && s.p1Q == last.p1Q
            && s.p2Freq == last.p2Freq && s.p2Gain == last.p2Gain && s.p2Q == last.p2Q
            && s.hsFreq == last.hsFreq && s.hsGain == last.hsGain;
    }

    double fs = 48000.0;
    Biquad bands[6];
    Settings last;
    bool dirty = true, hpOn = true;
};

//==============================================================================
/** Bass-Mono unterhalb einer Frequenz (Linkwitz-Riley 4. Ordnung) + Stereobreite darüber. */
class StereoTool
{
public:
    void prepare (double sampleRate) { fs = sampleRate; freq = -1.0f; reset(); }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }

    void process (float* L, float* R, int n, bool monoOn, float monoFreq, float widthPct) noexcept
    {
        const float w = widthPct * 0.01f;
        if (! monoOn)
        {
            if (std::abs (w - 1.0f) < 1e-4f) return;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * w;
                L[i] = m + s; R[i] = m - s;
            }
            return;
        }

        if (monoFreq != freq)
        {
            freq = monoFreq;
            lp1.c = lp2.c = BiquadCoeffs::lowpass (fs, freq, 0.70710678);
            hp1.c = hp2.c = BiquadCoeffs::highpass (fs, freq, 0.70710678);
        }

        for (int i = 0; i < n; ++i)
        {
            const float lowL = lp2.process (lp1.process (L[i], 0), 0);
            const float lowR = lp2.process (lp1.process (R[i], 1), 1);
            const float hiL  = hp2.process (hp1.process (L[i], 0), 0);
            const float hiR  = hp2.process (hp1.process (R[i], 1), 1);
            const float lowM = 0.5f * (lowL + lowR);
            const float m = 0.5f * (hiL + hiR), s = 0.5f * (hiL - hiR) * w;
            L[i] = lowM + m + s;
            R[i] = lowM + m - s;
        }
    }

private:
    double fs = 48000.0;
    float freq = -1.0f;
    Biquad lp1, lp2, hp1, hp2;
};

//==============================================================================
/** Stereo-gelinkter Feed-Forward-Kompressor mit Soft-Knee und Parallel-Mix. */
class Compressor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { grSmooth = 0.0f; }

    /** Gibt die maximale Gain-Reduction des Blocks in dB (<= 0) zurück. */
    float process (float* L, float* R, int n, const Settings& s) noexcept
    {
        const float atk = std::exp (-1.0f / (std::max (0.05f, s.attack) * 0.001f * (float) fs));
        const float rel = std::exp (-1.0f / (std::max (1.0f, s.release) * 0.001f * (float) fs));
        const float T = s.thresh, Rt = std::max (1.0f, s.ratio), W = std::max (0.0f, s.knee);
        const float makeup = dbToGain (s.makeup), mix = std::clamp (s.mix * 0.01f, 0.0f, 1.0f);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            const float xDb = gainToDb (x);
            float yDb;
            const float d = xDb - T;
            if (2.0f * d < -W)                 yDb = xDb;
            else if (2.0f * std::abs (d) <= W) yDb = xDb + (1.0f / Rt - 1.0f) * (d + W * 0.5f) * (d + W * 0.5f) / (2.0f * W);
            else                               yDb = T + d / Rt;
            const float gr = yDb - xDb;        // <= 0

            grSmooth = (gr < grSmooth) ? atk * grSmooth + (1.0f - atk) * gr
                                       : rel * grSmooth + (1.0f - rel) * gr;
            maxGr = std::min (maxGr, grSmooth);

            const float g = dbToGain (grSmooth) * makeup;
            L[i] = L[i] * (1.0f - mix) + L[i] * g * mix;
            R[i] = R[i] * (1.0f - mix) + R[i] * g * mix;
        }
        return maxGr;
    }

private:
    double fs = 48000.0;
    float grSmooth = 0.0f;
};

//==============================================================================
/** Lookahead-Limiter mit True-Peak-Erkennung (4x oversampled).
    Gain-Berechnung: Sliding-Minimum über das Lookahead-Fenster -> Release -> Box-Filter.
    Damit wird das Ceiling garantiert erreicht, bevor der Peak durchläuft. */
class TruePeakLimiter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        look   = std::max (4, (int) std::lround (0.0015 * fs));         // 1.5 ms Lookahead
        window = look + 4;                                                // Reserve für Detektor-Jitter
        delay  = (look - 1) + TruePeakDetector::kDelay + 2;
        delayL.assign ((size_t) delay + 1, 0.0f);
        delayR.assign ((size_t) delay + 1, 0.0f);
        box.assign ((size_t) look, 1.0);
        dqIdx.assign ((size_t) window + 2, 0);
        dqVal.assign ((size_t) window + 2, 1.0f);
        reset();
    }

    void reset()
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0);
        boxSum = (double) look; boxPos = 0; dPos = 0;
        dqHead = dqTail = 0; t = 0; env = 1.0f;
        det.reset();
    }

    int latency() const { return delay; }

    /** Gibt den minimalen Gain des Blocks in dB (<= 0) zurück. */
    float process (float* L, float* R, int n, bool enabled, float ceilingDb, float releaseMs) noexcept
    {
        const float ceil = dbToGain (ceilingDb - 0.1f);   // 0.1 dB Reserve für Messabweichungen
        const float relCoef = std::exp (-1.0f / (std::max (1.0f, releaseMs) * 0.001f * (float) fs));
        const int cap = (int) dqIdx.size();
        float minGain = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            const float tp = det.process (L[i], R[i]);
            const float r = (tp > ceil) ? ceil / tp : 1.0f;

            // Sliding-Minimum (monotone Deque)
            while (dqHead != dqTail && dqVal[(size_t) ((dqTail - 1 + cap) % cap)] >= r)
                dqTail = (dqTail - 1 + cap) % cap;
            dqIdx[(size_t) dqTail] = t; dqVal[(size_t) dqTail] = r;
            dqTail = (dqTail + 1) % cap;
            while (dqIdx[(size_t) dqHead] <= t - window)
                dqHead = (dqHead + 1) % cap;
            const float m = dqVal[(size_t) dqHead];
            ++t;

            // Release (Attack ist durch das Minimum sofort)
            env = (m < env) ? m : m + (env - m) * relCoef;

            // Box-Filter über Lookahead -> weicher Attack
            boxSum += env - box[(size_t) boxPos];
            box[(size_t) boxPos] = env;
            if (++boxPos == look) boxPos = 0;
            float g = (float) (boxSum / look);
            if (! enabled) g = 1.0f;

            // Audio-Delay
            const float dl = delayL[(size_t) dPos], dr = delayR[(size_t) dPos];
            delayL[(size_t) dPos] = L[i]; delayR[(size_t) dPos] = R[i];
            if (++dPos > delay) dPos = 0;

            float outL = dl * g, outR = dr * g;
            if (enabled)
            {
                outL = std::clamp (outL, -ceil, ceil);   // Sicherheitsnetz
                outR = std::clamp (outR, -ceil, ceil);
            }
            L[i] = outL; R[i] = outR;
            minGain = std::min (minGain, g);
        }
        return gainToDb (minGain);
    }

private:
    double fs = 48000.0;
    int look = 72, window = 76, delay = 81;
    std::vector<float> delayL, delayR;
    std::vector<double> box;
    double boxSum = 72.0;
    int boxPos = 0, dPos = 0;
    std::vector<long long> dqIdx;
    std::vector<float> dqVal;
    int dqHead = 0, dqTail = 0;
    long long t = 0;
    float env = 1.0f;
    TruePeakDetector det;
};
} // namespace fin
