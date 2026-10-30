#include "PluginEditor.h"

using namespace ParamIDs;
using ui::utf8;
namespace col = ui::col;

namespace
{
constexpr int kW = 1240, kH = 850;

/** Ampelfarbe nach Abstand zum Ziel. */
juce::Colour traffic (float diff, float good, float ok)
{
    const float a = std::abs (diff);
    return a <= good ? col::accent : (a <= ok ? col::warn : col::bad);
}

juce::String toTarget (float v, float target, const char* unit)
{
    if (v <= -100.0f) return {};
    const float d = v - target;
    return "Ziel " + juce::String (target, 1) + "  (" + (d >= 0 ? "+" : "") + juce::String (d, 1) + " " + unit + ")";
}

juce::String grText (float db) { return db < -0.05f ? "GR " + juce::String (db, 1) + " dB" : utf8 ("bereit"); }

/** Container, der seine Kinder vertikal stapelt (für rechte Spalten in Seiten). */
void stack (juce::Component& parent, std::initializer_list<std::pair<juce::Component*, int>> items)
{
    auto r = parent.getLocalBounds();
    for (auto& [c, h] : items)
    {
        if (h < 0) { c->setBounds (r); break; }
        c->setBounds (r.removeFromTop (h));
        r.removeFromTop (6);
    }
}
} // namespace

//==============================================================================
FinisherAudioProcessorEditor::FinisherAudioProcessorEditor (FinisherAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), eqCurve (p),
      abBtn (p.apvts, abBypass, "A/B Bypass", col::warn),
      deltaBtn (p.apvts, ParamIDs::delta, "Delta", col::clip),
      monoBtn (p.apvts, monoLis, "Mono", col::stereo),
      history (p)
{
    setLookAndFeel (&lnf);

    // Modulleiste
    struct TileDef { const char* on; const char* name; juce::Colour c; };
    const TileDef defs[kNumModules] = {
        { eqOn, "DYN-EQ", col::eq }, { monoOn, "STEREO", col::stereo }, { trOn, "TRANSIENT", col::trans },
        { compOn, "KOMPRESSOR", col::comp }, { mbOn, "MULTIBAND", col::mb }, { satOn, "SATURATION", col::sat },
        { clipOn, "CLIPPER", col::clip }, { limOn, "LIMITER", col::lim } };
    for (int m = 0; m < kNumModules; ++m)
    {
        tiles[(size_t) m] = std::make_unique<ui::ModuleTile> (p.apvts, defs[m].on, utf8 (defs[m].name), defs[m].c);
        tiles[(size_t) m]->onClick = [this, m] { selectModule (m); };
        addAndMakeVisible (*tiles[(size_t) m]);
    }

    buildPages();

    // Kopfzeile
    for (auto* b : { (juce::Component*) &abBtn, (juce::Component*) &deltaBtn, (juce::Component*) &monoBtn, (juce::Component*) &matchLabel })
        addAndMakeVisible (b);
    abBtn.setTooltip (utf8 ("Original hören – automatisch auf gleiche Lautheit gebracht"));
    deltaBtn.setTooltip (utf8 ("Nur das hören, was der Finisher verändert"));
    monoBtn.setTooltip (utf8 ("Mono abhören (Club-Check)"));
    matchLabel.setFont (ui::font (11.0f));
    matchLabel.setColour (juce::Label::textColourId, col::textDim);
    matchLabel.setJustificationType (juce::Justification::centredRight);

    // Unten: Meter + Ziele
    knob (tgtLufs, "ZIEL LUFS", col::target);
    knob (tgtTp, "ZIEL TRUE PEAK", col::target);
    knob (tgtLra, "ZIEL LRA", col::target);
    for (auto* id : { tgtLufs, tgtTp, tgtLra }) addAndMakeVisible (knobById[id]);
    for (auto* c : std::initializer_list<juce::Component*> { &history, &rdShort, &rdMom, &rdInt, &rdLra, &rdTp, &corrSmall, &resetBtn, &applyBtn, &status })
        addAndMakeVisible (c);

    resetBtn.setButtonText (utf8 ("Messung zurücksetzen"));
    resetBtn.onClick = [this]
    {
        proc.resetMeters();
        status.setText (utf8 ("Messung zurückgesetzt. Track von Anfang an abspielen."), juce::dontSendNotification);
    };
    applyBtn.setButtonText ("Ziele anwenden");
    applyBtn.setColour (juce::TextButton::buttonColourId, col::target.withAlpha (0.85f));
    applyBtn.setColour (juce::TextButton::textColourOffId, col::bg);
    applyBtn.onClick = [this] { status.setText (proc.applyTargets(), juce::dontSendNotification); };

    status.setFont (ui::font (11.5f));
    status.setColour (juce::Label::textColourId, col::textDim);
    status.setJustificationType (juce::Justification::topLeft);
    status.setText (utf8 ("Ziele einstellen → Track komplett abspielen → „Ziele anwenden“ → nochmal prüfen."), juce::dontSendNotification);

    // Presets
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &presetPrev, &presetNext, &presetSave })
        addAndMakeVisible (c);
    presetSave.setButtonText (utf8 ("Speichern…"));
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this] { loadPresetFromBox(); };
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (+1); };
    presetSave.onClick = [this] { savePresetDialog(); };
    refreshPresetList();

    selectModule (kLim);
    setSize (kW, kH);
    startTimerHz (30);
}

