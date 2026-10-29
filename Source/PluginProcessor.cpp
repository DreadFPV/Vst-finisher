#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
juce::NormalisableRange<float> freqRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi, 0.1f);
    r.setSkewForCentre (centre);
    return r;
}

juce::String fmtDb (float v, int)   { return juce::String (v, 1) + " dB"; }
juce::String fmtHz (float v, int)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
juce::String fmtMs (float v, int)   { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
juce::String fmtPct (float v, int)  { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String fmtRatio (float v, int){ return juce::String (v, 1) + ":1"; }
juce::String fmtQ (float v, int)    { return "Q " + juce::String (v, 2); }

void addFloat (APVTS::ParameterLayout& l, const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range,
               float def, std::function<juce::String (float, int)> fmt)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def,
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt))));
}

void addBool (APVTS::ParameterLayout& l, const juce::String& id, const juce::String& name, bool def)
{
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}

void addChoice (APVTS::ParameterLayout& l, const juce::String& id, const juce::String& name, juce::StringArray items, int def)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
}

juce::String fmtLu (float v, int)   { return juce::String (v, 1) + " LU"; }
juce::String fmtSigned (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String fmtTone (float v, int) { return v == 0.0f ? juce::String ("neutral") : (v < 0 ? "dunkel " : "hell ") + juce::String (juce::roundToInt (std::abs (v))); }
} // namespace

