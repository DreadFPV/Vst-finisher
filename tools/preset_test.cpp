// Preset-Test: alle Werks-Presets laden, Track-Ausschnitt durchrechnen, Ziel-Automatik in Durchgängen,
// prüfen ob LUFS/True Peak erreicht werden. Dazu Roundtrip eigener Presets.
//   finisher-presettest <in.wav> [start_s] [dauer_s]

#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"

struct Diag { float comp = 0, mb[3] {}, clip = 0, lim = 0; double grSum[2] {}; long n = 0; };
static Diag diag;

static juce::AudioBuffer<float>* capture = nullptr;   // optional: Ausgabe des letzten Durchgangs mitschneiden

static float pass (FinisherAudioProcessor& proc, juce::AudioFormatReader& reader, juce::int64 start, juce::int64 len)
{
    diag = {};
    proc.resetMeters();
    if (capture) capture->setSize (2, (int) len, false, true);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (juce::int64 pos = 0; pos < len; pos += 512)
    {
        reader.read (&buf, 0, 512, start + pos, true, true);
        proc.processBlock (buf, midi);
        if (capture)
            for (int c = 0; c < 2; ++c)
                capture->copyFrom (c, (int) pos, buf, c, 0, (int) juce::jmin<juce::int64> (512, len - pos));
        auto& e = proc.engine;
        diag.comp = std::min (diag.comp, e.compGr.load()); diag.clip = std::min (diag.clip, e.clipGr.load());
        diag.lim = std::min (diag.lim, e.limGr.load());
        for (int b = 0; b < 3; ++b) diag.mb[b] = std::min (diag.mb[b], e.mbGr[b].load());
        diag.grSum[0] += e.compGr.load(); diag.grSum[1] += e.mbGr[0].load() + e.mbGr[1].load() + e.mbGr[2].load(); ++diag.n;
    }
    return proc.engine.integrated.load();
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: presettest in.wav [start_s] [dauer_s]\n"; return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioFormatManager afm;
    afm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (afm.createReaderFor (juce::File (argv[1])));
    if (! reader) return 1;
    const double fs = reader->sampleRate;
    const auto start = (juce::int64) (fs * (argc > 2 ? std::atof (argv[2]) : 60.0));
    const auto len   = (juce::int64) (fs * (argc > 3 ? std::atof (argv[3]) : 120.0));

    int failures = 0;
    FinisherAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, fs, 512);
    proc.prepareToPlay (fs, 512);

    std::printf ("%-34s %8s %8s %6s %8s  %s\n", "Preset", "Ziel", "I", "LRA", "TP", "Status");
    const int only = std::getenv ("FINISHER_ONLY") ? std::atoi (std::getenv ("FINISHER_ONLY")) : -1;
    for (int i = 0; i < FinisherAudioProcessor::numFactoryPresets(); ++i)
    {
        if (only >= 0 && i != only) continue;
        const int unknown = proc.loadFactoryPreset (i);
        if (auto* off = std::getenv ("FINISHER_OFF"))   // z. B. FINISHER_OFF=satOn,mbOn
            for (auto& id : juce::StringArray::fromTokens (off, ",", ""))
                if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (0.0f);
        if (unknown != 0) { std::printf ("FAIL: %d unbekannte Parameter in Preset %d\n", unknown, i); ++failures; }
        const auto t = proc.readTargets();
        const bool measureOnly = proc.factoryPresetName (i).startsWith ("Nur Messen") || i == 0;

        pass (proc, *reader, start, len);
        if (! measureOnly)
            for (int k = 0; k < 3; ++k)
            {
                proc.applyTargets();
                pass (proc, *reader, start, len);
            }
        if (auto* dir = std::getenv ("FINISHER_WRITE"))
        {
            juce::AudioBuffer<float> out;
            capture = &out;
            pass (proc, *reader, start, len);
            capture = nullptr;
            juce::File f (juce::String (dir) + "/preset_" + juce::String (i) + juce::String (std::getenv ("FINISHER_TAG") ? std::getenv ("FINISHER_TAG") : "") + ".wav");
            f.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), fs, 2, 32, {}, 0));
            if (w) w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        }
        const float I = proc.engine.integrated.load(), lra = proc.engine.lra.load(), tp = proc.engine.truePeakMax.load();
        const auto s = proc.readSettings();
        bool ok = true;
        if (! measureOnly && std::abs (I - t.lufs) > 0.6f) ok = false;
        if (s.limOn && tp > t.truePeak + 0.05f) ok = false;
        if (! ok) ++failures;
        if (std::getenv ("FINISHER_DIAG"))
            std::printf ("   GR max: Comp %.1f | MB %.1f/%.1f/%.1f | Clip %.1f | Lim %.1f   GR avg: Comp %.1f, MB-Summe %.1f\n",
                         diag.comp, diag.mb[0], diag.mb[1], diag.mb[2], diag.clip, diag.lim, diag.grSum[0] / std::max (1L, diag.n), diag.grSum[1] / std::max (1L, diag.n));
        std::printf ("%-34s %8.1f %8.1f %6.1f %8.2f  %s (Drive %.1f dB)\n", proc.factoryPresetName (i).toRawUTF8(),
                     measureOnly ? 0.0f : t.lufs, I, lra, tp, ok ? "OK" : "FAIL", s.drive);
    }

    // Eigene Presets: speichern -> ändern -> laden; Monitoring bleibt unberührt
    {
        auto setP = [&proc] (const juce::String& id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        auto getP = [&proc] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
        proc.loadFactoryPreset (4);
        setP (ParamIDs::satDrive, 7.5f);
        setP (ParamIDs::abBypass, 1.0f);
        const bool saved = proc.saveUserPreset ("Selbsttest Preset");
        setP (ParamIDs::satDrive, 1.0f);
        setP (ParamIDs::abBypass, 0.0f);
        const auto file = FinisherAudioProcessor::userPresetFolder().getChildFile ("Selbsttest Preset.fpreset");
        const bool loaded = proc.loadUserPreset (file);
        const bool ok = saved && loaded && std::abs (getP (ParamIDs::satDrive) - 7.5f) < 0.01f && getP (ParamIDs::abBypass) < 0.5f
                        && proc.currentPresetName() == "Selbsttest Preset" && proc.userPresets().contains (file);
        std::printf ("\nEigenes Preset speichern/laden: %s (%s)\n", ok ? "OK" : "FAIL", file.getFullPathName().toRawUTF8());
        if (! ok) ++failures;
        file.deleteFile();
    }

    // State des Hosts behält den Preset-Namen
    {
        proc.loadFactoryPreset (1);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        FinisherAudioProcessor p2;
        p2.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = p2.currentPresetName() == proc.currentPresetName();
        std::printf ("Preset-Name im Projekt gespeichert: %s (%s)\n", ok ? "OK" : "FAIL", p2.currentPresetName().toRawUTF8());
        if (! ok) ++failures;
    }

    std::printf ("\n%s (%d Fehler)\n", failures == 0 ? "ALLE PRESET-TESTS OK" : "PRESET-TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
