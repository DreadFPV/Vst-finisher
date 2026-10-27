#include "Components.h"
#include "../PluginProcessor.h"

namespace ui
{
juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bg);
    setColour (juce::ToggleButton::textColourId, col::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.16f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (col::outline);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolare Parameter (Bereich um 0) von der Mitte aus füllen
    const auto range = s.getRange();
    float from = start;
    if (range.getStart() < 0.0 && range.getEnd() > 0.0)
        from = start + (float) s.valueToProportionOfLength (0.0) * (end - start);

    juce::Path val;
    val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.isEnabled() ? colour : col::textDim);
    g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - lineW * 1.3f;
    g.setColour (col::panelHi);
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (col::text);
    const juce::Point<float> tip (c.x + std::sin (angle) * knobR * 0.8f, c.y - std::cos (angle) * knobR * 0.8f);
    g.drawLine ({ c.getPointOnCircumference (knobR * 0.25f, angle), tip }, 2.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool onState = b.getToggleState();
    const auto pill = r.removeFromLeft (r.getHeight() * 1.8f).reduced (0.0f, r.getHeight() * 0.18f);
    g.setColour (onState ? col::accent : col::outline);
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    const float d = pill.getHeight() - 4.0f;
    const float cx = onState ? pill.getRight() - d - 2.0f : pill.getX() + 2.0f;
    g.setColour (onState ? col::bg : (highlighted ? col::text : col::textDim));
    g.fillEllipse (cx, pill.getY() + 2.0f, d, d);
    g.setColour (onState ? col::text : col::textDim);
    g.setFont (font (12.0f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = base;
    if (down) c = c.brighter (0.2f);
    else if (highlighted) c = c.brighter (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::outline.brighter (0.2f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.5f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text, juce::Colour colour)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (font (11.0f, true));
    label.setColour (juce::Label::textColourId, col::textDim);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds().withTrimmedTop (4);
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

Switch::Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text)
{
    setButtonText (text);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
}

//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    const auto r = area.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (col::text);
    g.setFont (font (13.0f, true));
    g.drawText (title, area.reduced (12, 0).removeFromTop (28), juce::Justification::centredLeft);
}

//==============================================================================
static float freqToX (float f, float w) { return w * std::log (f / 20.0f) / std::log (1000.0f); }

void EqCurve::refresh()
{
    const auto s = proc.readSettings();
    const double fs = proc.currentSampleRate.load();
    const float w = (float) getWidth(), h = (float) getHeight();
    curve.clear();
    for (int x = 0; x <= (int) w; x += 2)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) x / w);
        float db = s.eqOn ? (float) fin::Equalizer::responseDb (s, f, fs) : 0.0f;
        db = juce::jlimit (-15.0f, 15.0f, db);
        const float y = h * 0.5f - db / 15.0f * h * 0.5f;
        if (x == 0) curve.startNewSubPath ((float) x, y); else curve.lineTo ((float) x, y);
    }
    lastEqOn = s.eqOn;
    repaint();
}

void EqCurve::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 5.0f);

    g.setFont (font (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = freqToX (f, r.getWidth());
        g.setColour (col::outline);
        g.drawVerticalLine ((int) x, 0.0f, r.getHeight());
        g.setColour (col::textDim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x + 2, (int) r.getBottom() - 13, 30, 12,
                    juce::Justification::left);
    }
    g.setColour (col::outline.brighter (0.3f));
    g.drawHorizontalLine ((int) (r.getHeight() * 0.5f), 0.0f, r.getWidth());

    g.setColour ((lastEqOn ? col::accent : col::textDim).withAlpha (0.15f));
    juce::Path fill (curve);
    fill.lineTo (r.getWidth(), r.getHeight() * 0.5f);
    fill.lineTo (0.0f, r.getHeight() * 0.5f);
    fill.closeSubPath();
    g.fillPath (fill);
    g.setColour (lastEqOn ? col::accent : col::textDim);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

//==============================================================================
void GrMeter::setValue (float db)
{
    value = juce::jlimit (0.0f, range, -db);
    if (value >= hold) { hold = value; holdCounter = 45; }
    else if (--holdCounter <= 0) hold = juce::jmax (value, hold - 0.3f);
    repaint();
}

void GrMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (col::textDim);
    g.setFont (font (10.0f, true));
    g.drawText (name, r.removeFromTop (14), juce::Justification::centred);
    auto txt = r.removeFromBottom (14);
    auto bar = r.reduced (juce::jmax (0, (r.getWidth() - 14) / 2), 2).toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (bar, 3.0f);
    const float hh = bar.getHeight() * value / range;
    g.setColour (col::bad.withAlpha (0.85f));
    g.fillRoundedRectangle (bar.withHeight (hh), 3.0f);
    g.setColour (col::text);
    g.fillRect (bar.getX(), bar.getY() + bar.getHeight() * hold / range, bar.getWidth(), 1.5f);
    g.setColour (col::text);
    g.drawText (juce::String (-hold, 1), txt, juce::Justification::centred);
}