APVTS::ParameterLayout FinisherAudioProcessor::createLayout()
{
    using namespace ParamIDs;
    APVTS::ParameterLayout l;
    const fin::Settings d;

    addFloat (l, inGain,  "Input",  { -24.0f, 24.0f, 0.1f }, d.inGainDb, fmtDb);
    addFloat (l, outGain, "Output Trim", { -12.0f, 0.0f, 0.1f }, d.outGainDb, fmtDb);

    const juce::StringArray modes { "Stereo", "Mid", "Side" };
    addBool  (l, eqOn,    "EQ On", d.eqOn);
    addBool  (l, hpOn,    "Low-Cut On", d.hpOn);
    addFloat (l, hpFreq,  "Low-Cut Freq", freqRange (10.0f, 300.0f, 40.0f), d.hpFreq, fmtHz);
    addChoice (l, hpSlope, "Low-Cut Slope", { "12 dB", "24 dB", "48 dB" }, d.hpSlope);

    struct BandDef { const char *f, *g, *q, *m, *t, *r, *name; float lo, hi, centre; const fin::EqBand* def; };
    const BandDef bands[] = {
        { lsFreq, lsGain, nullptr, lsMode, lsThr, lsRng, "Low-Shelf", 20.0f, 500.0f, 100.0f, &d.ls },
        { p1Freq, p1Gain, p1Q,     p1Mode, p1Thr, p1Rng, "Bell 1",    30.0f, 2000.0f, 250.0f, &d.p1 },
        { p2Freq, p2Gain, p2Q,     p2Mode, p2Thr, p2Rng, "Bell 2",    500.0f, 16000.0f, 3000.0f, &d.p2 },
        { hsFreq, hsGain, nullptr, hsMode, hsThr, hsRng, "High-Shelf", 1000.0f, 20000.0f, 8000.0f, &d.hs } };
    for (const auto& b : bands)
    {
        const juce::String n (b.name);
        addFloat (l, b.f, n + " Freq", freqRange (b.lo, b.hi, b.centre), b.def->freq, fmtHz);
        addFloat (l, b.g, n + " Gain", { -12.0f, 12.0f, 0.1f }, b.def->gain, fmtDb);
        if (b.q) addFloat (l, b.q, n + " Q", freqRange (0.3f, 8.0f, 1.0f), b.def->q, fmtQ);
        addChoice (l, b.m, n + " Channel", modes, b.def->mode);
        addFloat (l, b.t, n + " Dyn Threshold", { -60.0f, 0.0f, 0.1f }, b.def->thresh, fmtDb);
        addFloat (l, b.r, n + " Dyn Range", { -12.0f, 12.0f, 0.1f }, b.def->range, fmtSigned);
    }
    addFloat (l, eqAtk, "EQ Dyn Attack", freqRange (0.5f, 100.0f, 10.0f), d.eqAttack, fmtMs);
    addFloat (l, eqRel, "EQ Dyn Release", freqRange (10.0f, 1000.0f, 120.0f), d.eqRelease, fmtMs);

    addBool  (l, monoOn,   "Mono Bass On", d.monoOn);
    addFloat (l, monoFreq, "Mono Bass Freq", freqRange (40.0f, 300.0f, 120.0f), d.monoFreq, fmtHz);
    addFloat (l, width,    "Width Mid", { 0.0f, 200.0f, 1.0f }, d.width, fmtPct);
    addFloat (l, widthHi,  "Width High", { 0.0f, 200.0f, 1.0f }, d.widthHigh, fmtPct);
    addFloat (l, widthXo,  "Width Crossover", freqRange (1000.0f, 12000.0f, 4000.0f), d.widthXover, fmtHz);

    addBool  (l, trOn,  "Transient On", d.trOn);
    addFloat (l, trAtk, "Transient Attack", { -12.0f, 12.0f, 0.1f }, d.trAttack, fmtSigned);
    addFloat (l, trSus, "Transient Sustain", { -12.0f, 12.0f, 0.1f }, d.trSustain, fmtSigned);

    addBool  (l, compOn,  "Comp On", d.compOn);
    addFloat (l, thresh,  "Threshold", { -40.0f, 0.0f, 0.1f }, d.thresh, fmtDb);
    addFloat (l, ratio,   "Ratio", freqRange (1.0f, 10.0f, 3.0f), d.ratio, fmtRatio);
    addFloat (l, attack,  "Attack", freqRange (0.1f, 100.0f, 15.0f), d.attack, fmtMs);
    addFloat (l, release, "Release", freqRange (10.0f, 1000.0f, 150.0f), d.release, fmtMs);
    addFloat (l, knee,    "Knee", { 0.0f, 12.0f, 0.1f }, d.knee, fmtDb);
    addFloat (l, makeup,  "Makeup", { -6.0f, 18.0f, 0.1f }, d.makeup, fmtDb);
    addFloat (l, mix,     "Comp Mix", { 0.0f, 100.0f, 1.0f }, d.mix, fmtPct);

    addBool  (l, mbOn, "Multiband On", d.mbOn);
    addFloat (l, mbX1, "MB Crossover Low", freqRange (40.0f, 800.0f, 150.0f), d.mbX1, fmtHz);
    addFloat (l, mbX2, "MB Crossover High", freqRange (800.0f, 12000.0f, 3000.0f), d.mbX2, fmtHz);
    const char* bandNames[] = { "Low", "Mid", "High" };
    for (int b = 0; b < 3; ++b)
    {
        const juce::String n = juce::String ("MB ") + bandNames[b] + " ";
        const auto& m = d.mb[b];
        addFloat (l, mbParam (b, "Thr"),   n + "Threshold", { -40.0f, 0.0f, 0.1f }, m.thresh, fmtDb);
        addFloat (l, mbParam (b, "Ratio"), n + "Ratio", freqRange (1.0f, 10.0f, 3.0f), m.ratio, fmtRatio);
        addFloat (l, mbParam (b, "Atk"),   n + "Attack", freqRange (0.1f, 100.0f, 15.0f), m.attack, fmtMs);
        addFloat (l, mbParam (b, "Rel"),   n + "Release", freqRange (10.0f, 1000.0f, 150.0f), m.release, fmtMs);
        addFloat (l, mbParam (b, "Gain"),  n + "Gain", { -12.0f, 12.0f, 0.1f }, m.gain, fmtSigned);
        addBool  (l, mbParam (b, "Solo"),  n + "Solo", false);
        addBool  (l, mbParam (b, "Byp"),   n + "Bypass", false);
    }

    addBool  (l, satOn,    "Saturation On", d.satOn);
    addChoice (l, satModel, "Saturation Model", { "Tape", "Tube", "Soft" }, d.satModel);
    addFloat (l, satDrive, "Saturation Drive", { 0.0f, 24.0f, 0.1f }, d.satDrive, fmtDb);
    addFloat (l, satTone,  "Saturation Tone", { -100.0f, 100.0f, 1.0f }, d.satTone, fmtTone);
    addFloat (l, satMix,   "Saturation Mix", { 0.0f, 100.0f, 1.0f }, d.satMix, fmtPct);
    addBool  (l, satAuto,  "Saturation Auto-Gain", d.satAuto);

    addBool  (l, clipOn,   "Clipper On", d.clipOn);
    addFloat (l, clipThr,  "Clipper Threshold", { -3.0f, 6.0f, 0.1f }, d.clipThresh, fmtSigned);
    addFloat (l, clipSoft, "Clipper Softness", { 0.0f, 100.0f, 1.0f }, d.clipSoft, fmtPct);

    addBool  (l, limOn,   "Limiter On", d.limOn);
    addFloat (l, drive,   "Drive", { 0.0f, 24.0f, 0.1f }, d.drive, fmtDb);
    addFloat (l, ceiling, "Ceiling", { -6.0f, 0.0f, 0.1f }, d.ceiling, [] (float v, int) { return juce::String (v, 1) + " dBTP"; });
    addFloat (l, limRel,  "Limiter Release", freqRange (1.0f, 500.0f, 60.0f), d.limRelease, fmtMs);

    addBool (l, abBypass, "A/B Bypass (Level-Matched)", false);
    addBool (l, delta,    "Delta", false);
    addBool (l, monoLis,  "Mono Listen", false);

    const fin::Targets t;
    addFloat (l, tgtLufs, "Target LUFS", { -20.0f, -5.0f, 0.1f }, t.lufs, [] (float v, int) { return juce::String (v, 1) + " LUFS"; });
    addFloat (l, tgtTp,   "Target True Peak", { -6.0f, 0.0f, 0.1f }, t.truePeak, [] (float v, int) { return juce::String (v, 1) + " dBTP"; });
    addFloat (l, tgtLra,  "Target LRA", { 2.0f, 20.0f, 0.1f }, t.lra, fmtLu);
    return l;
}

