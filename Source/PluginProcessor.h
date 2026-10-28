#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"

namespace ParamIDs
{
    // Input / Output
    inline constexpr const char* inGain   = "inGain";
    inline constexpr const char* outGain  = "outGain";
    // Dyn-EQ
    inline constexpr const char* eqOn     = "eqOn";
    inline constexpr const char* hpOn     = "hpOn";
    inline constexpr const char* hpFreq   = "hpFreq";
    inline constexpr const char* hpSlope  = "hpSlope";
    inline constexpr const char* lsFreq   = "lsFreq";
    inline constexpr const char* lsGain   = "lsGain";
    inline constexpr const char* lsMode   = "lsMode";
    inline constexpr const char* lsThr    = "lsThr";
    inline constexpr const char* lsRng    = "lsRng";
    inline constexpr const char* p1Freq   = "p1Freq";
    inline constexpr const char* p1Gain   = "p1Gain";
    inline constexpr const char* p1Q      = "p1Q";
    inline constexpr const char* p1Mode   = "p1Mode";
    inline constexpr const char* p1Thr    = "p1Thr";
    inline constexpr const char* p1Rng    = "p1Rng";
    inline constexpr const char* p2Freq   = "p2Freq";
    inline constexpr const char* p2Gain   = "p2Gain";
    inline constexpr const char* p2Q      = "p2Q";
    inline constexpr const char* p2Mode   = "p2Mode";
    inline constexpr const char* p2Thr    = "p2Thr";
    inline constexpr const char* p2Rng    = "p2Rng";
    inline constexpr const char* hsFreq   = "hsFreq";
    inline constexpr const char* hsGain   = "hsGain";
    inline constexpr const char* hsMode   = "hsMode";
    inline constexpr const char* hsThr    = "hsThr";
    inline constexpr const char* hsRng    = "hsRng";
    inline constexpr const char* eqAtk    = "eqAtk";
    inline constexpr const char* eqRel    = "eqRel";
    // Stereo
    inline constexpr const char* monoOn   = "monoOn";
    inline constexpr const char* monoFreq = "monoFreq";
    inline constexpr const char* width    = "width";
    inline constexpr const char* widthHi  = "widthHi";
    inline constexpr const char* widthXo  = "widthXo";
    // Transient
    inline constexpr const char* trOn     = "trOn";
    inline constexpr const char* trAtk    = "trAtk";
    inline constexpr const char* trSus    = "trSus";
    // Kompressor
    inline constexpr const char* compOn   = "compOn";
    inline constexpr const char* thresh   = "thresh";
    inline constexpr const char* ratio    = "ratio";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* release  = "release";
    inline constexpr const char* knee     = "knee";
    inline constexpr const char* makeup   = "makeup";
    inline constexpr const char* mix      = "mix";
    // Multiband
    inline constexpr const char* mbOn     = "mbOn";
    inline constexpr const char* mbX1     = "mbX1";
    inline constexpr const char* mbX2     = "mbX2";
    /** mbParam(1, "Thr") -> "mb1Thr" usw. (Thr, Ratio, Atk, Rel, Gain, Solo, Byp) */
    inline juce::String mbParam (int band, const char* name) { return "mb" + juce::String (band + 1) + name; }
    // Saturation
    inline constexpr const char* satOn    = "satOn";
    inline constexpr const char* satModel = "satModel";
    inline constexpr const char* satDrive = "satDrive";
    inline constexpr const char* satTone  = "satTone";
    inline constexpr const char* satMix   = "satMix";
    inline constexpr const char* satAuto  = "satAuto";
    // Clipper
    inline constexpr const char* clipOn   = "clipOn";
    inline constexpr const char* clipThr  = "clipThr";
    inline constexpr const char* clipSoft = "clipSoft";
    // Limiter
    inline constexpr const char* limOn    = "limOn";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* ceiling  = "ceiling";
    inline constexpr const char* limRel   = "limRel";
    // Monitoring
    inline constexpr const char* abBypass = "abBypass";
    inline constexpr const char* delta    = "delta";
    inline constexpr const char* monoLis  = "monoListen";
    // Ziele
    inline constexpr const char* tgtLufs  = "tgtLufs";
    inline constexpr const char* tgtTp    = "tgtTp";
    inline constexpr const char* tgtLra   = "tgtLra";
}

class FinisherAudioProcessor : public juce::AudioProcessor
{
public:
    FinisherAudioProcessor();
    ~FinisherAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    fin::Settings readSettings() const;
    fin::Targets readTargets() const;

    /** Ziel-Automatik ausführen (Message-Thread). Gibt den Hinweistext zurück. */
    juce::String applyTargets();
    void resetMeters() { engine.requestMeterReset(); }

    juce::AudioProcessorValueTreeState apvts;
    fin::Engine engine;
    std::atomic<double> currentSampleRate { 48000.0 };

private:
    void setParam (const juce::String& id, float value);
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const juce::String& id) const { return get (id) > 0.5f; }
    int choice (const juce::String& id) const { return juce::roundToInt (get (id)); }

    juce::AudioBuffer<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinisherAudioProcessor)
};
