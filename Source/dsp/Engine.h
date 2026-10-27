#pragma once

#include <atomic>
#include <vector>
#include <string>
#include "Settings.h"
#include "Modules.h"
#include "LoudnessMeter.h"

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
    double seconds = 0.0;
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
        eq.prepare (fs); stereo.prepare (fs); comp.prepare (fs); limiter.prepare (fs);
        outMeter.prepare (fs); compInMeter.prepare (fs);
        stepLen = std::max (1, (int) std::lround (fs * 0.1));
        resetMeters();
        curIn = dbToGain (settings.inGainDb); curDrive = dbToGain (settings.drive); curOut = dbToGain (settings.outGainDb);
    }

    void resetDsp() { eq.reset(); stereo.reset(); comp.reset(); limiter.reset(); }
    int latency() const { return limiter.latency(); }
    double sampleRate() const { return fs; }

    void setSettings (const Settings& s) { settings = s; eq.update (s); }
    const Settings& getSettings() const { return settings; }

    /** Thread-sicher: Messung zurücksetzen (wird im nächsten Block ausgeführt). */
    void requestMeterReset() { resetRequested.store (true); }

    void process (float* L, float* R, int n) noexcept
    {
        if (resetRequested.exchange (false))
            resetMeters();
        compGrBlock = 0.0f; limGrBlock = 0.0f;

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

    /** Verlauf (Single-Producer). historyCount = Gesamtanzahl geschriebener Punkte. */
    std::vector<HistoryPoint> history;
    std::atomic<long long> historyCount { 0 };

    Measurement measurement() const
    {
        Measurement m;
        m.integrated = integrated.load(); m.lra = lra.load(); m.truePeak = truePeakMax.load();
        m.compInIntegrated = compInIntegrated.load(); m.seconds = measuredSeconds.load();
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

        // 2) LRA -> Kompressor
        const double excess = m.lra - t.lra;
        if (excess > 0.5)
        {
            if (! cur.compOn)
            {
                s.compOn = true;
                s.attack = 30.0f; s.release = 200.0f; s.knee = 6.0f; s.mix = 100.0f; s.makeup = 0.0f;
                s.thresh = (float) std::clamp (m.compInIntegrated + 4.0 - excess, -40.0, 0.0);
                s.ratio  = (float) std::clamp (1.5 + excess * 0.4, 1.5, 6.0);
            }
            else
            {
                s.thresh = (float) std::clamp ((double) cur.thresh - excess * 0.75, -40.0, 0.0);
                s.ratio  = (float) std::clamp ((double) cur.ratio * (1.0 + excess * 0.1), 1.0, 10.0);
            }
            msg += "LRA zu hoch → mehr Kompression. ";
        }
        else if (excess < -1.0 && cur.compOn)
        {
            s.thresh = (float) std::clamp ((double) cur.thresh - excess * 0.75, -40.0, 0.0);
            s.ratio  = (float) std::clamp ((double) cur.ratio * (1.0 + excess * 0.1), 1.0, 10.0);
            msg += "LRA zu niedrig → weniger Kompression. ";
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
        if (s.drive > 12.0f)
            msg += "Achtung: viel Drive (" + std::to_string ((int) s.drive) + " dB) – Limiter arbeitet hart. ";

        msg += "Angewendet. Track nochmal komplett abspielen und prüfen (2–3 Durchgänge).";
        r.message = msg;
        r.ok = true;
        return r;
    }

private:
    void resetMeters()
    {
        outMeter.reset(); compInMeter.reset();
        histPos = 0; segInPeak = segOutPeak = 0.0f; segLimGr = 0.0f;
        tpMax = -200.0f; tpNow = -200.0f;
        tpDet.reset();
        historyCount.store (0);
        momentary = shortTerm = integrated = compInIntegrated = -200.0f;
        lra = 0.0f; truePeakMax = -200.0f; measuredSeconds = 0.0f;
    }

    void processSegment (float* L, float* R, int n) noexcept
    {
        const Settings& s = settings;

        // Input-Gain (Rampe)
        ramp (L, R, n, curIn, dbToGain (s.inGainDb));
        for (int i = 0; i < n; ++i) segInPeak = std::max (segInPeak, std::max (std::abs (L[i]), std::abs (R[i])));

        if (s.eqOn) eq.process (L, R, n);
        stereo.process (L, R, n, s.monoOn, s.monoFreq, s.width);

        compInMeter.process (L, R, n);
        compGrBlock = std::min (compGrBlock, s.compOn ? comp.process (L, R, n, s) : 0.0f);

        ramp (L, R, n, curDrive, s.limOn ? dbToGain (s.drive) : 1.0f);
        const float lg = limiter.process (L, R, n, s.limOn, s.ceiling, s.limRelease);
        segLimGr = std::min (segLimGr, lg);
        limGrBlock = std::min (limGrBlock, lg);

        ramp (L, R, n, curOut, dbToGain (std::min (0.0f, s.outGainDb)));

        outMeter.process (L, R, n);
        float blockTp = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            blockTp = std::max (blockTp, tpDet.process (L[i], R[i]));
            segOutPeak = std::max (segOutPeak, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        tpNow = gainToDb (blockTp);
        tpMax = std::max (tpMax, tpNow);
    }

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
        segInPeak = segOutPeak = 0.0f; segLimGr = 0.0f;
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
    }

    double fs = 48000.0;
    Settings settings;
    Equalizer eq;
    StereoTool stereo;
    Compressor comp;
    TruePeakLimiter limiter;
    LoudnessMeter outMeter, compInMeter;
    TruePeakDetector tpDet;

    float curIn = 1.0f, curDrive = 1.0f, curOut = 1.0f;
    float compGrBlock = 0.0f, limGrBlock = 0.0f;
    float tpMax = -200.0f, tpNow = -200.0f;
    int stepLen = 4800, histPos = 0;
    float segInPeak = 0.0f, segOutPeak = 0.0f, segLimGr = 0.0f;
    std::atomic<bool> resetRequested { false };
};
} // namespace fin
