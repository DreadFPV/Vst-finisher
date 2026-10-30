#pragma once

#include <atomic>
#include <vector>
#include <string>
#include "Settings.h"
#include "Modules.h"
#include "LoudnessMeter.h"
#include "Oversampler.h"

namespace fin
{
/** Ein Punkt der Verlaufsanzeige (alle 100 ms). */
struct HistoryPoint
{
    float inPeak = 0.0f;      // linear, vor der Kette
    float outPeak = 0.0f;     // linear, nach der Kette
    float shortTerm = -200.0f;
    float momentary = -200.0f;
    float limGrDb = 0.0f;
};

/** Messwerte für die Ziel-Automatik. */
struct Measurement
{
    double integrated = LoudnessMeter::kSilence;   // Output, LUFS
    double lra = 0.0;                               // Output, LU
    double truePeak = -200.0;                       // Output, dBTP (max)
    double compInIntegrated = LoudnessMeter::kSilence;
    /** 90. Perzentil der 10-ms-Spitzen am Kompressor-Eingang: [0] Vollband, [1..3] Multiband-Bänder (dBFS). */
    double levelP90[4] { -200.0, -200.0, -200.0, -200.0 };
    double limGrP95 = 0.0;    // Limiter-Gain-Reduction (dB, positiv), 95. Perzentil über den Track
    double clipGrP95 = 0.0;   // Clipper-Reduktion (dB, positiv), 95. Perzentil
    double seconds = 0.0;
};

/** Histogramm für Gain-Reduction-Werte (0 ... 24 dB, 0.25-dB-Schritte). */
struct GrStats
{
    static constexpr int kBins = 96;
    double h[kBins] {};
    double total = 0.0;
    void clear() { std::fill (std::begin (h), std::end (h), 0.0); total = 0.0; }
    void add (float grDb) { h[std::clamp ((int) (grDb / 0.25f), 0, kBins - 1)] += 1.0; total += 1.0; }
    double percentile (double q) const
    {
        if (total < 1.0) return 0.0;
        double cum = 0.0;
        for (int b = 0; b < kBins; ++b) { cum += h[b]; if (cum >= q * total) return b * 0.25; }
        return kBins * 0.25;
    }
};

struct Targets
{
    float lufs = -9.0f;
    float truePeak = -1.0f;
    float lra = 7.0f;
};

/** Ergebnis der Ziel-Automatik: neue Settings + Hinweistext. */
struct TargetResult
{
    Settings settings;
    bool ok = false;
    std::string message;
};

/** Komplette Mastering-Kette inkl. Messung. Ohne JUCE-Abhängigkeit (auch im CLI-Tool nutzbar). */
class Engine
{
public:
    static constexpr int kHistorySize = 18000;     // 30 Minuten @ 10 Hz

    Engine() : history ((size_t) kHistorySize) {}

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        stepLen = std::max (1, (int) std::lround (fs * 0.1));
        const int maxSeg = stepLen;                       // process() teilt in Segmente <= stepLen
        eq.prepare (fs); stereo.prepare (fs); trans.prepare (fs); comp.prepare (fs);
        mbComp.prepare (fs, maxSeg);
        levels.prepare (fs);
        os.prepare (maxSeg); sat.prepare (fs * 4.0);
        limiter.prepare (fs);
        dryDelay.prepare (latency());
        dryL.assign ((size_t) maxSeg, 0.0f); dryR.assign ((size_t) maxSeg, 0.0f);
        outMeter.prepare (fs); compInMeter.prepare (fs); dryMeter.prepare (fs);
        corrC = coefFromMs (300.0f, fs);
        resetMeters();
        curIn = dbToGain (settings.inGainDb); curDrive = dbToGain (settings.drive); curOut = dbToGain (settings.outGainDb);
        curMatch = 1.0f; matchDbSmooth = 0.0f;
    }

    void resetDsp()
    {
        eq.reset(); stereo.reset(); trans.reset(); comp.reset(); mbComp.reset(); os.reset(); sat.reset();
        limiter.reset(); dryDelay.reset();
    }
    /** Gesamtlatenz: 4x-Oversampling + Limiter-Lookahead (konstant, unabhängig von Ein/Aus). */
    int latency() const { return Oversampler4x::kLatency + limiter.latency(); }
    double sampleRate() const { return fs; }

    void setSettings (const Settings& s) { settings = s; }
    const Settings& getSettings() const { return settings; }

