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
inline float coefFromMs (float ms, double fs) { return std::exp (-1.0f / (std::max (0.01f, ms) * 0.001f * (float) fs)); }

/** Bandpass mit 0 dB Spitze (RBJ), für die Detektion im Dyn-EQ. */
inline BiquadCoeffs bandpass (double fs, double f, double q)
{
    const double w = 2.0 * kPi * BiquadCoeffs::clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    return BiquadCoeffs::normalise (al, 0.0, -al, 1 + al, -2 * c, 1 - al);
}

/** Linkwitz-Riley 4. Ordnung (2x Butterworth 2. Ordnung). */
struct Lr4
{
    Biquad a, b;
    void set (const BiquadCoeffs& c) { a.c = b.c = c; }
    void reset() { a.reset(); b.reset(); }
    inline float process (float x, int ch) noexcept { return b.process (a.process (x, ch), ch); }
};

/** Frequenzweiche in 3 Bänder (LR4) mit Allpass-Kompensation im tiefen Band -> phasenneutrale Summe. */
class Crossover3
{
public:
    void prepare (double sampleRate) { fs = sampleRate; f1 = f2 = -1.0f; reset(); }
    void reset() { lp1.reset(); hp1.reset(); lp2.reset(); hp2.reset(); apLp.reset(); apHp.reset(); }

    void setFreqs (float a, float b)
    {
        b = std::max (b, a * 1.5f);
        if (a == f1 && b == f2) return;
        f1 = a; f2 = b;
        lp1.set (BiquadCoeffs::lowpass (fs, f1, 0.70710678));
        hp1.set (BiquadCoeffs::highpass (fs, f1, 0.70710678));
        lp2.set (BiquadCoeffs::lowpass (fs, f2, 0.70710678));
        hp2.set (BiquadCoeffs::highpass (fs, f2, 0.70710678));
        apLp.set (BiquadCoeffs::lowpass (fs, f2, 0.70710678));
        apHp.set (BiquadCoeffs::highpass (fs, f2, 0.70710678));
    }

    inline void split (float x, int ch, float& lo, float& mid, float& hi) noexcept
    {
        const float l = lp1.process (x, ch), rest = hp1.process (x, ch);
        mid = lp2.process (rest, ch);
        hi  = hp2.process (rest, ch);
        lo  = apLp.process (l, ch) + apHp.process (l, ch);   // Allpass bei f2
    }

private:
    double fs = 48000.0;
    float f1 = -1.0f, f2 = -1.0f;
    Lr4 lp1, hp1, lp2, hp2, apLp, apHp;
};

//==============================================================================
/** Dynamischer EQ: Low-Cut (12/24/48 dB), Low-Shelf, 2 Glocken, High-Shelf.
    Jedes Band: Stereo / Mid / Side, optional dynamisch (Threshold + Range). */
class DynamicEq
{
public:
    static constexpr int kBands = 4;

    void prepare (double sampleRate) { fs = sampleRate; dirty = true; reset(); }
    void reset()
    {
        for (auto& h : hp) h.reset();
        for (auto& b : bands) { b.f.reset(); b.det.reset(); b.env = 0.0f; b.dyn = 0.0f; b.appliedDyn = 1e9f; }
    }

    static const EqBand& band (const Settings& s, int i)
    {
        switch (i) { case 0: return s.ls; case 1: return s.p1; case 2: return s.p2; default: return s.hs; }
    }

    static BiquadCoeffs bandCoeffs (int i, const EqBand& b, double fs, float gainDb)
    {
        switch (i)
        {
            case 0:  return BiquadCoeffs::lowShelf (fs, b.freq, gainDb, 0.7071);
            case 3:  return BiquadCoeffs::highShelf (fs, b.freq, gainDb, 0.7071);
            default: return BiquadCoeffs::peak (fs, b.freq, b.q, gainDb);
        }
    }

