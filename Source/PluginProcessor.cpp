#include "PluginProcessor.h"
#include "PluginEditor.h"

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

void addFloat (APVTS::ParameterLayout& l, const char* id, const char* name, juce::NormalisableRange<float> range,
               float def, std::function<juce::String (float, int)> fmt)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def,
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt))));
}

void addBool (APVTS::ParameterLayout& l, const char* id, const char* name, bool def)
{
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}
} // namespace

APVTS::ParameterLayout FinisherAudioProcessor::createLayout()
{
    using namespace ParamIDs;
    APVTS::ParameterLayout l;
    const fin::Settings d;

    addFloat (l, inGain,  "Input",  { -24.0f, 24.0f, 0.1f }, d.inGainDb, fmtDb);
    addFloat (l, outGain, "Output Trim", { -12.0f, 0.0f, 0.1f }, d.outGainDb, fmtDb);

    addBool  (l, eqOn,   "EQ On", d.eqOn);
    addBool  (l, hpOn,   "Low-Cut On", d.hpOn);
    addFloat (l, hpFreq, "Low-Cut Freq", freqRange (10.0f, 300.0f, 40.0f), d.hpFreq, fmtHz);
    addFloat (l, lsFreq, "Low-Shelf Freq", freqRange (20.0f, 500.0f, 100.0f), d.lsFreq, fmtHz);
    addFloat (l, lsGain, "Low-Shelf Gain", { -12.0f, 12.0f, 0.1f }, d.lsGain, fmtDb);
    addFloat (l, p1Freq, "Bell 1 Freq", freqRange (30.0f, 2000.0f, 250.0f), d.p1Freq, fmtHz);
    addFloat (l, p1Gain, "Bell 1 Gain", { -12.0f, 12.0f, 0.1f }, d.p1Gain, fmtDb);
    addFloat (l, p1Q,    "Bell 1 Q", freqRange (0.3f, 8.0f, 1.0f), d.p1Q, fmtQ);
    addFloat (l, p2Freq, "Bell 2 Freq", freqRange (500.0f, 16000.0f, 3000.0f), d.p2Freq, fmtHz);
    addFloat (l, p2Gain, "Bell 2 Gain", { -12.0f, 12.0f, 0.1f }, d.p2Gain, fmtDb);
    addFloat (l, p2Q,    "Bell 2 Q", freqRange (0.3f, 8.0f, 1.0f), d.p2Q, fmtQ);
    addFloat (l, hsFreq, "High-Shelf Freq", freqRange (1000.0f, 20000.0f, 8000.0f), d.hsFreq, fmtHz);
    addFloat (l, hsGain, "High-Shelf Gain", { -12.0f, 12.0f, 0.1f }, d.hsGain, fmtDb);

    addBool  (l, monoOn,   "Mono Bass On", d.monoOn);
    addFloat (l, monoFreq, "Mono Bass Freq", freqRange (40.0f, 300.0f, 120.0f), d.monoFreq, fmtHz);
    addFloat (l, width,    "Width", { 0.0f, 200.0f, 1.0f }, d.width, fmtPct);

    addBool  (l, compOn,  "Comp On", d.compOn);
    addFloat (l, thresh,  "Threshold", { -40.0f, 0.0f, 0.1f }, d.thresh, fmtDb);
    addFloat (l, ratio,   "Ratio", freqRange (1.0f, 10.0f, 3.0f), d.ratio, fmtRatio);
    addFloat (l, attack,  "Attack", freqRange (0.1f, 100.0f, 15.0f), d.attack, fmtMs);
    addFloat (l, release, "Release", freqRange (10.0f, 1000.0f, 150.0f), d.release, fmtMs);
    addFloat (l, knee,    "Knee", { 0.0f, 12.0f, 0.1f }, d.knee, fmtDb);
    addFloat (l, makeup,  "Makeup", { -6.0f, 18.0f, 0.1f }, d.makeup, fmtDb);
    addFloat (l, mix,     "Comp Mix", { 0.0f, 100.0f, 1.0f }, d.mix, fmtPct);

    addBool  (l, limOn,   "Limiter On", d.limOn);
    addFloat (l, drive,   "Drive", { 0.0f, 24.0f, 0.1f }, d.drive, fmtDb);
    addFloat (l, ceiling, "Ceiling", { -6.0f, 0.0f, 0.1f }, d.ceiling, [] (float v, int) { return juce::String (v, 1) + " dBTP"; });
    addFloat (l, limRel,  "Limiter Release", freqRange (1.0f, 500.0f, 60.0f), d.limRelease, fmtMs);

    const fin::Targets t;
    addFloat (l, tgtLufs, "Target LUFS", { -20.0f, -5.0f, 0.1f }, t.lufs, [] (float v, int) { return juce::String (v, 1) + " LUFS"; });
    addFloat (l, tgtTp,   "Target True Peak", { -6.0f, 0.0f, 0.1f }, t.truePeak, [] (float v, int) { return juce::String (v, 1) + " dBTP"; });
    addFloat (l, tgtLra,  "Target LRA", { 2.0f, 20.0f, 0.1f }, t.lra, [] (float v, int) { return juce::String (v, 1) + " LU"; });
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
    s.eqOn = on (eqOn); s.hpOn = on (hpOn); s.hpFreq = get (hpFreq);
    s.lsFreq = get (lsFreq); s.lsGain = get (lsGain);
    s.p1Freq = get (p1Freq); s.p1Gain = get (p1Gain); s.p1Q = get (p1Q);
    s.p2Freq = get (p2Freq); s.p2Gain = get (p2Gain); s.p2Q = get (p2Q);
    s.hsFreq = get (hsFreq); s.hsGain = get (hsGain);
    s.monoOn = on (monoOn); s.monoFreq = get (monoFreq); s.width = get (width);
    s.compOn = on (compOn); s.thresh = get (thresh); s.ratio = get (ratio); s.attack = get (attack);
    s.release = get (release); s.knee = get (knee); s.makeup = get (makeup); s.mix = get (mix);
    s.limOn = on (limOn); s.drive = get (drive); s.ceiling = get (ceiling); s.limRelease = get (limRel);
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

void FinisherAudioProcessor::setParam (const char* id, float value)
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
