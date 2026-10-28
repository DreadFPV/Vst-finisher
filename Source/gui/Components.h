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

    // Modulfarben
    const juce::Colour eq       { 0xff56ccf2 };
    const juce::Colour stereo   { 0xffbb86fc };
    const juce::Colour trans    { 0xff6fcf97 };
    const juce::Colour comp     { 0xfff2994a };
    const juce::Colour mb       { 0xffeb8fb0 };
    const juce::Colour sat      { 0xffe2b45a };
    const juce::Colour clip     { 0xffff6b6b };
    const juce::Colour lim      { 0xff3ddc84 };
    const juce::Colour target   { 0xfff2c94c };
}

juce::Font font (float size, bool bold = false);
juce::String utf8 (const char* s);

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.5f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f, true); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
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

/** Auswahlbox mit Beschriftung, an einen Choice-Parameter gebunden. */
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

/** Ein/Aus-Schalter, an einen Bool-Parameter gebunden. */
class Switch : public juce::ToggleButton
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** Umschalt-Taste (Text-Button), an einen Bool-Parameter gebunden. */
class ToggleTextButton : public juce::TextButton
{
public:
    ToggleTextButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text,
                      juce::Colour onColour);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour titleColour = col::text);

/** Seite eines Moduls: Spalten mit Titel, darin Zeilen mit Bedienelementen (gleich breit verteilt). */
class Page : public juce::Component
{
public:
    struct Column
    {
        juce::String title;
        std::vector<std::vector<juce::Component*>> rows;
        float weight = 1.0f;
        juce::Component* headerItem = nullptr;   // z. B. Schalter neben dem Titel
    };

    Page (juce::String name, juce::Colour colour) : title (std::move (name)), colour (colour) {}

    void setColumns (std::vector<Column> c) { columns = std::move (c); resized(); }
    void setTop (juce::Component* c, int height) { top = c; topHeight = height; }
    void setRight (juce::Component* c, int width) { right = c; rightWidth = width; }
    void setHint (const juce::String& h) { hint = h; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title, hint;
    juce::Colour colour;
    std::vector<Column> columns;
    std::vector<juce::Rectangle<int>> colRects;
    juce::Component* top = nullptr;
    int topHeight = 0;
    juce::Component* right = nullptr;
    int rightWidth = 0;
};

//==============================================================================
/** Kachel in der Modulleiste: Name, Ein/Aus, Live-Wert, anklickbar. */
class ModuleTile : public juce::Component
{
public:
    ModuleTile (juce::AudioProcessorValueTreeState& state, const juce::String& onParam, juce::String name, juce::Colour colour);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setValueText (const juce::String& t) { if (t != valueText) { valueText = t; repaint(); } }
    bool isOn() const { return power.getToggleState(); }
    std::function<void()> onClick;

private:
    Switch power;
    juce::String name, valueText;
    juce::Colour colour;
    bool selected = false;
};

//==============================================================================
/** Frequenzgang des EQ (20 Hz - 20 kHz, +-15 dB): statisch (gefüllt) + aktuell inkl. Dynamik (weiss). */
class EqCurve : public juce::Component
{
public:
    explicit EqCurve (FinisherAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void refresh();

private:
    FinisherAudioProcessor& proc;
    juce::Path curveStatic, curveDyn;
    std::vector<juce::Point<float>> markers;
    bool lastEqOn = true, hasDyn = false;
};

//==============================================================================
/** Vertikale Gain-Reduction-Anzeige mit Peak-Hold. */
class GrMeter : public juce::Component
{
public:
    GrMeter (juce::String label, float rangeDb, juce::Colour c = col::bad) : name (std::move (label)), range (rangeDb), colour (c) {}
    void setValue (float db);
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    float range;
    juce::Colour colour;
    float value = 0.0f, hold = 0.0f;
    int holdCounter = 0;
};

/** Horizontale Anzeige mit Mittelpunkt (z. B. Korrelation -1..+1 oder Transient +-12 dB). */
class BarMeter : public juce::Component
{
public:
    BarMeter (juce::String label, float minV, float maxV, juce::String unit, bool correlation = false)
        : name (std::move (label)), lo (minV), hi (maxV), unitText (std::move (unit)), corr (correlation) {}
    void setValue (float v) { if (std::abs (v - value) > 1e-3f) { value = v; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    float lo, hi;
    juce::String unitText;
    bool corr;
    float value = 0.0f;
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
/** Grosser Messwert mit Ampelfarbe. */
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