    /** Thread-sicher: Messung zurücksetzen (wird im nächsten Block ausgeführt). */
    void requestMeterReset() { resetRequested.store (true); }

    void process (float* L, float* R, int n) noexcept
    {
        if (resetRequested.exchange (false))
            resetMeters();
        compGrBlock = 0.0f; limGrBlock = 0.0f; clipGrBlock = 0.0f; trBlock = 0.0f;
        mbGrBlock[0] = mbGrBlock[1] = mbGrBlock[2] = 0.0f;

        int done = 0;
        while (done < n)
        {
            const int len = std::min (n - done, stepLen - histPos);
            processSegment (L + done, R + done, len);
            done += len;
            histPos += len;
            if (histPos >= stepLen)
            {
                histPos = 0;
                pushHistory();
            }
        }
        publish();
    }

    //==============================================================================
    // Messwerte (von der GUI lesbar)
    std::atomic<float> momentary { -200.0f }, shortTerm { -200.0f }, integrated { -200.0f }, lra { 0.0f };
    std::atomic<float> truePeakMax { -200.0f }, truePeakNow { -200.0f };
    std::atomic<float> compGr { 0.0f }, limGr { 0.0f };
    std::atomic<float> compInIntegrated { -200.0f };
    std::atomic<float> measuredSeconds { 0.0f };
    std::atomic<float> correlation { 1.0f };      // -1 ... +1 (Output)
    std::atomic<float> matchDb { 0.0f };          // Pegelabgleich für A/B (dB)
    std::atomic<float> clipGr { 0.0f }, trGain { 0.0f };
    std::atomic<float> mbGr[3] { { 0.0f }, { 0.0f }, { 0.0f } };
    std::atomic<float> eqDyn[DynamicEq::kBands] { { 0.0f }, { 0.0f }, { 0.0f }, { 0.0f } };
    std::atomic<float> levelP90[4] { { -200.0f }, { -200.0f }, { -200.0f }, { -200.0f } };
    std::atomic<float> limGrP95 { 0.0f }, clipGrP95 { 0.0f };

    /** Verlauf (Single-Producer). historyCount = Gesamtanzahl geschriebener Punkte. */
    std::vector<HistoryPoint> history;
    std::atomic<long long> historyCount { 0 };

    Measurement measurement() const
    {
        Measurement m;
        m.integrated = integrated.load(); m.lra = lra.load(); m.truePeak = truePeakMax.load();
        m.compInIntegrated = compInIntegrated.load(); m.seconds = measuredSeconds.load();
        for (int k = 0; k < 4; ++k) m.levelP90[k] = levelP90[k].load();
        m.limGrP95 = limGrP95.load(); m.clipGrP95 = clipGrP95.load();
        return m;
    }