FinisherAudioProcessorEditor::~FinisherAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ui::Knob* FinisherAudioProcessorEditor::knob (const juce::String& id, const juce::String& label, juce::Colour c)
{
    auto k = std::make_unique<ui::Knob> (proc.apvts, id, label, c);
    auto* raw = k.get();
    knobById[id] = raw;
    owned.push_back (std::move (k));
    return raw;
}

ui::Choice* FinisherAudioProcessorEditor::choice (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<ui::Choice> (proc.apvts, id, label);
    auto* raw = c.get();
    owned.push_back (std::move (c));
    return raw;
}

ui::Switch* FinisherAudioProcessorEditor::toggle (const juce::String& id, const juce::String& label, juce::Colour c)
{
    auto s = std::make_unique<ui::Switch> (proc.apvts, id, label);
    s->setColour (juce::ToggleButton::tickColourId, c);
    auto* raw = s.get();
    owned.push_back (std::move (s));
    return raw;
}

//==============================================================================
void FinisherAudioProcessorEditor::buildPages()
{
    using Col = ui::Page::Column;
    auto addTo = [] (ui::Page& page, std::initializer_list<juce::Component*> comps) { for (auto* c : comps) page.addAndMakeVisible (c); };

    // ---- Dyn-EQ --------------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> (utf8 ("DYNAMISCHER EQ"), col::eq);
        const auto c = col::eq;
        auto* hpS = toggle (hpOn, {}, c);
        auto band = [&] (const char* title, const char* f, const char* g, const char* q, const char* m, const char* t, const char* r) -> Col
        {
            std::vector<juce::Component*> row1 { knob (f, "FREQ", c), knob (g, "GAIN", c) };
            if (q) row1.push_back (knob (q, "Q", c));
            return Col { utf8 (title), { row1, { choice (m, "KANAL"), knob (t, "DYN THR", c), knob (r, "DYN RANGE", c) } }, 3.0f };
        };
        std::vector<Col> cols {
            Col { "LOW-CUT", { { knob (hpFreq, "FREQ", c) }, { choice (hpSlope, "STEILHEIT") } }, 1.3f, hpS },
            band ("LOW-SHELF", lsFreq, lsGain, nullptr, lsMode, lsThr, lsRng),
            band ("BELL 1", p1Freq, p1Gain, p1Q, p1Mode, p1Thr, p1Rng),
            band ("BELL 2", p2Freq, p2Gain, p2Q, p2Mode, p2Thr, p2Rng),
            band ("HIGH-SHELF", hsFreq, hsGain, nullptr, hsMode, hsThr, hsRng),
            Col { "DYNAMIK", { { knob (eqAtk, "ATTACK", c) }, { knob (eqRel, "RELEASE", c) } }, 1.0f },
            Col { "INPUT", { { knob (inGain, "GAIN", col::text) }, {} }, 1.0f } };
        for (auto& cc : cols) { for (auto& row : cc.rows) for (auto* x : row) if (x) pg->addAndMakeVisible (x); if (cc.headerItem) pg->addAndMakeVisible (cc.headerItem); }
        pg->addAndMakeVisible (eqCurve);
        pg->setTop (&eqCurve, 92);
        pg->setHint (utf8 ("DYN RANGE ≠ 0 macht ein Band dynamisch: minus = absenken wenn es laut wird (Resonanzen), plus = anheben. KANAL: nur Mitte oder nur Seiten bearbeiten."));
        pg->setColumns (std::move (cols));
        pages[kEq] = std::move (pg);
    }

    // ---- Stereo --------------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("STEREO", col::stereo);
        const auto c = col::stereo;
        std::vector<Col> cols {
            Col { "BASS MONO", { { knob (monoFreq, "MONO UNTER", c) } }, 1.0f },
            Col { "MITTEN", { { knob (width, "BREITE", c) } }, 1.0f },
            Col { utf8 ("HÖHEN"), { { knob (widthXo, "AB FREQ", c), knob (widthHi, "BREITE", c) } }, 2.0f } };
        for (auto& cc : cols) for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x);
        stereoRight.addAndMakeVisible (corrMeter);
        pg->addAndMakeVisible (stereoRight);
        pg->setRight (&stereoRight, 380);
        pg->setHint (utf8 ("Korrelation unter 0 = Phasenprobleme, im Club droht Bass-Auslöschung. Mit „Mono“ oben rechts gegenhören."));
        pg->setColumns (std::move (cols));
        pages[kStereo] = std::move (pg);
    }

    // ---- Transient -----------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("TRANSIENT-SHAPER", col::trans);
        const auto c = col::trans;
        std::vector<Col> cols { Col { "TRANSIENTEN", { { knob (trAtk, "ATTACK", c), knob (trSus, "SUSTAIN", c) } }, 1.0f } };
        for (auto& cc : cols) for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x);
        transRight.addAndMakeVisible (trMeter);
        pg->addAndMakeVisible (transRight);
        pg->setRight (&transRight, 420);
        pg->setHint (utf8 ("Attack + = mehr Punch bei Kick/Snare, Sustain − = kürzere, trockenere Ausklänge. Pegelunabhängig."));
        pg->setColumns (std::move (cols));
        pages[kTrans] = std::move (pg);
    }

    // ---- Kompressor ----------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("KOMPRESSOR", col::comp);
        const auto c = col::comp;
        std::vector<Col> cols {
            Col { "PEGEL", { { knob (thresh, "THRESHOLD", c), knob (ratio, "RATIO", c), knob (knee, "KNEE", c) } }, 3.0f },
            Col { "ZEIT", { { knob (attack, "ATTACK", c), knob (release, "RELEASE", c) } }, 2.0f },
            Col { "AUSGANG", { { knob (makeup, "MAKEUP", c), knob (mix, "MIX", c) } }, 2.0f } };
        for (auto& cc : cols) for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x);
        compRight.addAndMakeVisible (compGr);
        pg->addAndMakeVisible (compRight);
        pg->setRight (&compRight, 50);
        pg->setHint (utf8 ("Glue-Kompressor. „Ziele anwenden“ setzt die Schwelle passend zum Track (sanft, ca. 1.5 dB) und mehr bei zu hoher LRA."));
        pg->setColumns (std::move (cols));
        pages[kComp] = std::move (pg);
    }

    // ---- Multiband -----------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("MULTIBAND-KOMPRESSOR", col::mb);
        const auto c = col::mb;
        std::vector<Col> cols { Col { "WEICHEN", { { knob (mbX1, "LOW / MID", c) }, { knob (mbX2, "MID / HIGH", c) } }, 1.0f } };
        const char* names[] = { "LOW", "MID", "HIGH" };
        for (int b = 0; b < 3; ++b)
        {
            Col bc { names[b], { { knob (mbParam (b, "Thr"), "THRESHOLD", c), knob (mbParam (b, "Ratio"), "RATIO", c), knob (mbParam (b, "Gain"), "GAIN", c) },
                                 { knob (mbParam (b, "Atk"), "ATTACK", c), knob (mbParam (b, "Rel"), "RELEASE", c),
                                   toggle (mbParam (b, "Solo"), "Solo", col::warn) } },
                     3.0f, toggle (mbParam (b, "Byp"), "Bypass", col::textDim) };
            bc.rows[1].push_back (&mbGr[b]);
            cols.push_back (std::move (bc));
        }
        for (auto& cc : cols) { for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x); if (cc.headerItem) pg->addAndMakeVisible (cc.headerItem); }
        pg->setHint (utf8 ("„Ziele anwenden“ misst jedes Band und setzt die Schwellen so, dass nur 1.5–2 dB geregelt wird. Solo = Band einzeln anhören."));
        pg->setColumns (std::move (cols));
        pages[kMb] = std::move (pg);
    }

    // ---- Saturation ----------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("SATURATION", col::sat);
        const auto c = col::sat;
        std::vector<Col> cols {
            Col { "MODELL", { { choice (satModel, "TYP") }, { toggle (satAuto, "Auto-Gain", c), toggle (satBass, "Bass sauber", c) } }, 1.6f },
            Col { utf8 ("SÄTTIGUNG"), { { knob (satDrive, "DRIVE", c), knob (satTone, "TON", c), knob (satMix, "MIX", c) } }, 3.0f } };
        for (auto& cc : cols) for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x);
        pg->setHint (utf8 ("Tape = rund, Tube = warm, Soft = sanft. „Bass sauber“: unter 150 Hz keine Sättigung (Kick/Sub bleiben klar). 4x oversampled."));
        pg->setColumns (std::move (cols));
        pages[kSat] = std::move (pg);
    }

    // ---- Clipper -------------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("CLIPPER", col::clip);
        const auto c = col::clip;
        std::vector<Col> cols { Col { "CLIPPING", { { knob (clipThr, utf8 ("SCHWELLE ÜBER CEILING"), c), knob (clipSoft, "WEICHHEIT", c) } }, 1.0f } };
        for (auto& cc : cols) for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x);
        clipRight.addAndMakeVisible (clipGr);
        pg->addAndMakeVisible (clipRight);
        pg->setRight (&clipRight, 50);
        pg->setHint (utf8 ("Kappt Kick/Snare-Spitzen vor dem Limiter → mehr Lautheit ohne Pumpen. Schwelle relativ zum Limiter-Ceiling, 4x oversampled."));
        pg->setColumns (std::move (cols));
        pages[kClip] = std::move (pg);
    }

    // ---- Limiter -------------------------------------------------------------
    {
        auto pg = std::make_unique<ui::Page> ("TRUE-PEAK-LIMITER", col::lim);
        const auto c = col::lim;
        std::vector<Col> cols {
            Col { "LAUTHEIT", { { knob (drive, "DRIVE", c), knob (ceiling, "CEILING", c) } }, 2.0f },
            Col { "VERHALTEN", { { knob (limRel, "RELEASE", c) } }, 1.0f },
            Col { "AUSGANG", { { knob (outGain, "OUTPUT TRIM", col::text) } }, 1.0f } };
        for (auto& cc : cols) for (auto& row : cc.rows) for (auto* x : row) pg->addAndMakeVisible (x);
        limRight.addAndMakeVisible (limGr);
        pg->addAndMakeVisible (limRight);
        pg->setRight (&limRight, 50);
        pg->setHint (utf8 ("Drive geht in Clipper + Limiter. Ceiling = maximaler True Peak. Club/DJ: −1 dBTP."));
        pg->setColumns (std::move (cols));
        pages[kLim] = std::move (pg);
    }

    for (auto& pg : pages) addChildComponent (*pg);
}

