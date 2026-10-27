#include "PluginEditor.h"

using namespace ParamIDs;

namespace
{
constexpr int kW = 1200, kH = 730;
const juce::Colour eqCol   { 0xff56ccf2 };
const juce::Colour stCol   { 0xffbb86fc };
const juce::Colour compCol { 0xfff2994a };
const juce::Colour limCol  { 0xff3ddc84 };
const juce::Colour tgtCol  { 0xfff2c94c };

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

/** Ampelfarbe nach Abstand zum Ziel. */
juce::Colour traffic (float diff, float good, float ok)
{
    const float a = std::abs (diff);
    return a <= good ? ui::col::accent : (a <= ok ? ui::col::warn : ui::col::bad);
}

juce::String delta (float v, float target, const char* unit)
{
    if (v <= -100.0f) return {};
    const float d = v - target;
    return utf8 ("Ziel ") + juce::String (target, 1) + "  (" + (d >= 0 ? "+" : "") + juce::String (d, 1) + " " + unit + ")";
}
} // namespace

FinisherAudioProcessorEditor::FinisherAudioProcessorEditor (FinisherAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      eqOn (p.apvts, ParamIDs::eqOn, "EQ"),
      hpOn (p.apvts, ParamIDs::hpOn, "Low-Cut"),
      monoOn (p.apvts, ParamIDs::monoOn, "Mono-Bass"),
      compOn (p.apvts, ParamIDs::compOn, "Kompressor"),
      limOn (p.apvts, ParamIDs::limOn, "Limiter"),
      eqCurve (p), history (p)
{
    setLookAndFeel (&lnf);

    // EQ
    knob (hpFreq, "LOW-CUT", eqCol);
    knob (lsFreq, "LOW FREQ", eqCol);   knob (lsGain, "LOW GAIN", eqCol);
    knob (p1Freq, "BELL 1", eqCol);     knob (p1Gain, "GAIN", eqCol);     knob (p1Q, "Q", eqCol);
    knob (p2Freq, "BELL 2", eqCol);     knob (p2Gain, "GAIN", eqCol);     knob (p2Q, "Q", eqCol);
    knob (hsFreq, "HIGH FREQ", eqCol);  knob (hsGain, "HIGH GAIN", eqCol);
    knob (inGain, "INPUT", ui::col::text);
    // Stereo
    knob (monoFreq, "MONO UNTER", stCol);
    knob (width, "BREITE", stCol);
    // Kompressor
    knob (thresh, "THRESHOLD", compCol); knob (ratio, "RATIO", compCol);
    knob (attack, "ATTACK", compCol);    knob (release, "RELEASE", compCol);
    knob (knee, "KNEE", compCol);        knob (makeup, "MAKEUP", compCol);   knob (mix, "MIX", compCol);
    // Limiter
    knob (drive, "DRIVE", limCol);       knob (ceiling, "CEILING", limCol);
    knob (limRel, "RELEASE", limCol);    knob (outGain, "OUTPUT", ui::col::text);
    // Ziele
    knob (tgtLufs, "ZIEL LUFS", tgtCol); knob (tgtTp, "ZIEL TRUE PEAK", tgtCol); knob (tgtLra, "ZIEL LRA", tgtCol);

    for (auto* c : std::initializer_list<juce::Component*> { &eqOn, &hpOn, &monoOn, &compOn, &limOn, &eqCurve, &compGr, &limGr,
                                                             &history, &rdShort, &rdMom, &rdInt, &rdLra, &rdTp, &resetBtn, &applyBtn, &status })
        addAndMakeVisible (c);

    resetBtn.setButtonText (utf8 ("Messung zurücksetzen"));
    resetBtn.onClick = [this]
    {
        proc.resetMeters();
        status.setText (utf8 ("Messung zurückgesetzt. Track von Anfang an abspielen."), juce::dontSendNotification);
    };
    applyBtn.setColour (juce::TextButton::buttonColourId, tgtCol.withAlpha (0.85f));
    applyBtn.setColour (juce::TextButton::textColourOffId, ui::col::bg);
    applyBtn.onClick = [this] { status.setText (utf8 (proc.applyTargets().toRawUTF8()), juce::dontSendNotification); };

    status.setFont (ui::font (11.5f));
    status.setColour (juce::Label::textColourId, ui::col::textDim);
    status.setJustificationType (juce::Justification::topLeft);
    status.setText (utf8 ("Ziele einstellen → Track komplett abspielen → „Ziele anwenden“ → nochmal prüfen."), juce::dontSendNotification);

    setSize (kW, kH);
    startTimerHz (30);
}

FinisherAudioProcessorEditor::~FinisherAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ui::Knob& FinisherAudioProcessorEditor::knob (const char* id, const juce::String& label, juce::Colour c)
{
    knobs.push_back (std::make_unique<ui::Knob> (proc.apvts, id, label, c));
    auto& k = *knobs.back();
    knobById[id] = &k;
    addAndMakeVisible (k);
    return k;
}

void FinisherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::col::bg);

    // Kopfzeile
    g.setColour (ui::col::text);
    g.setFont (ui::font (24.0f, true));
    g.drawText ("FINISHER", 18, 10, 200, 30, juce::Justification::centredLeft);
    g.setColour (ui::col::textDim);
    g.setFont (ui::font (12.0f));
    g.drawText (utf8 ("Mastering-Strip · EQ → Mono-Bass → Kompressor → True-Peak-Limiter · Just DereadBeats / justcreateit.ch"),
                150, 10, 760, 30, juce::Justification::centredLeft);
    g.drawText ("v" JucePlugin_VersionString, getWidth() - 80, 10, 62, 30, juce::Justification::centredRight);

    ui::drawPanel (g, pEq, "");
    ui::drawPanel (g, pStereo, "");
    ui::drawPanel (g, pComp, "");
    ui::drawPanel (g, pLim, "");
    ui::drawPanel (g, pHist, "VERLAUF");
    ui::drawPanel (g, pMeter, "LOUDNESS & ZIELE");
}

void FinisherAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (38);

    auto top = area.removeFromTop (330);
    area.removeFromTop (10);
    auto bottom = area;

    pEq = top.removeFromLeft (480);     top.removeFromLeft (10);
    pStereo = top.removeFromLeft (130); top.removeFromLeft (10);
    pComp = top.removeFromLeft (300);   top.removeFromLeft (10);
    pLim = top;

    pHist = bottom.removeFromLeft (700); bottom.removeFromLeft (10);
    pMeter = bottom;

    auto header = [] (juce::Rectangle<int> panel) { return panel.reduced (10, 0).removeFromTop (30).reduced (0, 4); };
    auto body   = [] (juce::Rectangle<int> panel) { return panel.reduced (10).withTrimmedTop (24); };

    auto placeRow = [this] (juce::Rectangle<int> row, std::initializer_list<const char*> ids)
    {
        const int w = row.getWidth() / (int) ids.size();
        for (auto* id : ids)
            knobById[id]->setBounds (row.removeFromLeft (w).reduced (2, 2));
    };

    // EQ
    {
        auto h = header (pEq);
        eqOn.setBounds (h.removeFromLeft (70));
        hpOn.setBounds (h.removeFromLeft (100));
        auto b = body (pEq);
        eqCurve.setBounds (b.removeFromTop (100));
        eqCurve.refresh();
        b.removeFromTop (6);
        const int rowH = b.getHeight() / 2;
        placeRow (b.removeFromTop (rowH), { hpFreq, lsFreq, lsGain, p1Freq, p1Gain, p1Q });
        placeRow (b, { p2Freq, p2Gain, p2Q, hsFreq, hsGain, inGain });
    }
    // Stereo
    {
        monoOn.setBounds (header (pStereo));
        auto b = body (pStereo);
        const int rowH = b.getHeight() / 2;
        knobById[monoFreq]->setBounds (b.removeFromTop (rowH).reduced (6, 4));
        knobById[width]->setBounds (b.reduced (6, 4));
    }
    // Kompressor
    {
        compOn.setBounds (header (pComp).removeFromLeft (120));
        auto b = body (pComp);
        compGr.setBounds (b.removeFromRight (34));
        const int rowH = b.getHeight() / 2;
        placeRow (b.removeFromTop (rowH), { thresh, ratio, attack, release });
        placeRow (b.withTrimmedRight (b.getWidth() / 4), { knee, makeup, mix });
    }
    // Limiter
    {
        limOn.setBounds (header (pLim).removeFromLeft (110));
        auto b = body (pLim);
        limGr.setBounds (b.removeFromRight (34));
        const int rowH = b.getHeight() / 2;
        placeRow (b.removeFromTop (rowH), { drive, ceiling });
        placeRow (b, { limRel, outGain });
    }
    // Verlauf
    history.setBounds (body (pHist));

    // Meter + Ziele
    {
        auto b = body (pMeter);
        auto row1 = b.removeFromTop (66);
        rdShort.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (2));
        rdInt.setBounds (row1.reduced (2));
        auto row2 = b.removeFromTop (56);
        const int w3 = row2.getWidth() / 3;
        rdMom.setBounds (row2.removeFromLeft (w3).reduced (2));
        rdLra.setBounds (row2.removeFromLeft (w3).reduced (2));
        rdTp.setBounds (row2.reduced (2));
        b.removeFromTop (6);
        auto row3 = b.removeFromTop (88);
        placeRow (row3, { tgtLufs, tgtTp, tgtLra });
        b.removeFromTop (6);
        auto row4 = b.removeFromTop (30);
        resetBtn.setBounds (row4.removeFromLeft (row4.getWidth() / 2).reduced (2, 0));
        applyBtn.setBounds (row4.reduced (2, 0));
        b.removeFromTop (4);
        status.setBounds (b);
    }
}