    static int hpStages (int slope) { return slope == 0 ? 1 : (slope == 1 ? 2 : 4); }
    static BiquadCoeffs hpCoeffs (int slope, int stage, double fs, float f)
    {
        static const double q12[1] = { 0.70710678 };
        static const double q24[2] = { 0.54119610, 1.30656296 };
        static const double q48[4] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        const double q = slope == 0 ? q12[0] : (slope == 1 ? q24[stage] : q48[stage]);
        return BiquadCoeffs::highpass (fs, f, q);
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        // Low-Cut
        if (dirty || s.hpFreq != lastHpFreq || s.hpSlope != lastHpSlope)
        {
            lastHpFreq = s.hpFreq; lastHpSlope = s.hpSlope;
            for (int k = 0; k < hpStages (s.hpSlope); ++k) hp[k].c = hpCoeffs (s.hpSlope, k, fs, s.hpFreq);
        }
        if (s.hpOn)
            for (int k = 0; k < hpStages (s.hpSlope); ++k)
                for (int i = 0; i < n; ++i) { L[i] = hp[k].process (L[i], 0); R[i] = hp[k].process (R[i], 1); }

        const float atk = coefFromMs (s.eqAttack, fs), rel = coefFromMs (s.eqRelease, fs);

        for (int bi = 0; bi < kBands; ++bi)
        {
            auto& st = bands[bi];
            const EqBand& p = band (s, bi);
            const bool dynamic = std::abs (p.range) > 0.01f;
            if (! dynamic && std::abs (p.gain) < 0.01f) { st.dyn = 0.0f; dynDb[bi] = 0.0f; continue; }

            const bool staticChanged = dirty || p.freq != st.last.freq || p.gain != st.last.gain || p.q != st.last.q || p.mode != st.last.mode;
            if (staticChanged)
            {
                st.last = p;
                st.det.c = bi == 0 ? BiquadCoeffs::lowpass (fs, p.freq, 0.7071)
                         : bi == 3 ? BiquadCoeffs::highpass (fs, p.freq, 0.7071)
                                   : bandpass (fs, p.freq, std::max (0.5f, p.q));
                st.appliedDyn = 1e9f;   // Neuberechnung erzwingen
            }
            if (! dynamic) st.dyn = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]);

                if (dynamic)
                {
                    float d;
                    if (p.mode == kStereo) d = std::max (std::abs (st.det.process (L[i], 0)), std::abs (st.det.process (R[i], 1)));
                    else d = std::abs (st.det.process (p.mode == kMid ? m : sd, 0));
                    st.env = d > st.env ? atk * st.env + (1.0f - atk) * d : rel * st.env + (1.0f - rel) * d;

                    if ((++st.counter & 15) == 0)
                    {
                        const float over = gainToDb (st.env) - p.thresh;
                        st.dyn = over > 0.0f ? std::copysign (std::min (std::abs (p.range), over * 0.5f), p.range) : 0.0f;
                    }
                }

                if (std::abs (st.dyn - st.appliedDyn) > 0.05f)
                {
                    st.appliedDyn = st.dyn;
                    st.f.c = bandCoeffs (bi, p, fs, p.gain + st.dyn);
                }

                if (p.mode == kStereo)
                {
                    L[i] = st.f.process (L[i], 0);
                    R[i] = st.f.process (R[i], 1);
                }
                else if (p.mode == kMid)
                {
                    const float mf = st.f.process (m, 0);
                    L[i] = mf + sd; R[i] = mf - sd;
                }
                else
                {
                    const float sf = st.f.process (sd, 0);
                    L[i] = m + sf; R[i] = m - sf;
                }
            }
            dynDb[bi] = st.dyn;
        }
        dirty = false;
    }

    /** Frequenzgang für die GUI (statisch + optional aktuelle Dynamik). */
    static double responseDb (const Settings& s, double f, double sampleRate, const float* dyn = nullptr)
    {
        double db = 0.0;
        if (s.hpOn)
            for (int k = 0; k < hpStages (s.hpSlope); ++k)
                db += hpCoeffs (s.hpSlope, k, sampleRate, s.hpFreq).magnitudeDb (f, sampleRate);
        for (int i = 0; i < kBands; ++i)
        {
            const auto& b = band (s, i);
            const float g = b.gain + (dyn ? dyn[i] : 0.0f);
            if (std::abs (g) > 0.01f)
                db += bandCoeffs (i, b, sampleRate, g).magnitudeDb (f, sampleRate);
        }
        return db;
    }

    float dynDb[kBands] {};