void FinisherAudioProcessorEditor::selectModule (int m)
{
    selected = m;
    for (int i = 0; i < kNumModules; ++i)
    {
        tiles[(size_t) i]->setSelected (i == m);
        pages[(size_t) i]->setVisible (i == m);
    }
}

//==============================================================================
void FinisherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::text);
    g.setFont (ui::font (24.0f, true));
    g.drawText ("FINISHER", 18, 8, 160, 34, juce::Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (ui::font (12.0f));
    g.drawText ("v" JucePlugin_VersionString, 136, 8, 50, 34, juce::Justification::centredLeft);

    // Signalfluss-Pfeile zwischen den Kacheln
    if (tiles[0] != nullptr)
    {
        g.setColour (col::textDim);
        for (int i = 0; i + 1 < kNumModules; ++i)
        {
            const auto a = tiles[(size_t) i]->getBounds(), b = tiles[(size_t) i + 1]->getBounds();
            const float y = (float) a.getCentreY(), x0 = (float) a.getRight() + 1.0f, x1 = (float) b.getX() - 1.0f;
            juce::Path arrow;
            arrow.addTriangle (x0, y - 4.0f, x0, y + 4.0f, x1, y);
            g.fillPath (arrow);
        }
    }

    ui::drawPanel (g, pHist, "VERLAUF");
    ui::drawPanel (g, pMeter, "LOUDNESS & ZIELE");
}

void FinisherAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Kopfzeile
    auto head = area.removeFromTop (38);
    monoBtn.setBounds (head.removeFromRight (70).reduced (0, 5));
    head.removeFromRight (6);
    deltaBtn.setBounds (head.removeFromRight (70).reduced (0, 5));
    head.removeFromRight (6);
    abBtn.setBounds (head.removeFromRight (100).reduced (0, 5));
    matchLabel.setBounds (head.removeFromRight (190));
    head.removeFromLeft (190);
    auto pb = head.reduced (0, 5);
    presetPrev.setBounds (pb.removeFromLeft (28));
    pb.removeFromLeft (4);
    presetBox.setBounds (pb.removeFromLeft (300));
    pb.removeFromLeft (4);
    presetNext.setBounds (pb.removeFromLeft (28));
    pb.removeFromLeft (8);
    presetSave.setBounds (pb.removeFromLeft (100));
    area.removeFromTop (6);

    // Modulleiste
    auto tileRow = area.removeFromTop (58);
    const int gap = 12;
    const int tw = (tileRow.getWidth() - gap * (kNumModules - 1)) / kNumModules;
    for (int m = 0; m < kNumModules; ++m)
    {
        tiles[(size_t) m]->setBounds (tileRow.removeFromLeft (tw));
        tileRow.removeFromLeft (gap);
    }
    area.removeFromTop (8);

    // Modulseite
    const auto pageArea = area.removeFromTop (345);
    for (auto& pg : pages) pg->setBounds (pageArea);
    stack (stereoRight, { { &corrMeter, 50 } });
    stack (transRight, { { &trMeter, 50 } });
    stack (compRight, { { &compGr, -1 } });
    stack (clipRight, { { &clipGr, -1 } });
    stack (limRight, { { &limGr, -1 } });
    area.removeFromTop (10);

    // Unten
    pHist = area.removeFromLeft (720);
    area.removeFromLeft (10);
    pMeter = area;
    history.setBounds (pHist.reduced (10).withTrimmedTop (22));

    auto b = pMeter.reduced (10).withTrimmedTop (22);
    auto row1 = b.removeFromTop (62);
    rdShort.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (2));
    rdInt.setBounds (row1.reduced (2));
    auto row2 = b.removeFromTop (54);
    const int w3 = row2.getWidth() / 3;
    rdMom.setBounds (row2.removeFromLeft (w3).reduced (2));
    rdLra.setBounds (row2.removeFromLeft (w3).reduced (2));
    rdTp.setBounds (row2.reduced (2));
    corrSmall.setBounds (b.removeFromTop (44).reduced (2, 2));
    auto row3 = b.removeFromTop (80);
    const int kw = row3.getWidth() / 3;
    for (auto* id : { tgtLufs, tgtTp, tgtLra })
    {
        knobById[id]->setBounds (row3.removeFromLeft (kw).reduced (2, 0));
    }
    b.removeFromTop (4);
    auto row4 = b.removeFromTop (28);
    resetBtn.setBounds (row4.removeFromLeft (row4.getWidth() / 2).reduced (2, 0));
    applyBtn.setBounds (row4.reduced (2, 0));
    b.removeFromTop (4);
    status.setBounds (b);
}

