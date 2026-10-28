// DSP-Selbsttest für die Finisher-Engine (ohne JUCE).  Exit-Code 0 = alles OK.
#include "../Source/dsp/Engine.h"
#include <cstdio>
#include <random>
#include <vector>

using namespace fin;

namespace
{
int failures = 0;
void check (bool ok, const char* name, double value, const char* unit)
{
    std::printf ("[%s] %-48s %9.2f %s\n", ok ? " OK " : "FAIL", name, value, unit);
    if (! ok) ++failures;
}

/** Bandbegrenztes Rauschen (bis ~15 kHz), Stereo, leicht dekorreliert. */
void makeNoise (std::vector<float>& L, std::vector<float>& R, size_t n, double fs, float level)
{
    std::mt19937 rng (1234);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    Biquad lp1, lp2;
    lp1.c = lp2.c = BiquadCoeffs::lowpass (fs, 15000.0, 0.7071);
    L.resize (n); R.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const float a = nd (rng), b = nd (rng);
        L[i] = lp2.process (lp1.process (level * a, 0), 0);
        R[i] = lp2.process (lp1.process (level * (0.7f * a + 0.3f * b), 1), 1);
    }
}

Settings neutral()
{
    Settings s;
    s.eqOn = false; s.monoOn = false; s.compOn = false; s.limOn = false;
    s.trOn = s.mbOn = s.satOn = s.clipOn = false;
    return s;
}

/** Läuft die Engine, Ausgabe latenzbereinigt. */
std::vector<float> run (const Settings& s, const std::vector<float>& L, const std::vector<float>& R, std::vector<float>* outR,
                        double fs, Engine* keep = nullptr, int block = 512)
{
    Engine local;
    Engine& e = keep ? *keep : local;
    e.setSettings (s);
    e.prepare (fs, block);
    e.setSettings (s);
    const int lat = e.latency();
    const size_t total = L.size() + (size_t) lat;
    std::vector<float> oL, oR, bL ((size_t) block), bR ((size_t) block);
    for (size_t pos = 0; pos < total; pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const size_t k = pos + (size_t) i;
            bL[(size_t) i] = k < L.size() ? L[k] : 0.0f;
            bR[(size_t) i] = k < R.size() ? R[k] : 0.0f;
        }
        e.process (bL.data(), bR.data(), n);
        for (int i = 0; i < n; ++i)
            if (pos + (size_t) i >= (size_t) lat) { oL.push_back (bL[(size_t) i]); oR.push_back (bR[(size_t) i]); }
    }
    if (outR) *outR = oR;
    return oL;
}

double rmsDb (const std::vector<float>& x, size_t from = 0)
{
    double s = 0.0;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return 10.0 * std::log10 (s / std::max<size_t> (1, x.size() - from) + 1e-30);
}

double diffDb (const std::vector<float>& a, const std::vector<float>& b, size_t from)
{
    std::vector<float> d (a.size());
    for (size_t i = 0; i < a.size(); ++i) d[i] = a[i] - b[i];
    return rmsDb (d, from) - rmsDb (b, from);
}
} // namespace

