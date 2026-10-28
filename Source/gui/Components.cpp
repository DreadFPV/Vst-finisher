#include "Components.h"
#include "../PluginProcessor.h"

namespace ui
{
juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

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
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::outline);
    setColour (juce::ComboBox::arrowColourId, col::textDim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
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
    g.setColour (s.isEnabled() ? colour : col::textDim.withAlpha (0.6f));
    g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - lineW * 1.3f;
    g.setColour (col::panelHi);
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (s.isEnabled() ? col::text : col::textDim);
    const juce::Point<float> tip (c.x + std::sin (angle) * knobR * 0.8f, c.y - std::cos (angle) * knobR * 0.8f);
    g.drawLine ({ c.getPointOnCircumference (knobR * 0.25f, angle), tip }, 2.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool onState = b.getToggleState();
    const float ph = juce::jmin (r.getHeight(), 18.0f);
    const auto pill = r.removeFromLeft (ph * 1.8f).withSizeKeepingCentre (ph * 1.8f, ph * 0.8f);
    const auto onCol = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (onState ? onCol : col::outline);
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    const float d = pill.getHeight() - 4.0f;
    const float cx = onState ? pill.getRight() - d - 2.0f : pill.getX() + 2.0f;
    g.setColour (onState ? col::bg : (highlighted ? col::text : col::textDim));
    g.fillEllipse (cx, pill.getY() + 2.0f, d, d);
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (onState ? col::text : col::textDim);
        g.setFont (font (12.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (down) c = c.brighter (0.2f);
    else if (highlighted) c = c.brighter (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::outline.brighter (0.2f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (col::textDim);
    g.fillPath (arrow);
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (font (10.5f, true));
    label.setColour (juce::Label::textColourId, col::textDim);
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    // Regler nicht riesig werden lassen; Beschriftung direkt darüber
    auto r = getLocalBounds().withTrimmedTop (2);
    const int w = juce::jmin (r.getWidth(), 110), h = juce::jmin (r.getHeight(), 126);
    auto block = r.withSizeKeepingCentre (w, h);
    label.setBounds (block.removeFromTop (14).withX (r.getX()).withWidth (r.getWidth()));
    slider.setBounds (block);
}

Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (10.5f, true));
    label.setColour (juce::Label::textColourId, col::textDim);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, box);
}

void Choice::resized()
{
    auto r = getLocalBounds().withTrimmedTop (2);
    label.setBounds (r.removeFromTop (14));
    box.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 96), 24));
}

Switch::Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text)
{
    setButtonText (text);
    setColour (juce::ToggleButton::tickColourId, col::accent);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
}

ToggleTextButton::ToggleTextButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text,
                                    juce::Colour onColour)
{
    setButtonText (text);
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonOnColourId, onColour);
    setColour (juce::TextButton::textColourOnId, col::bg);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
}

//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour titleColour)
{
    const auto r = area.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (titleColour);
        g.setFont (font (13.0f, true));
        g.drawText (title, area.reduced (12, 0).removeFromTop (28), juce::Justification::centredLeft);
    }
}

//==============================================================================
void Page::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), title, colour);
    g.setFont (font (10.5f, true));
    for (size_t i = 0; i < columns.size() && i < colRects.size(); ++i)
    {
        const auto r = colRects[i];
        g.setColour (col::bg.withAlpha (0.35f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.drawText (columns[i].title, r.reduced (8, 0).removeFromTop (20), juce::Justification::centredLeft);
    }
    if (hint.isNotEmpty())
    {
        g.setColour (col::textDim);
        g.setFont (font (11.0f));
        g.drawText (hint, getLocalBounds().reduced (14, 6).removeFromBottom (16), juce::Justification::centredLeft);
    }
}

void Page::resized()
{
    auto area = getLocalBounds().reduced (10).withTrimmedTop (22);
    if (hint.isNotEmpty()) area.removeFromBottom (16);
    if (right != nullptr) { right->setBounds (area.removeFromRight (rightWidth)); area.removeFromRight (8); }
    if (top != nullptr)   { top->setBounds (area.removeFromTop (topHeight)); area.removeFromTop (6); }

    colRects.clear();
    float total = 0.0f;
    for (auto& c : columns) total += c.weight;
    const int gap = 6;
    const int avail = area.getWidth() - gap * juce::jmax (0, (int) columns.size() - 1);
    int x = area.getX();
    for (auto& c : columns)
    {
        const int w = (int) std::round (avail * c.weight / juce::jmax (0.001f, total));
        juce::Rectangle<int> r (x, area.getY(), w, area.getHeight());
        colRects.push_back (r);
        x += w + gap;

        auto inner = r.reduced (4).withTrimmedTop (18);
        if (c.headerItem != nullptr)
        {
            const bool noText = dynamic_cast<juce::Button*> (c.headerItem) != nullptr && static_cast<juce::Button*> (c.headerItem)->getButtonText().isEmpty();
            c.headerItem->setBounds (r.reduced (6, 0).removeFromTop (22).removeFromRight (noText ? 36 : juce::jmin (90, r.getWidth() / 2)));
        }
        if (c.rows.empty()) continue;
        const int rowH = inner.getHeight() / (int) c.rows.size();
        for (auto& row : c.rows)
        {
            auto rr = inner.removeFromTop (rowH);
            if (row.empty()) continue;
            const int iw = rr.getWidth() / (int) row.size();
            for (auto* comp : row)
            {
                auto cell = rr.removeFromLeft (iw);
                if (comp == nullptr) continue;
                if (dynamic_cast<juce::ToggleButton*> (comp) != nullptr)
                    comp->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 4, 24));
                else
                    comp->setBounds (cell.reduced (1, 1));
            }
        }
    }
}