//==============================================================================
void FinisherAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("WERK-PRESETS");
    for (int i = 0; i < FinisherAudioProcessor::numFactoryPresets(); ++i)
        presetBox.addItem (FinisherAudioProcessor::factoryPresetName (i), i + 1);
    userPresetFiles = proc.userPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("EIGENE PRESETS");
        for (int j = 0; j < userPresetFiles.size(); ++j)
            presetBox.addItem (userPresetFiles[j].getFileNameWithoutExtension(), 1000 + j);
    }
    // aktuellen Namen anzeigen
    const auto current = proc.currentPresetName();
    for (int k = 0; k < presetBox.getNumItems(); ++k)
        if (presetBox.getItemText (k) == current) { presetBox.setSelectedItemIndex (k, juce::dontSendNotification); return; }
    presetBox.setText (current, juce::dontSendNotification);
}

void FinisherAudioProcessorEditor::loadPresetFromBox()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    if (id < 1000) proc.loadFactoryPreset (id - 1);
    else if (! proc.loadUserPreset (userPresetFiles[id - 1000])) { status.setText (utf8 ("Preset konnte nicht geladen werden."), juce::dontSendNotification); return; }
    status.setText (utf8 ("Preset „") + presetBox.getText() + utf8 ("“ geladen. Track komplett abspielen → „Ziele anwenden“."),
                    juce::dontSendNotification);
}