    //==============================================================================
    /** Ziel-Automatik: berechnet neue Einstellungen aus Messung + Zielen.
        LUFS -> Limiter-Drive, True Peak -> Ceiling, LRA -> Kompressor (Vorschlag). */
    static TargetResult computeTargets (const Settings& cur, const Measurement& m, const Targets& t)
    {
        TargetResult r;
        r.settings = cur;
        auto& s = r.settings;

        if (m.seconds < 10.0 || m.integrated < -69.0)
        {
            r.message = "Zu wenig gemessen. Track mind. 10 s abspielen (besser komplett), dann nochmal.";
            return r;
        }

        std::string msg;

        // 1) True Peak
        s.limOn = true;
        s.ceiling = std::clamp (t.truePeak, -6.0f, 0.0f);

        // 2) Dynamik auf den Track kalibrieren: Schwellen aus den gemessenen Pegeln (90. Perzentil),
        //    so dass Kompressor und Multiband nur moderat regeln. LRA steuert die Menge der Kompression.
        auto grAt   = [] (double level, double thr, double ratio) { return std::max (0.0, (level - thr) * (1.0 - 1.0 / std::max (1.01, ratio))); };
        auto thrFor = [] (double level, double gr, double ratio)  { return level - gr / (1.0 - 1.0 / std::max (1.2, ratio)); };
        const double excess = m.lra - t.lra;

        if (m.levelP90[0] > -60.0)
        {
            double compGr = cur.compOn ? grAt (m.levelP90[0], cur.thresh, cur.ratio) : 0.0;
            if (compGr < 0.3) compGr = 1.5;                     // Startwert: sanfter Glue
            if (excess > 0.5)
            {
                compGr += std::min (3.0, excess * 0.6);
                s.compOn = true;
                msg += "LRA zu hoch → etwas mehr Kompression. ";
            }
            else if (excess < -1.0 && cur.compOn)
            {
                compGr += excess * 0.4;
                msg += "LRA zu niedrig → weniger Kompression. ";
            }
            compGr = std::clamp (compGr, 0.5, 6.0);

            if (s.compOn)
            {
                if (! cur.compOn) { s.attack = 30.0f; s.release = 200.0f; s.knee = 6.0f; s.mix = 100.0f; s.makeup = 0.0f; s.ratio = 2.0f; }
                s.thresh = (float) std::clamp (thrFor (m.levelP90[0], compGr, s.ratio), -40.0, 0.0);
            }
            if (s.mbOn)
            {
                const double grTarget[3] = { 2.5, 2.0, 2.0 };   // am 90. Perzentil -> Anzeige typ. 1-2 dB
                for (int b = 0; b < 3; ++b)
                    if (m.levelP90[b + 1] > -60.0 && s.mb[b].ratio > 1.05f)
                        s.mb[b].thresh = (float) std::clamp (thrFor (m.levelP90[b + 1], grTarget[b], s.mb[b].ratio), -40.0, 0.0);
                msg += "Multiband auf den Track eingestellt (max. 1.5–2 dB pro Band). ";
            }
        }
        else if (excess > 0.5 && ! cur.compOn)
        {
            s.compOn = true;
            s.attack = 30.0f; s.release = 200.0f; s.knee = 6.0f; s.mix = 100.0f; s.makeup = 0.0f; s.ratio = 2.0f;
            s.thresh = (float) std::clamp (m.compInIntegrated + 4.0 - excess, -40.0, 0.0);
        }

        // 3) LUFS -> Drive (Kompressor-Änderung grob kompensieren)
        double delta = (double) t.lufs - m.integrated;
        if (s.compOn && cur.compOn != s.compOn)
            delta += 1.0;
        // Im Limiting-Bereich bringt 1 dB Drive weniger als 1 dB Lautheit -> etwas mehr geben
        if (delta > 0.0 && cur.limOn && cur.drive > 0.5f)
            delta *= 1.4;
        s.drive = (float) std::clamp ((double) cur.drive + delta, 0.0, 24.0);
        if (cur.drive + delta < 0.0)
        {
            s.inGainDb = (float) std::clamp ((double) cur.inGainDb + cur.drive + delta, -24.0, 24.0);
            s.drive = 0.0f;
        }
        // 4) Spitzen verteilen: Limiter max. ~3 dB, darüber nimmt ein weicher Clipper die kurzen Spitzen
        //    (Kick/Snare). Ein Limiter mit viel GR duckt den ganzen Mix inkl. Bass -> kraftlos/verzerrt.
        const double projectedLim = m.limGrP95 + std::max (0.0, (double) s.drive - cur.drive) * 0.8;
        if (projectedLim > 3.0)
        {
            if (! cur.clipOn)
            {
                s.clipOn = true; s.clipThresh = 2.5f; s.clipSoft = 50.0f;
                msg += "Limiter würde zu viel arbeiten → weicher Clipper übernimmt die Spitzen. ";
            }
            else if (cur.clipThresh > 1.5f)
            {
                s.clipThresh = std::max (1.5f, cur.clipThresh - 0.5f);
                msg += "Clipper etwas früher, damit der Limiter entlastet wird. ";
            }
        }
        if (m.clipGrP95 > 3.5 || (projectedLim > 4.5 && s.clipOn && s.clipThresh <= 1.5f))
        {
            const double realistic = std::round ((t.lufs - std::max (1.0, m.clipGrP95 - 3.0) - 1.0) * 2.0) / 2.0;
            msg += "Achtung: Ziel ist für diesen Mix sehr laut (Spitzen werden stark gekappt). Empfehlung: Ziel "
                 + std::to_string ((int) std::floor (realistic)) + " LUFS oder weniger. ";
        }

        msg += "Angewendet. Track nochmal komplett abspielen und prüfen (2–3 Durchgänge).";
        r.message = msg;
        r.ok = true;
        return r;
    }

private:
    void resetMeters()
    {
        outMeter.reset(); compInMeter.reset(); dryMeter.reset(); levels.reset();
        for (auto& v : levelP90) v.store (-200.0f);
        sLR = sLL = sRR = 0.0;
        histPos = 0; segInPeak = segOutPeak = 0.0f; segLimGr = 0.0f; segClipGr = 0.0f;
        limStats.clear(); clipStats.clear(); limGrP95 = 0.0f; clipGrP95 = 0.0f;
        tpMax = -200.0f; tpNow = -200.0f;
        tpDet.reset();
        historyCount.store (0);
        momentary = shortTerm = integrated = compInIntegrated = -200.0f;
        lra = 0.0f; truePeakMax = -200.0f; measuredSeconds = 0.0f;
    }