//==============================================================================
ModuleTile::ModuleTile (juce::AudioProcessorValueTreeState& state, const juce::String& onParam, juce::String n, juce::Colour c)
    : power (state, onParam, {}), name (std::move (n)), colour (c)
{
    power.setColour (juce::ToggleButton::tickColourId, colour);
    power.onStateChange = [this] { repaint(); };
    addAndMakeVisible (power);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModuleTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? col::panelHi : col::panel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (selected ? colour : col::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, selected ? 2.0f : 1.0f);

    // Farbstreifen oben
    g.setColour (power.getToggleState() ? colour : col::outline);
    g.fillRoundedRectangle (r.removeFromTop (4.0f).reduced (8.0f, 0.0f), 2.0f);

    auto t = getLocalBounds().reduced (10, 6);
    t.removeFromRight (36);
    g.setColour (power.getToggleState() ? col::text : col::textDim);
    g.setFont (font (12.0f, true));
    g.drawText (name, t.removeFromTop (t.getHeight() / 2 + 4), juce::Justification::bottomLeft);
    g.setColour (power.getToggleState() ? colour.withAlpha (0.9f) : col::textDim.withAlpha (0.7f));
    g.setFont (font (11.0f));
    g.drawText (power.getToggleState() ? valueText : utf8 ("aus"), t, juce::Justification::topLeft);
}

void ModuleTile::resized()
{
    power.setBounds (getLocalBounds().reduced (6, 0).removeFromRight (36).withSizeKeepingCentre (36, 24));
}

//==============================================================================
static float freqToX (float f, float w) { return w * std::log (f / 20.0f) / std::log (1000.0f); }

void EqCurve::refresh()
{
    const auto s = proc.readSettings();
    const double fs = proc.currentSampleRate.load();
    const float w = (float) getWidth(), h = (float) getHeight();
    float dyn[fin::DynamicEq::kBands];
    hasDyn = false;
    for (int b = 0; b < fin::DynamicEq::kBands; ++b)
    {
        dyn[b] = proc.engine.eqDyn[b].load();
        hasDyn |= std::abs (dyn[b]) > 0.05f;
    }
    auto toY = [h] (double db) { return h * 0.5f - (float) juce::jlimit (-15.0, 15.0, db) / 15.0f * h * 0.5f; };

    curveStatic.clear(); curveDyn.clear();
    for (int x = 0; x <= (int) w; x += 2)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) x / w);
        const double st = s.eqOn ? fin::DynamicEq::responseDb (s, f, fs) : 0.0;
        if (x == 0) curveStatic.startNewSubPath ((float) x, toY (st)); else curveStatic.lineTo ((float) x, toY (st));
        if (hasDyn && s.eqOn)
        {
            const double dy = fin::DynamicEq::responseDb (s, f, fs, dyn);
            if (x == 0) curveDyn.startNewSubPath ((float) x, toY (dy)); else curveDyn.lineTo ((float) x, toY (dy));
        }
    }
    markers.clear();
    for (int b = 0; b < fin::DynamicEq::kBands; ++b)
    {
        const auto& band = fin::DynamicEq::band (s, b);
        markers.push_back ({ freqToX (band.freq, w), toY (band.gain + dyn[b]) });
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

    const auto c = lastEqOn ? col::eq : col::textDim;
    g.setColour (c.withAlpha (0.15f));
    juce::Path fill (curveStatic);
    fill.lineTo (r.getWidth(), r.getHeight() * 0.5f);
    fill.lineTo (0.0f, r.getHeight() * 0.5f);
    fill.closeSubPath();
    g.fillPath (fill);
    g.setColour (c);
    g.strokePath (curveStatic, juce::PathStrokeType (2.0f));
    if (hasDyn && lastEqOn)
    {
        g.setColour (col::text.withAlpha (0.85f));
        g.strokePath (curveDyn, juce::PathStrokeType (1.3f));
    }
    const char* names[] = { "L", "1", "2", "H" };
    g.setFont (font (9.5f, true));
    for (size_t i = 0; i < markers.size(); ++i)
    {
        const auto p = markers[i];
        g.setColour (c);
        g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
        g.setColour (col::bg);
        g.drawText (names[i], juce::Rectangle<float> (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f), juce::Justification::centred);
    }
    g.setColour (col::textDim);
    g.setFont (font (10.0f));
    g.drawText (utf8 ("farbig = eingestellt   weiss = aktuell inkl. Dynamik"), r.reduced (6, 3).toNearestInt(), juce::Justification::topRight);
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
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * value / range), 3.0f);
    g.setColour (col::text);
    g.fillRect (bar.getX(), bar.getY() + bar.getHeight() * hold / range, bar.getWidth(), 1.5f);
    g.drawText (juce::String (-hold, 1), txt, juce::Justification::centred);
}

void BarMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (2);
    g.setColour (col::textDim);
    g.setFont (font (10.5f, true));
    auto head = r.removeFromTop (16);
    g.drawText (name, head, juce::Justification::centredLeft);
    const float v = juce::jlimit (lo, hi, value);
    juce::Colour c = col::accent;
    if (corr) c = v < 0.0f ? col::bad : (v < 0.3f ? col::warn : col::accent);
    g.setColour (c);
    g.setFont (font (12.0f, true));
    g.drawText ((v > 0 && ! corr ? "+" : "") + juce::String (v, corr ? 2 : 1) + (unitText.isEmpty() ? "" : " " + unitText), head,
                juce::Justification::centredRight);

    auto bar = r.removeFromTop (14).toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (bar, 3.0f);
    const float zero = bar.getX() + bar.getWidth() * (0.0f - lo) / (hi - lo);
    const float pos  = bar.getX() + bar.getWidth() * (v - lo) / (hi - lo);
    g.setColour (c.withAlpha (0.85f));
    g.fillRect (juce::Rectangle<float> (juce::jmin (zero, pos), bar.getY() + 2, std::abs (pos - zero), bar.getHeight() - 4));
    g.setColour (col::text);
    g.fillRect (zero - 0.5f, bar.getY(), 1.0f, bar.getHeight());
    g.setFont (font (9.5f));
    g.setColour (col::textDim);
    auto lab = r.removeFromTop (12);
    g.drawText (juce::String (lo, 0), lab, juce::Justification::left);
    g.drawText (juce::String (hi, 0), lab, juce::Justification::right);
}

//==============================================================================
void HistoryView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (area, 6.0f);

    auto plot = area.reduced (8.0f, 8.0f).withTrimmedRight (34.0f).withTrimmedBottom (14.0f);
    const float midY = plot.getCentreY(), halfH = plot.getHeight() * 0.5f;

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

    for (int x = 0; x < px; ++x)
    {
        const auto& c = cols[(size_t) x];
        if (! c.valid) continue;
        const float fx = plot.getX() + (float) x;
        const float hin = juce::jmin (1.0f, c.in) * halfH, hout = juce::jmin (1.0f, c.out) * halfH;
        g.setColour (col::wave);
        g.drawVerticalLine ((int) fx, midY - hin, midY + hin);
        g.setColour (col::accent.withAlpha (0.5f));
        g.drawVerticalLine ((int) fx, midY - hout, midY + hout);
        if (c.gr < -0.1f)
        {
            g.setColour (col::bad.withAlpha (0.8f));
            g.drawVerticalLine ((int) fx, plot.getY(), plot.getY() + juce::jmin (12.0f, -c.gr) / 12.0f * 30.0f);
        }
    }

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

    g.setColour (col::textDim);
    g.setFont (font (10.0f));
    const int secs = (int) (count / 10);
    g.drawText ("Zeit " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2)
                    + utf8 ("   grau = Input   grün = Output   gelb = Short-Term LUFS   rot = Limiter GR"),
                area.reduced (8.0f, 4.0f).toNearestInt().removeFromBottom (14), juce::Justification::left);
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
    g.setFont (font (big ? 28.0f : 20.0f, true));
    const juce::String v = value <= -100.0f ? juce::String ("--") : juce::String (value, 1);
    g.drawText (v + " " + unitText, r, juce::Justification::centredLeft);
}
} // namespace ui