int main()
{
    const double fs = 48000.0;
    const size_t N = (size_t) (fs * 8);
    std::vector<float> L, R, oR;
    makeNoise (L, R, N, fs, 0.1f);
    const size_t skip = (size_t) fs;   // Einschwingen ignorieren

    {
        Engine e; e.prepare (fs, 512);
        std::printf ("Latenz: %d Samples (%.2f ms)\n\n", e.latency(), e.latency() / fs * 1000.0);
    }

    // 1) Neutrale Kette = Original (nur Oversampling-Filter)
    auto oL = run (neutral(), L, R, &oR, fs);
    check (diffDb (oL, L, skip) < -50.0, "Null-Test neutrale Kette (Restfehler)", diffDb (oL, L, skip), "dB");

    // 2) A/B-Bypass bei neutraler Kette: exakt das Original
    {
        auto s = neutral(); s.abBypass = true;
        auto o = run (s, L, R, nullptr, fs);
        check (diffDb (o, L, skip) < -55.0, "A/B-Bypass = Original (Abgleich < 0.02 dB)", diffDb (o, L, skip), "dB");
    }
    // 3) Delta bei neutraler Kette: nahezu Stille
    {
        auto s = neutral(); s.delta = true;
        auto o = run (s, L, R, nullptr, fs);
        check (rmsDb (o, skip) - rmsDb (L, skip) < -50.0, "Delta neutrale Kette (relativ)", rmsDb (o, skip) - rmsDb (L, skip), "dB");
    }
    // 4) Multiband mit Ratio 1 = phasenneutrale Summe (Pegel gleich)
    {
        auto s = neutral(); s.mbOn = true;
        for (auto& b : s.mb) b.ratio = 1.0f;
        auto o = run (s, L, R, nullptr, fs);
        check (std::abs (rmsDb (o, skip) - rmsDb (L, skip)) < 0.1, "Multiband Ratio 1: Pegelabweichung", rmsDb (o, skip) - rmsDb (L, skip), "dB");
    }
    // 5) Stereo-Tool mit Breite 100 / Mono aus, aber 3-Band aktiv (Breite 101 %): Pegel gleich
    {
        auto s = neutral(); s.width = 100.0f; s.widthHigh = 100.5f;
        auto o = run (s, L, R, nullptr, fs);
        check (std::abs (rmsDb (o, skip) - rmsDb (L, skip)) < 0.1, "Stereo 3-Band: Pegelabweichung", rmsDb (o, skip) - rmsDb (L, skip), "dB");
    }
    // 6) Mono-Bass: Seitenanteil unter 80 Hz verschwindet
    {
        auto s = neutral(); s.monoOn = true; s.monoFreq = 150.0f;
        std::vector<float> l (N), r (N);
        for (size_t i = 0; i < N; ++i) { const float v = 0.3f * std::sin (2.0f * (float) kPi * 40.0f * (float) i / (float) fs); l[i] = v; r[i] = -v; }
        std::vector<float> outR2;
        auto o = run (s, l, r, &outR2, fs);
        std::vector<float> side (o.size());
        for (size_t i = 0; i < o.size(); ++i) side[i] = 0.5f * (o[i] - outR2[i]);
        check (rmsDb (side, skip) - rmsDb (l, skip) < -40.0, "Mono-Bass 150 Hz: Side-Anteil bei 40 Hz", rmsDb (side, skip) - rmsDb (l, skip), "dB");
    }
    // 7) Clipper: lauter Sinus, Peaks vor dem Limiter begrenzt
    {
        auto s = neutral(); s.clipOn = true; s.clipThresh = 0.0f; s.ceiling = -1.0f; s.clipSoft = 0.0f; s.drive = 6.0f;
        std::vector<float> l (N);
        for (size_t i = 0; i < N; ++i) l[i] = 0.9f * std::sin (2.0f * (float) kPi * 100.0f * (float) i / (float) fs);
        auto o = run (s, l, l, nullptr, fs);
        float pk = 0.0f; for (size_t i = skip; i < o.size(); ++i) pk = std::max (pk, std::abs (o[i]));
        check (gainToDb (pk) < -0.8f, "Clipper hart @ -1 dB: Sample-Peak", gainToDb (pk), "dBFS");
    }
    // 8) Dyn-EQ: Band mit Range -6 dB auf Bell bei 1 kHz -> laute 1-kHz-Sinus wird leiser
    {
        auto s = neutral(); s.eqOn = true; s.hpOn = false;
        s.p1 = { 1000.0f, 0.0f, 1.0f, kStereo, -30.0f, -6.0f };
        std::vector<float> l (N);
        for (size_t i = 0; i < N; ++i) l[i] = 0.5f * std::sin (2.0f * (float) kPi * 1000.0f * (float) i / (float) fs);
        Engine e;
        auto o = run (s, l, l, nullptr, fs, &e);
        check (rmsDb (o, skip) - rmsDb (l, skip) < -5.0, "Dyn-EQ Range -6 dB (1 kHz laut)", rmsDb (o, skip) - rmsDb (l, skip), "dB");
        check (e.eqDyn[1].load() < -5.0f, "Dyn-EQ Anzeige aktuelle Absenkung", e.eqDyn[1].load(), "dB");
    }
    // 9) Volllast: alles an, Ceiling -1 dBTP, kein NaN, True Peak hält, verschiedene Blockgrößen
    for (int block : { 1, 64, 511, 4096 })
    {
        Settings s;
        s.eqOn = true; s.ls.gain = 2.0f; s.p1.range = -3.0f; s.hs.gain = 1.5f; s.hs.mode = kSide;
        s.trOn = true; s.trAttack = 4.0f; s.trSustain = -2.0f;
        s.compOn = true; s.mbOn = true; s.satOn = true; s.satModel = 1; s.satTone = 30.0f;
        s.clipOn = true; s.drive = 12.0f; s.width = 120.0f; s.widthHigh = 130.0f;
        std::vector<float> l, r;
        makeNoise (l, r, (size_t) (fs * (block == 1 ? 1 : 4)), fs, 0.3f);
        Engine e;
        auto o = run (s, l, r, nullptr, fs, &e, block);
        bool finite = true; for (float v : o) finite &= std::isfinite (v);
        char name[64]; std::snprintf (name, sizeof name, "Volllast Block %d: True Peak (Ziel <= -1.0)", block);
        check (finite && e.truePeakMax.load() <= -0.95f, name, e.truePeakMax.load(), "dBTP");
    }

    std::printf ("\n%s (%d Fehler)\n", failures == 0 ? "ALLE DSP-TESTS OK" : "DSP-TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
