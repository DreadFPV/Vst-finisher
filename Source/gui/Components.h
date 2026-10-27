#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class FinisherAudioProcessor;

namespace ui
{
namespace col
{
    const juce::Colour bg       { 0xff121417 };
    const juce::Colour panel    { 0xff1b1f24 };
    const juce::Colour panelHi  { 0xff242a31 };
    const juce::Colour outline  { 0xff2e353d };
    const juce::Colour text     { 0xffe6e9ec };
    const juce::Colour textDim  { 0xff8b949e };
    const juce::Colour accent   { 0xff3ddc84 };   // Grün
    const juce::Colour warn     { 0xfff2c94c };   // Gelb
    const juce::Colour bad      { 0xffeb5757 };   // Rot
    const juce::Colour wave     { 0xff4a5560 };
}

juce::Font font (float size, bool bold = false);

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13.0f, true); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
/** Drehregler mit Beschriftung, an einen Parameter gebunden. Doppelklick = Standardwert. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label,
          juce::Colour colour = col::accent);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Ein/Aus-Schalter, an einen Bool-Parameter gebunden. */
class Switch : public juce::ToggleButton
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
/** Panel mit Titel; Kinder werden vom Editor platziert. */
void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title);

//==============================================================================
/** Frequenzgang des EQ (20 Hz - 20 kHz, +-12 dB). */
class EqCurve : public juce::Component
{
public:
    explicit EqCurve (FinisherAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void refresh();

private:
    FinisherAudioProcessor& proc;
    juce::Path curve;
    bool lastEqOn = true;
};

//==============================================================================
/** Vertikale Pegelanzeige (z. B. Gain Reduction). */
class GrMeter : public juce::Component
{
public:
    GrMeter (juce::String label, float rangeDb) : name (std::move (label)), range (rangeDb) {}
    void setValue (float db);
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    float range;
    float value = 0.0f, hold = 0.0f;
    int holdCounter = 0;
};

//==============================================================================
/** Verlauf: Waveform (In grau, Out grün), Short-Term-LUFS-Kurve, Ziel-Linie, Limiter-GR. */
class HistoryView : public juce::Component
{
public:
    explicit HistoryView (FinisherAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    FinisherAudioProcessor& proc;
};

//==============================================================================
/** Großer Messwert mit Ampelfarbe. */
class Readout : public juce::Component
{
public:
    explicit Readout (juce::String label, juce::String unit, bool large = false)
        : name (std::move (label)), unitText (std::move (unit)), big (large) {}
    void set (float v, juce::Colour c, const juce::String& sub = {});
    void paint (juce::Graphics&) override;

private:
    juce::String name, unitText, subText;
    float value = -200.0f;
    juce::Colour colour = col::text;
    bool big;
};
} // namespace ui