void FinisherAudioProcessorEditor::timerCallback()
{
    auto& e = proc.engine;
    const auto t = proc.readTargets();

    const float st = e.shortTerm.load(), mo = e.momentary.load(), in = e.integrated.load();
    const float lra = e.lra.load(), tp = e.truePeakMax.load();

    rdShort.set (st, st <= -100.0f ? ui::col::textDim : traffic (st - t.lufs, 1.0f, 3.0f));
    rdMom.set (mo, ui::col::text);
    rdInt.set (in, in <= -100.0f ? ui::col::textDim : traffic (in - t.lufs, 0.5f, 1.5f), delta (in, t.lufs, "LU"));
    const bool lraValid = e.measuredSeconds.load() >= 3.0f;
    rdLra.set (lraValid ? lra : -200.0f, lraValid ? traffic (lra - t.lra, 1.0f, 2.5f) : ui::col::textDim,
               lraValid ? delta (lra, t.lra, "LU") : juce::String());
    rdTp.set (tp, tp <= -100.0f ? ui::col::textDim : (tp <= t.truePeak + 0.05f ? ui::col::accent : ui::col::bad),
              delta (tp, t.truePeak, "dB"));

    compGr.setValue (e.compGr.load());
    limGr.setValue (e.limGr.load());

    for (auto* k : { knobById[thresh], knobById[ratio], knobById[attack], knobById[release], knobById[knee], knobById[makeup], knobById[mix] })
        k->setEnabled (compOn.getToggleState());
    for (auto* k : { knobById[drive], knobById[ceiling], knobById[limRel] })
        k->setEnabled (limOn.getToggleState());
    for (auto* k : { knobById[monoFreq] })
        k->setEnabled (monoOn.getToggleState());
    knobById[hpFreq]->setEnabled (hpOn.getToggleState() && eqOn.getToggleState());

    eqCurve.refresh();
    history.repaint();
}