    void processSegment (float* L, float* R, int n) noexcept
    {
        const Settings& s = settings;

        // Dry-Abgriff (Original) für A/B und Delta
        std::copy (L, L + n, dryL.begin());
        std::copy (R, R + n, dryR.begin());
        dryMeter.process (dryL.data(), dryR.data(), n);
        dryDelay.process (dryL.data(), dryR.data(), n);

        // Input-Gain (Rampe)
        ramp (L, R, n, curIn, dbToGain (s.inGainDb));
        for (int i = 0; i < n; ++i) segInPeak = std::max (segInPeak, std::max (std::abs (L[i]), std::abs (R[i])));

        if (s.eqOn) eq.process (L, R, n, s);
        else for (int b = 0; b < DynamicEq::kBands; ++b) eq.dynDb[b] = 0.0f;
        stereo.process (L, R, n, s);
        if (s.trOn) trBlock = absMax (trBlock, trans.process (L, R, n, s.trAttack, s.trSustain));

        compInMeter.process (L, R, n);
        levels.process (L, R, n, s.mbX1, s.mbX2);
        if (s.compOn) compGrBlock = std::min (compGrBlock, comp.process (L, R, n, Compressor::fromSettings (s)));
        if (s.mbOn)
        {
            float gr[3];
            mbComp.process (L, R, n, s, gr);
            for (int b = 0; b < 3; ++b) mbGrBlock[b] = std::min (mbGrBlock[b], gr[b]);
        }

        // Drive -> [4x: Saturation -> Clipper] -> Limiter
        ramp (L, R, n, curDrive, dbToGain (s.drive));
        os.up (L, R, n);
        if (s.satOn) sat.process (os.hiL(), os.hiR(), 4 * n, s);
        if (s.clipOn)
        {
            const float cg = processClipper (os.hiL(), os.hiR(), 4 * n, s.ceiling + s.clipThresh, s.clipSoft);
            clipGrBlock = std::min (clipGrBlock, cg);
            segClipGr = std::min (segClipGr, cg);
        }
        os.down (L, R, n);

        const float lg = limiter.process (L, R, n, s.limOn, s.ceiling, s.limRelease);
        segLimGr = std::min (segLimGr, lg);
        limGrBlock = std::min (limGrBlock, lg);

        ramp (L, R, n, curOut, dbToGain (std::min (0.0f, s.outGainDb)));

        // Messung (bearbeitetes Signal)
        outMeter.process (L, R, n);
        float blockTp = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            blockTp = std::max (blockTp, tpDet.process (L[i], R[i]));
            segOutPeak = std::max (segOutPeak, std::max (std::abs (L[i]), std::abs (R[i])));
            sLR = corrC * sLR + (1.0 - corrC) * (double) L[i] * R[i];
            sLL = corrC * sLL + (1.0 - corrC) * (double) L[i] * L[i];
            sRR = corrC * sRR + (1.0 - corrC) * (double) R[i] * R[i];
        }
        tpNow = gainToDb (blockTp);
        tpMax = std::max (tpMax, tpNow);

