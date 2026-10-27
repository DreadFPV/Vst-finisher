#pragma once

#include "PluginProcessor.h"
#include "gui/Components.h"
#include <map>

class FinisherAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit FinisherAudioProcessorEditor (FinisherAudioProcessor&);
    ~FinisherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ui::Knob& knob (const char* id, const juce::String& label, juce::Colour c = ui::col::accent);

    FinisherAudioProcessor& proc;
    ui::LookAndFeel lnf;

    std::vector<std::unique_ptr<ui::Knob>> knobs;
    std::map<juce::String, ui::Knob*> knobById;

    ui::Switch eqOn, hpOn, monoOn, compOn, limOn;
    ui::EqCurve eqCurve;
    ui::GrMeter compGr { "GR", 12.0f }, limGr { "GR", 12.0f };
    ui::HistoryView history;

    ui::Readout rdShort { "SHORT-TERM", "LUFS", true }, rdMom { "MOMENTARY", "LUFS" },
                rdInt { "INTEGRATED", "LUFS", true }, rdLra { "LOUDNESS RANGE", "LU" },
                rdTp { "TRUE PEAK MAX", "dBTP" };

    juce::TextButton resetBtn { "Messung zuruecksetzen" }, applyBtn { "Ziele anwenden" };
    juce::Label status;

    juce::Rectangle<int> pEq, pStereo, pComp, pLim, pHist, pMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinisherAudioProcessorEditor)
};