private:
    struct State
    {
        Biquad f, det;
        EqBand last;
        float env = 0.0f, dyn = 0.0f, appliedDyn = 1e9f;
        unsigned counter = 0;
    };

    double fs = 48000.0;
    Biquad hp[4];
    float lastHpFreq = -1.0f;
    int lastHpSlope = -1;
    State bands[kBands];
    bool dirty = true;
};

//==============================================================================
/** Stereo: Bass mono unter monoFreq, Breite für Mitten und Höhen getrennt. */
class StereoTool
{
public:
    void prepare (double sampleRate) { xo.prepare (sampleRate); }
    void reset() { xo.reset(); }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        const float wm = s.width * 0.01f, wh = s.widthHigh * 0.01f;
        if (! s.monoOn && std::abs (wm - 1.0f) < 1e-4f && std::abs (wh - 1.0f) < 1e-4f) return;
        xo.setFreqs (s.monoFreq, s.widthXover);

        for (int i = 0; i < n; ++i)
        {
            float lL, mL, hL, lR, mR, hR;
            xo.split (L[i], 0, lL, mL, hL);
            xo.split (R[i], 1, lR, mR, hR);
            if (s.monoOn) lL = lR = 0.5f * (lL + lR);
            const float mm = 0.5f * (mL + mR), ms = 0.5f * (mL - mR) * wm;
            const float hm = 0.5f * (hL + hR), hs = 0.5f * (hL - hR) * wh;
            L[i] = lL + mm + ms + hm + hs;
            R[i] = lR + mm - ms + hm - hs;
        }
    }

private:
    Crossover3 xo;
};

//==============================================================================
/** Transient-Shaper: Attack/Sustain +-12 dB, pegelunabhängig (Hüllkurven-Differenz). */
class TransientShaper
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aFast = coefFromMs (0.5f, fs); aSlow = coefFromMs (20.0f, fs); rAtt = coefFromMs (60.0f, fs);
        aSus = coefFromMs (1.0f, fs);  rShort = coefFromMs (50.0f, fs); rLong = coefFromMs (300.0f, fs);
        gSmooth = coefFromMs (1.0f, fs);
        reset();
    }
    void reset() { eF = eS = eA = eB = 0.0f; g = 0.0f; }

    /** Gibt die stärkste Gain-Änderung des Blocks in dB zurück. */
    float process (float* L, float* R, int n, float attackDb, float sustainDb) noexcept
    {
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i])) + 1e-9f;
            eF = x > eF ? aFast * eF + (1 - aFast) * x : rAtt * eF + (1 - rAtt) * x;
            eS = x > eS ? aSlow * eS + (1 - aSlow) * x : rAtt * eS + (1 - rAtt) * x;
            eA = x > eA ? aSus * eA + (1 - aSus) * x : rShort * eA + (1 - rShort) * x;
            eB = x > eB ? aSus * eB + (1 - aSus) * x : rLong * eB + (1 - rLong) * x;

            const float att = std::clamp (gainToDb (eF) - gainToDb (eS), 0.0f, 12.0f) / 12.0f;
            const float sus = std::clamp (gainToDb (eB) - gainToDb (eA), 0.0f, 12.0f) / 12.0f;
            const float target = attackDb * att + sustainDb * sus;
            g = gSmooth * g + (1.0f - gSmooth) * target;

            const float gl = dbToGain (g);
            L[i] *= gl; R[i] *= gl;
            if (std::abs (g) > std::abs (peak)) peak = g;
        }
        return peak;
    }