        // Monitoring: lautheitsgleicher Bypass / Delta / Mono
        const float mTarget = dbToGain (matchDbSmooth);
        if (s.abBypass || s.delta)
        {
            const float step = (mTarget - curMatch) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                curMatch += step;
                const float dl = dryL[(size_t) i] * curMatch, dr = dryR[(size_t) i] * curMatch;
                if (s.abBypass) { L[i] = dl; R[i] = dr; }
                else            { L[i] -= dl; R[i] -= dr; }
            }
        }
        curMatch = mTarget;
        if (s.monoListen)
            for (int i = 0; i < n; ++i) L[i] = R[i] = 0.5f * (L[i] + R[i]);
    }

    static float absMax (float a, float b) { return std::abs (b) > std::abs (a) ? b : a; }

    static void ramp (float* L, float* R, int n, float& cur, float target) noexcept
    {
        if (n <= 0) return;
        if (std::abs (cur - target) < 1e-6f)
        {
            if (std::abs (cur - 1.0f) < 1e-6f) return;
            for (int i = 0; i < n; ++i) { L[i] *= cur; R[i] *= cur; }
            return;
        }
        const float step = (target - cur) / (float) n;
        for (int i = 0; i < n; ++i) { cur += step; L[i] *= cur; R[i] *= cur; }
        cur = target;
    }

    void pushHistory() noexcept
    {
        const long long c = historyCount.load (std::memory_order_relaxed);
        auto& p = history[(size_t) (c % kHistorySize)];
        p.inPeak = segInPeak; p.outPeak = segOutPeak;
        p.shortTerm = (float) outMeter.shortTerm(); p.momentary = (float) outMeter.momentary();
        p.limGrDb = segLimGr;
        historyCount.store (c + 1, std::memory_order_release);
        // GR-Verteilung nur in lauten Abschnitten sammeln (Stille/Intro verfälscht sonst)
        if (outMeter.momentary() > -40.0)
        {
            limStats.add (-segLimGr); clipStats.add (-segClipGr);
            limGrP95.store ((float) limStats.percentile (0.95));
            clipGrP95.store ((float) clipStats.percentile (0.95));
        }
        segInPeak = segOutPeak = 0.0f; segLimGr = 0.0f; segClipGr = 0.0f;

        for (int k = 0; k < 4; ++k) levelP90[k].store ((float) levels.percentile (k, 0.90));

        // Pegelabgleich für A/B: Short-Term Output minus Short-Term Original (geglättet)
        const double so = outMeter.shortTerm(), sd = dryMeter.shortTerm();
        if (so > -60.0 && sd > -60.0)
            matchDbSmooth += 0.25f * ((float) (so - sd) - matchDbSmooth);
    }

    void publish() noexcept
    {
        momentary.store ((float) outMeter.momentary());
        shortTerm.store ((float) outMeter.shortTerm());
        integrated.store ((float) outMeter.integrated());
        lra.store ((float) outMeter.lra());
        compInIntegrated.store ((float) compInMeter.integrated());
        measuredSeconds.store ((float) outMeter.seconds());
        truePeakMax.store (tpMax);
        truePeakNow.store (tpNow);
        compGr.store (compGrBlock);
        limGr.store (limGrBlock);
        clipGr.store (clipGrBlock);
        trGain.store (trBlock);
        for (int b = 0; b < 3; ++b) mbGr[b].store (mbGrBlock[b]);
        for (int b = 0; b < DynamicEq::kBands; ++b) eqDyn[b].store (eq.dynDb[b]);
        correlation.store ((float) (sLL * sRR > 1e-12 ? sLR / std::sqrt (sLL * sRR) : 1.0));
        matchDb.store (matchDbSmooth);
    }

    double fs = 48000.0;
    Settings settings;
    DynamicEq eq;
    StereoTool stereo;
    TransientShaper trans;
    Compressor comp;
    MultibandCompressor mbComp;
    Oversampler4x os;
    Saturation sat;
    TruePeakLimiter limiter;
    DelayLine dryDelay;
    std::vector<float> dryL, dryR;
    LoudnessMeter outMeter, compInMeter, dryMeter;
    LevelAnalyzer levels;
    double sLR = 0.0, sLL = 0.0, sRR = 0.0;
    float corrC = 0.0f;
    float curMatch = 1.0f, matchDbSmooth = 0.0f;
    float clipGrBlock = 0.0f, trBlock = 0.0f, mbGrBlock[3] {};
    TruePeakDetector tpDet;

    float curIn = 1.0f, curDrive = 1.0f, curOut = 1.0f;
    float compGrBlock = 0.0f, limGrBlock = 0.0f;
    float tpMax = -200.0f, tpNow = -200.0f;
    int stepLen = 4800, histPos = 0;
    float segInPeak = 0.0f, segOutPeak = 0.0f, segLimGr = 0.0f, segClipGr = 0.0f;
    GrStats limStats, clipStats;
    std::atomic<bool> resetRequested { false };
};
} // namespace fin