//==============================================================================
FinisherAudioProcessor::FinisherAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FinisherState", createLayout())
{
}

bool FinisherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FinisherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latency());
    monoScratch.setSize (1, samplesPerBlock);
}

void FinisherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh == 0 || n == 0) return;

    engine.setSettings (readSettings());

    float* L = buffer.getWritePointer (0);
    float* R = nullptr;
    if (numCh > 1)
    {
        R = buffer.getWritePointer (1);
    }
    else
    {
        if (monoScratch.getNumSamples() < n) monoScratch.setSize (1, n, false, false, true);
        R = monoScratch.getWritePointer (0);
        juce::FloatVectorOperations::copy (R, L, n);
    }

    engine.process (L, R, n);

    if (numCh == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
fin::Settings FinisherAudioProcessor::readSettings() const
{
    using namespace ParamIDs;
    fin::Settings s;
    s.inGainDb = get (inGain);
    s.outGainDb = get (outGain);

    s.eqOn = on (eqOn); s.hpOn = on (hpOn); s.hpFreq = get (hpFreq); s.hpSlope = choice (hpSlope);
    auto band = [this] (fin::EqBand& b, const char* f, const char* g, const char* q, const char* m, const char* t, const char* r)
    {
        b.freq = get (f); b.gain = get (g); if (q) b.q = get (q);
        b.mode = choice (m); b.thresh = get (t); b.range = get (r);
    };
    band (s.ls, lsFreq, lsGain, nullptr, lsMode, lsThr, lsRng);
    band (s.p1, p1Freq, p1Gain, p1Q, p1Mode, p1Thr, p1Rng);
    band (s.p2, p2Freq, p2Gain, p2Q, p2Mode, p2Thr, p2Rng);
    band (s.hs, hsFreq, hsGain, nullptr, hsMode, hsThr, hsRng);
    s.eqAttack = get (eqAtk); s.eqRelease = get (eqRel);

    s.monoOn = on (monoOn); s.monoFreq = get (monoFreq); s.width = get (width);
    s.widthHigh = get (widthHi); s.widthXover = get (widthXo);

    s.trOn = on (trOn); s.trAttack = get (trAtk); s.trSustain = get (trSus);

    s.compOn = on (compOn); s.thresh = get (thresh); s.ratio = get (ratio); s.attack = get (attack);
    s.release = get (release); s.knee = get (knee); s.makeup = get (makeup); s.mix = get (mix);

    s.mbOn = on (mbOn); s.mbX1 = get (mbX1); s.mbX2 = get (mbX2);
    for (int b = 0; b < 3; ++b)
    {
        auto& m = s.mb[b];
        m.thresh = get (mbParam (b, "Thr")); m.ratio = get (mbParam (b, "Ratio"));
        m.attack = get (mbParam (b, "Atk")); m.release = get (mbParam (b, "Rel"));
        m.gain = get (mbParam (b, "Gain")); m.solo = on (mbParam (b, "Solo")); m.bypass = on (mbParam (b, "Byp"));
    }

    s.satOn = on (satOn); s.satModel = choice (satModel); s.satDrive = get (satDrive);
    s.satTone = get (satTone); s.satMix = get (satMix); s.satAuto = on (satAuto);

    s.clipOn = on (clipOn); s.clipThresh = get (clipThr); s.clipSoft = get (clipSoft);

    s.limOn = on (limOn); s.drive = get (drive); s.ceiling = get (ceiling); s.limRelease = get (limRel);

    s.abBypass = on (abBypass); s.delta = on (delta); s.monoListen = on (monoLis);
    return s;
}

fin::Targets FinisherAudioProcessor::readTargets() const
{
    fin::Targets t;
    t.lufs = get (ParamIDs::tgtLufs);
    t.truePeak = get (ParamIDs::tgtTp);
    t.lra = get (ParamIDs::tgtLra);
    return t;
}

void FinisherAudioProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

juce::String FinisherAudioProcessor::applyTargets()
{
    const auto cur = readSettings();
    const auto res = fin::Engine::computeTargets (cur, engine.measurement(), readTargets());
    if (! res.ok)
        return res.message;

    using namespace ParamIDs;
    const auto& s = res.settings;
    setParam (inGain, s.inGainDb);
    setParam (compOn, s.compOn ? 1.0f : 0.0f);
    setParam (thresh, s.thresh);
    setParam (ratio, s.ratio);
    setParam (attack, s.attack);
    setParam (release, s.release);
    setParam (knee, s.knee);
    setParam (makeup, s.makeup);
    setParam (mix, s.mix);
    setParam (limOn, s.limOn ? 1.0f : 0.0f);
    setParam (drive, s.drive);
    setParam (ceiling, s.ceiling);

    engine.requestMeterReset();
    return res.message;
}

//==============================================================================
int FinisherAudioProcessor::numFactoryPresets() { return (int) presets::factory().size(); }

juce::String FinisherAudioProcessor::factoryPresetName (int index)
{
    const auto& f = presets::factory();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String::fromUTF8 (f[(size_t) index].name) : juce::String();
}

bool FinisherAudioProcessor::isMonitoringParam (const juce::String& id)
{
    return id == ParamIDs::abBypass || id == ParamIDs::delta || id == ParamIDs::monoLis;
}

void FinisherAudioProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isMonitoringParam (rp->getParameterID()))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
}