private:
    double fs = 48000.0;
    float aFast = 0, aSlow = 0, rAtt = 0, aSus = 0, rShort = 0, rLong = 0, gSmooth = 0;
    float eF = 0, eS = 0, eA = 0, eB = 0, g = 0;
};

//==============================================================================
struct CompParams
{
    float thresh = -12.0f, ratio = 2.0f, attack = 30.0f, release = 150.0f, knee = 6.0f, makeup = 0.0f, mix = 100.0f;
};

/** Stereo-gelinkter Feed-Forward-Kompressor mit Soft-Knee und Parallel-Mix. */
class Compressor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { grSmooth = 0.0f; }

    static CompParams fromSettings (const Settings& s) { return { s.thresh, s.ratio, s.attack, s.release, s.knee, s.makeup, s.mix }; }

    /** Gibt die maximale Gain-Reduction des Blocks in dB (<= 0) zurück. */
    float process (float* L, float* R, int n, const CompParams& p) noexcept
    {
        const float atk = coefFromMs (std::max (0.05f, p.attack), fs);
        const float rel = coefFromMs (std::max (1.0f, p.release), fs);
        const float T = p.thresh, Rt = std::max (1.0f, p.ratio), W = std::max (0.0f, p.knee);
        const float makeup = dbToGain (p.makeup), mix = std::clamp (p.mix * 0.01f, 0.0f, 1.0f);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float xDb = gainToDb (std::max (std::abs (L[i]), std::abs (R[i])));
            const float d = xDb - T;
            float yDb;
            if (2.0f * d < -W)                 yDb = xDb;
            else if (2.0f * std::abs (d) <= W) yDb = xDb + (1.0f / Rt - 1.0f) * (d + W * 0.5f) * (d + W * 0.5f) / (2.0f * W);
            else                               yDb = T + d / Rt;
            const float gr = yDb - xDb;

            grSmooth = (gr < grSmooth) ? atk * grSmooth + (1.0f - atk) * gr
                                       : rel * grSmooth + (1.0f - rel) * gr;
            maxGr = std::min (maxGr, grSmooth);

            const float g = (1.0f - mix) + dbToGain (grSmooth) * makeup * mix;
            L[i] *= g; R[i] *= g;
        }
        return maxGr;
    }

private:
    double fs = 48000.0;
    float grSmooth = 0.0f;
};

//==============================================================================
/** 3-Band-Kompressor (LR4-Weichen, phasenneutral), Solo/Bypass pro Band. */
class MultibandCompressor
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        xo.prepare (sampleRate);
        for (auto& c : comp) c.prepare (sampleRate);
        for (auto& b : buf) for (auto& ch : b) ch.assign ((size_t) maxBlock, 0.0f);
    }
    void reset() { xo.reset(); for (auto& c : comp) c.reset(); }

    void process (float* L, float* R, int n, const Settings& s, float* grOut) noexcept
    {
        xo.setFreqs (s.mbX1, s.mbX2);
        for (int i = 0; i < n; ++i)
        {
            xo.split (L[i], 0, buf[0][0][(size_t) i], buf[1][0][(size_t) i], buf[2][0][(size_t) i]);
            xo.split (R[i], 1, buf[0][1][(size_t) i], buf[1][1][(size_t) i], buf[2][1][(size_t) i]);
        }
        const bool anySolo = s.mb[0].solo || s.mb[1].solo || s.mb[2].solo;
        for (int b = 0; b < 3; ++b)
        {
            const auto& p = s.mb[b];
            if (p.bypass) { grOut[b] = 0.0f; continue; }
            const CompParams cp { p.thresh, p.ratio, p.attack, p.release, 6.0f, p.gain, 100.0f };
            grOut[b] = comp[b].process (buf[b][0].data(), buf[b][1].data(), n, cp);
        }
        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (int b = 0; b < 3; ++b)
                if (! anySolo || s.mb[b].solo) { l += buf[b][0][(size_t) i]; r += buf[b][1][(size_t) i]; }
            L[i] = l; R[i] = r;
        }
    }