//==============================================================================
void HistoryView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (area, 6.0f);

    auto plot = area.reduced (8.0f, 8.0f).withTrimmedRight (34.0f);
    const float midY = plot.getCentreY(), halfH = plot.getHeight() * 0.5f;

    // LUFS-Skala rechts: -30 ... 0 LUFS über die ganze Höhe
    auto lufsToY = [&] (float l) { return plot.getBottom() - (juce::jlimit (-30.0f, 0.0f, l) + 30.0f) / 30.0f * plot.getHeight(); };
    g.setFont (font (10.0f));
    for (float l : { -24.0f, -18.0f, -12.0f, -6.0f })
    {
        const float y = lufsToY (l);
        g.setColour (col::outline.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (col::textDim);
        g.drawText (juce::String ((int) l), (int) plot.getRight() + 4, (int) y - 6, 30, 12, juce::Justification::left);
    }

    auto& e = proc.engine;
    const long long count = e.historyCount.load (std::memory_order_acquire);
    const int n = (int) std::min<long long> (count, fin::Engine::kHistorySize);
    const long long first = count - n;
    const int span = juce::jmax (n, 600);                      // min. 60 s Fensterbreite
    const int px = juce::jmax (1, (int) plot.getWidth());

    struct Col { float in = 0, out = 0, st = -200, gr = 0; bool valid = false; };
    std::vector<Col> cols ((size_t) px);
    for (int i = 0; i < n; ++i)
    {
        const auto& p = e.history[(size_t) ((first + i) % fin::Engine::kHistorySize)];
        const int x = juce::jlimit (0, px - 1, (int) ((double) i / span * px));
        auto& c = cols[(size_t) x];
        c.in = juce::jmax (c.in, p.inPeak);
        c.out = juce::jmax (c.out, p.outPeak);
        c.st = juce::jmax (c.st, p.shortTerm);
        c.gr = juce::jmin (c.gr, p.limGrDb);
        c.valid = true;
    }

    // Waveform
    for (int x = 0; x < px; ++x)
    {
        const auto& c = cols[(size_t) x];
        if (! c.valid) continue;
        const float fx = plot.getX() + (float) x;
        const float hin = juce::jmin (1.0f, c.in) * halfH, hout = juce::jmin (1.0f, c.out) * halfH;
        g.setColour (col::wave);
        g.drawVerticalLine ((int) fx, midY - hin, midY + hin);
        g.setColour (col::accent.withAlpha (0.55f));
        g.drawVerticalLine ((int) fx, midY - hout, midY + hout);
        if (c.gr < -0.1f)
        {
            g.setColour (col::bad.withAlpha (0.8f));
            g.drawVerticalLine ((int) fx, plot.getY(), plot.getY() + juce::jmin (12.0f, -c.gr) / 12.0f * 30.0f);
        }
    }

    // Short-Term-LUFS
    juce::Path lp;
    bool started = false;
    for (int x = 0; x < px; ++x)
    {
        const auto& c = cols[(size_t) x];
        if (! c.valid || c.st < -60.0f) { started = false; continue; }
        const float y = lufsToY (c.st);
        if (! started) { lp.startNewSubPath (plot.getX() + (float) x, y); started = true; }
        else lp.lineTo (plot.getX() + (float) x, y);
    }
    g.setColour (col::warn);
    g.strokePath (lp, juce::PathStrokeType (1.8f));

    // Ziel-Linie
    const float tgt = proc.apvts.getRawParameterValue (ParamIDs::tgtLufs)->load();
    const float ty = lufsToY (tgt);
    juce::Path dash;
    dash.startNewSubPath (plot.getX(), ty);
    dash.lineTo (plot.getRight(), ty);
    const float pattern[] = { 6.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dash, dash, pattern, 2);
    g.setColour (col::text.withAlpha (0.7f));
    g.fillPath (dash);
    g.setFont (font (10.0f, true));
    g.drawText ("Ziel " + juce::String (tgt, 1), (int) plot.getRight() - 80, (int) ty - 14, 78, 12, juce::Justification::right);

    // Legende / Zeit
    g.setColour (col::textDim);
    g.setFont (font (10.0f));
    const int secs = (int) (count / 10);
    g.drawText ("Zeit " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2)
                    + "   grau = Input   gruen = Output   gelb = Short-Term LUFS   rot = Limiter GR",
                plot.toNearestInt().removeFromBottom (14), juce::Justification::left);
}

//==============================================================================
void Readout::set (float v, juce::Colour c, const juce::String& sub)
{
    if (v != value || c != colour || sub != subText)
    {
        value = v; colour = c; subText = sub;
        repaint();
    }
}

void Readout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    r.reduce (8, 4);
    g.setColour (col::textDim);
    g.setFont (font (10.5f, true));
    g.drawText (name, r.removeFromTop (14), juce::Justification::left);
    if (subText.isNotEmpty())
    {
        g.setFont (font (10.0f));
        g.drawText (subText, r.removeFromBottom (13), juce::Justification::left);
    }
    g.setColour (colour);
    g.setFont (font (big ? 30.0f : 21.0f, true));
    const juce::String v = value <= -100.0f ? juce::String ("--") : juce::String (value, 1);
    g.drawText (v + " " + unitText, r, juce::Justification::centredLeft);
}
} // namespace ui