void FinisherAudioProcessorEditor::stepPreset (int dir)
{
    const int n = presetBox.getNumItems();
    if (n == 0) return;
    int idx = presetBox.getSelectedItemIndex();
    if (idx < 0) idx = dir > 0 ? -1 : 0;
    idx = (idx + dir + n) % n;
    presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
}

void FinisherAudioProcessorEditor::savePresetDialog()
{
    auto* aw = new juce::AlertWindow (utf8 ("Preset speichern"), utf8 ("Name für das eigene Preset:"), juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", proc.currentPresetName());
    aw->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        if (result != 1) return;
        const auto name = aw->getTextEditorContents ("name").trim();
        if (proc.saveUserPreset (name))
        {
            refreshPresetList();
            status.setText (utf8 ("Preset „") + name + utf8 ("“ gespeichert."), juce::dontSendNotification);
        }
        else
            status.setText (utf8 ("Speichern fehlgeschlagen (Name leer oder Ordner nicht beschreibbar)."), juce::dontSendNotification);
    }), true);
}

//==============================================================================
void FinisherAudioProcessorEditor::timerCallback()
{
    auto& e = proc.engine;
    const auto t = proc.readTargets();
    const auto s = proc.readSettings();

    const float st = e.shortTerm.load(), mo = e.momentary.load(), in = e.integrated.load();
    const float lra = e.lra.load(), tp = e.truePeakMax.load();

    rdShort.set (st, st <= -100.0f ? col::textDim : traffic (st - t.lufs, 1.0f, 3.0f));
    rdMom.set (mo, col::text);
    rdInt.set (in, in <= -100.0f ? col::textDim : traffic (in - t.lufs, 0.5f, 1.5f), toTarget (in, t.lufs, "LU"));
    const bool lraValid = e.measuredSeconds.load() >= 3.0f;
    rdLra.set (lraValid ? lra : -200.0f, lraValid ? traffic (lra - t.lra, 1.0f, 2.5f) : col::textDim,
               lraValid ? toTarget (lra, t.lra, "LU") : juce::String());
    rdTp.set (tp, tp <= -100.0f ? col::textDim : (tp <= t.truePeak + 0.05f ? col::accent : col::bad), toTarget (tp, t.truePeak, "dB"));

    const float corr = e.correlation.load();
    corrMeter.setValue (corr);
    corrSmall.setValue (corr);
    trMeter.setValue (e.trGain.load());
    compGr.setValue (e.compGr.load());
    limGr.setValue (e.limGr.load());
    clipGr.setValue (e.clipGr.load());
    float mbMin = 0.0f;
    for (int b = 0; b < 3; ++b) { const float v = e.mbGr[b].load(); mbGr[b].setValue (v); mbMin = juce::jmin (mbMin, v); }

    // Kachel-Werte
    float dynMax = 0.0f;
    for (int b = 0; b < fin::DynamicEq::kBands; ++b) { const float v = e.eqDyn[b].load(); if (std::abs (v) > std::abs (dynMax)) dynMax = v; }
    tiles[kEq]->setValueText (std::abs (dynMax) > 0.05f ? "Dyn " + juce::String (dynMax, 1) + " dB" : utf8 ("aktiv"));
    tiles[kStereo]->setValueText ("Korr " + juce::String (corr, 2));
    tiles[kTrans]->setValueText ((e.trGain.load() >= 0 ? "+" : "") + juce::String (e.trGain.load(), 1) + " dB");
    tiles[kComp]->setValueText (grText (e.compGr.load()));
    tiles[kMb]->setValueText (grText (mbMin));
    const char* models[] = { "Tape", "Tube", "Soft" };
    tiles[kSat]->setValueText (juce::String (models[juce::jlimit (0, 2, s.satModel)]) + " +" + juce::String (s.satDrive, 1) + " dB");
    tiles[kClip]->setValueText (e.clipGr.load() < -0.05f ? "Clip " + juce::String (e.clipGr.load(), 1) + " dB" : utf8 ("bereit"));
    tiles[kLim]->setValueText (grText (e.limGr.load()));

    // A/B-Abgleich
    const float m = e.matchDb.load();
    matchLabel.setText (s.abBypass ? utf8 ("A/B: Original, angeglichen um ") + (m >= 0 ? "+" : "") + juce::String (m, 1) + " dB"
                                   : (s.delta ? utf8 ("Delta: nur die Veränderung") : utf8 ("Abgleich A/B: ") + (m >= 0 ? "+" : "") + juce::String (m, 1) + " dB"),
                        juce::dontSendNotification);

    // Regler deaktivieren, wenn Modul aus
    auto enable = [this] (std::initializer_list<juce::String> ids, bool on) { for (auto& id : ids) if (auto it = knobById.find (id); it != knobById.end()) it->second->setEnabled (on); };
    enable ({ thresh, ratio, attack, release, knee, makeup, mix }, s.compOn);
    enable ({ drive, ceiling, limRel }, s.limOn);
    enable ({ monoFreq }, s.monoOn);
    enable ({ hpFreq }, s.hpOn && s.eqOn);
    enable ({ trAtk, trSus }, s.trOn);
    enable ({ satDrive, satTone, satMix }, s.satOn);
    enable ({ clipThr, clipSoft }, s.clipOn);
    enable ({ mbX1, mbX2 }, s.mbOn);

    if (! presetBox.isPopupActive() && presetBox.getText() != proc.currentPresetName())
        refreshPresetList();

    if (selected == kEq) eqCurve.refresh();
    history.repaint();
}
