# Finisher

Mastering-Strip als **ein** Plugin (VST3 / AU / Standalone) für Cubase, Ableton & Co.
Ziel: Tracks club- und DJ-tauglich fertig machen – mit Zielwerten für LUFS, True Peak und Loudness Range.

**Kette:** Input → Dyn-EQ → Stereo → Transient → Kompressor → Multiband → Drive → Saturation → Clipper → True-Peak-Limiter → Output-Trim

Oben eine **Modulleiste**: Kachel anklicken öffnet die Seite des Moduls, Schalter in der Kachel = Ein/Aus, darunter der Live-Wert (GR, Korrelation …). Verlauf und Loudness-Meter bleiben immer sichtbar.

## Module

| Modul | Funktion |
|---|---|
| **Dyn-EQ** | Low-Cut 12/24/48 dB/Okt, Low-Shelf, 2× Glocke, High-Shelf. Jedes Band: **Stereo / Mid / Side** und optional **dynamisch** (Threshold + Range: minus = absenken wenn laut, plus = anheben). Kurve zeigt eingestellt + aktuell. |
| **Stereo** | Bass mono unter X Hz, Breite für Mitten und Höhen getrennt (3 Bänder, phasenneutral), **Korrelationsmesser** |
| **Transient** | Attack / Sustain ±12 dB, pegelunabhängig |
| **Kompressor** | Stereo-gelinkt, Threshold, Ratio, Attack, Release, Knee, Makeup, Parallel-Mix |
| **Multiband** | 3 Bänder (LR4-Weichen, phasenneutral), je Threshold/Ratio/Attack/Release/Gain, Solo, Bypass, GR-Anzeige |
| **Saturation** | Tape / Tube / Soft, Drive, Ton (dunkel–hell), Mix, Auto-Gain, **4× oversampled** |
| **Clipper** | Kappt Spitzen vor dem Limiter (Schwelle relativ zum Ceiling, Weichheit), **4× oversampled** |
| **Limiter** | Lookahead 1.5 ms, **True-Peak-Erkennung 4×**, Drive, Ceiling (dBTP), Release, Output-Trim |
| **Messung** | ITU-R BS.1770-4 / EBU R128: Momentary, Short-Term, Integrated, LRA, True Peak Max, Korrelation |
| **Verlauf** | Waveform In/Out, Short-Term-LUFS-Kurve, Ziel-Linie, Limiter-GR |
| **Ziele** | Ziel-LUFS / True Peak / LRA → „Ziele anwenden“ |

**Abhören (oben rechts):**
- **A/B Bypass** – Original, automatisch auf gleiche Lautheit gebracht (ehrlicher Vergleich, lauter klingt nicht automatisch besser)
- **Delta** – nur das hören, was der Finisher verändert
- **Mono** – Club-Check

Latenz: 136 Samples @ 48 kHz (2.8 ms), konstant – wird automatisch an die DAW gemeldet.

## Ziel-Automatik – so geht's

1. Zielwerte einstellen (z. B. Club: **−9 LUFS**, **−1 dBTP**, **LRA 6–8 LU**).
2. **Messung zurücksetzen**, Track **komplett** abspielen.
3. **Ziele anwenden** → Drive (LUFS), Ceiling (True Peak) und Kompressor (LRA) werden gesetzt.
4. Messung startet neu → Track nochmal abspielen. Nach 2–3 Durchgängen sitzt es (Anzeigen grün).

Hinweis: LRA wird über den ganzen Track gemessen, darum geht das nur in Durchgängen, nicht live.
Alles bleibt jederzeit manuell verstellbar.

Ampelfarben: grün = im Ziel, gelb = knapp daneben, rot = deutlich daneben / True Peak über Ziel.

## Download / Installation

Jeder Push baut automatisch (GitHub → **Actions** → letzter Lauf → *Artifacts*):
`Finisher-macOS.zip` (Universal, Apple Silicon + Intel) und `Finisher-Windows.zip`.
Bei einem Tag `v*` (z. B. `v0.2.0`) wird zusätzlich ein Release erstellt.

**macOS**
- `Finisher.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- `Finisher.component` → `~/Library/Audio/Plug-Ins/Components/` (Ableton, Logic)
- Da das Plugin nicht notarisiert ist, einmal im Terminal:
  ```
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Finisher.vst3
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Finisher.component
  ```
- Cubase: *Studio → VST-Plug-in-Manager → Plug-ins neu scannen*.

**Windows**
- `Finisher.vst3` (Ordner) → `C:\Program Files\Common Files\VST3\`

## Selbst bauen

Voraussetzung: CMake ≥ 3.22, C++17-Compiler (Xcode / Visual Studio 2022). JUCE 8 wird automatisch geladen.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release            # macOS: -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

Ergebnis: `build/Finisher_artefacts/Release/{VST3,AU,Standalone}`

## CLI (offline messen / mastern)

Gleiche Engine wie das Plugin, ohne DAW:

```bash
finisher-cli track.wav                                   # nur messen
finisher-cli track.wav master.wav --lufs -9 --tp -1 --lra 7   # Ziel-Automatik, schreibt 32-bit-float-WAV
finisher-cli track.wav master.wav --drive 3 --ceiling -1      # manuell
```

## Tests (Entwicklung)

`ctest -C Release` startet den DSP-Selbsttest (Null-Tests der Kette, A/B, Delta, phasenneutrale Weichen, Mono-Bass, Clipper, Dyn-EQ, True Peak bei verschiedenen Blockgrößen) – läuft auch in GitHub Actions.

`-DFINISHER_BUILD_HOSTTEST=ON` baut zwei Tools:
- `finisher-hosttest` – lädt das VST3, prüft Latenz, Verarbeitung, variable Blockgrößen, State-Roundtrip
- `finisher-snapshot` – verarbeitet ein WAV und rendert alle Modulseiten als PNG

Messgenauigkeit geprüft gegen `ffmpeg ebur128` (Integrated ±0.1 LU, LRA ±0.1 LU, True Peak ±0.05 dB).

---
© JustCreateit.ch · Just DereadBeats