private:
    Crossover3 xo;
    Compressor comp[3];
    std::vector<float> buf[3][2];
};

//==============================================================================
/** Saturation (läuft oversampled): Tape / Tube / Soft, Ton-Tilt, Mix, Auto-Gain. */
class Saturation
{
public:
    void prepare (double osRate) { fs = osRate; envC = coefFromMs (300.0f, fs); dcC = std::exp (-2.0f * (float) kPi * 8.0f / (float) fs); tone = 1e9f; reset(); }
    void reset() { pIn = pOut = 1e-6f; dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f; tiltLo.reset(); tiltHi.reset(); }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        if (s.satTone != tone)
        {
            tone = s.satTone;
            const float t = tone * 0.01f * 4.0f;
            tiltLo.c = BiquadCoeffs::lowShelf (fs, 300.0, -t);
            tiltHi.c = BiquadCoeffs::highShelf (fs, 3000.0, t);
        }
        const float g = dbToGain (s.satDrive), mix = std::clamp (s.satMix * 0.01f, 0.0f, 1.0f);
        const bool useTone = std::abs (tone) > 0.01f;

        for (int i = 0; i < n; ++i)
        {
            float x[2] = { L[i], R[i] }, y[2];
            for (int c = 0; c < 2; ++c)
            {
                const float v = x[c] * g;
                float w;
                switch (s.satModel)
                {
                    case 1:  w = std::tanh (v + 0.3f) - std::tanh (0.3f); break;          // Tube: asymmetrisch -> gerade Obertöne
                    case 2:  w = (2.0f / (float) kPi) * std::atan (v * (float) kPi * 0.5f); break; // Soft
                    default: w = std::tanh (v); break;                                     // Tape
                }
                // DC-Blocker (wegen Tube)
                const float dy = w - dcX[c] + dcC * dcY[c];
                dcX[c] = w; dcY[c] = dy;
                w = dy;
                if (useTone) w = tiltHi.process (tiltLo.process (w, c), c);
                y[c] = w;
            }
            // Auto-Gain: Wet auf Lautheit des Dry-Signals bringen
            pIn  = envC * pIn  + (1.0f - envC) * (x[0] * x[0] + x[1] * x[1]);
            pOut = envC * pOut + (1.0f - envC) * (y[0] * y[0] + y[1] * y[1]);
            const float comp = s.satAuto ? std::clamp (std::sqrt (pIn / std::max (pOut, 1e-12f)), 0.1f, 4.0f) : 1.0f;
            L[i] = x[0] * (1.0f - mix) + y[0] * comp * mix;
            R[i] = x[1] * (1.0f - mix) + y[1] * comp * mix;
        }
    }

private:
    double fs = 192000.0;
    float envC = 0.0f, dcC = 0.0f, tone = 1e9f;
    float pIn = 1e-6f, pOut = 1e-6f;
    float dcX[2] {}, dcY[2] {};
    Biquad tiltLo, tiltHi;
};

/** Clipper (läuft oversampled) mit weichem Knie. Gibt maximale Reduktion in dB zurück. */
inline float processClipper (float* L, float* R, int n, float ceilingDb, float softPct) noexcept
{
    const float c = dbToGain (ceilingDb);
    const float ks = std::clamp (softPct * 0.01f, 0.0f, 0.95f) * c;
    const float t = c - ks;
    float minRatio = 1.0f;
    auto clip = [&] (float x)
    {
        const float a = std::abs (x);
        if (a <= t) return x;
        const float y = ks > 1e-9f ? t + ks * std::tanh ((a - t) / ks) : c;
        minRatio = std::min (minRatio, y / a);
        return std::copysign (y, x);
    };
    for (int i = 0; i < n; ++i) { L[i] = clip (L[i]); R[i] = clip (R[i]); }
    return gainToDb (minRatio);
}

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
        const float relCoef = coefFromMs (std::max (1.0f, releaseMs), fs);
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
            if (++dPos >= delay) dPos = 0;   // Verzögerung exakt 'delay'

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