int FinisherAudioProcessor::loadFactoryPreset (int index)
{
    const auto& f = presets::factory();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return -1;
    resetToDefaults();
    int unknown = 0;
    for (const auto& [id, value] : f[(size_t) index].values)
    {
        if (apvts.getParameter (id) == nullptr) { ++unknown; jassertfalse; continue; }
        setParam (id, value);
    }
    setPresetName (factoryPresetName (index));
    engine.requestMeterReset();
    return unknown;
}

juce::File FinisherAudioProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("JustCreateit").getChildFile ("Finisher").getChildFile ("Presets");
}

juce::Array<juce::File> FinisherAudioProcessor::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.fpreset");
    files.sort();
    return files;
}

bool FinisherAudioProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;

    setPresetName (name.trim());
    auto state = apvts.copyState();
    for (int i = state.getNumChildren(); --i >= 0;)        // Monitoring nicht mitspeichern
        if (isMonitoringParam (state.getChild (i).getProperty ("id").toString()))
            state.removeChild (i, nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (folder.getChildFile (clean + ".fpreset"));
    return false;
}

bool FinisherAudioProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    const auto tree = juce::ValueTree::fromXml (*xml);
    resetToDefaults();
    for (const auto& child : tree)
    {
        const auto id = child.getProperty ("id").toString();
        if (id.isEmpty() || isMonitoringParam (id)) continue;
        if (apvts.getParameter (id) != nullptr)
            setParam (id, (float) child.getProperty ("value"));
    }
    setPresetName (tree.getProperty ("presetName", file.getFileNameWithoutExtension()).toString());
    engine.requestMeterReset();
    return true;
}

//==============================================================================
void FinisherAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FinisherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FinisherAudioProcessor::createEditor()
{
    return new FinisherAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FinisherAudioProcessor();
}
