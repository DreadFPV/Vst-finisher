#pragma once

#include "PluginProcessor.h"
#include "gui/Components.h"
#include <map>
#include <array>

class FinisherAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit FinisherAudioProcessorEditor (FinisherAudioProcessor&);
    ~FinisherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum Module { kEq, kStereo, kTrans, kComp, kMb, kSat, kClip, kLim, kNumModules };
    void selectModule (int m);

private:
    void timerCallback() override;
    ui::Knob* knob (const juce::String& id, const juce::String& label, juce::Colour c);
    ui::Choice* choice (const juce::String& id, const juce::String& label);
    ui::Switch* toggle (const juce::String& id, const juce::String& label, juce::Colour c);
    void buildPages();

    FinisherAudioProcessor& proc;
    ui::LookAndFeel lnf;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::map<juce::String, ui::Knob*> knobById;

    std::array<std::unique_ptr<ui::ModuleTile>, kNumModules> tiles;
    std::array<std::unique_ptr<ui::Page>, kNumModules> pages;
    int selected = kLim;

    // Modul-spezifische Anzeigen
    ui::EqCurve eqCurve;
    ui::GrMeter compGr { "GR", 12.0f }, limGr { "GR", 12.0f }, clipGr { "CLIP", 6.0f, ui::col::clip };
    ui::GrMeter mbGr[3] { { "GR", 12.0f, ui::col::mb }, { "GR", 12.0f, ui::col::mb }, { "GR", 12.0f, ui::col::mb } };
    ui::BarMeter corrMeter { "KORRELATION", -1.0f, 1.0f, {}, true }, corrSmall { "KORRELATION", -1.0f, 1.0f, {}, true };
    ui::BarMeter trMeter { "TRANSIENT-GAIN", -12.0f, 12.0f, "dB" };
    juce::Component stereoRight, transRight, compRight, clipRight, limRight;

    // Presets
    void refreshPresetList();
    void loadPresetFromBox();
    void stepPreset (int dir);
    void savePresetDialog();
    juce::ComboBox presetBox;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetSave;
    juce::Array<juce::File> userPresetFiles;

    // Kopfzeile
    ui::ToggleTextButton abBtn, deltaBtn, monoBtn;
    juce::Label matchLabel;

    // Unten
    ui::HistoryView history;
    ui::Readout rdShort { "SHORT-TERM", "LUFS", true }, rdMom { "MOMENTARY", "LUFS" },
                rdInt { "INTEGRATED", "LUFS", true }, rdLra { "LOUDNESS RANGE", "LU" },
                rdTp { "TRUE PEAK MAX", "dBTP" };
    juce::TextButton resetBtn, applyBtn;
    juce::Label status;
    juce::Rectangle<int> pHist, pMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinisherAudioProcessorEditor)
};
