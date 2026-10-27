# Finisher

Mastering-Strip als **ein** Plugin (VST3 / AU / Standalone) für Cubase, Ableton & Co.
Ziel: Tracks club- und DJ-tauglich fertig machen – mit Zielwerten für LUFS, True Peak und Loudness Range.

**Kette:** Input → EQ → Mono-Bass / Breite → Kompressor → True-Peak-Limiter → Output-Trim

## Module

| Modul | Funktion |
|---|---|
| **EQ** | Low-Cut 24 dB/Okt, Low-Shelf, 2× Glocke (Freq/Gain/Q), High-Shelf, Frequenzgang-Anzeige |
| **Mono-Bass** | Alles unter der eingestellten Frequenz mono (Linkwitz-Riley 24 dB/Okt, phasenneutral summiert), Stereobreite darüber 0–200 % |
| **Kompressor** | Stereo-gelinkt, Threshold, Ratio, Attack, Release, Soft-Knee, Makeup, Parallel-Mix, GR-Anzeige |
| **Limiter** | Lookahead 1.5 ms, **True-Peak-Erkennung 4× oversampled**, Drive, Ceiling (dBTP), Release, GR-Anzeige |
| **Messung** | Nach ITU-R BS.1770-4 / EBU R128: Momentary, Short-Term, Integrated, LRA, True Peak Max |
| **Verlauf** | Waveform In (grau) / Out (grün), Short-Term-LUFS-Kurve, Ziel-Linie, Limiter-GR (rot) |
| **Ziele** | Ziel-LUFS, Ziel-True-Peak, Ziel-LRA → „Ziele anwenden“ stellt Drive, Ceiling und Kompressor ein |

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
Bei einem Tag `v*` (z. B. `v0.1.0`) wird zusätzlich ein Release erstellt.

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

`-DFINISHER_BUILD_HOSTTEST=ON` baut zwei Tools:
- `finisher-hosttest` – lädt das VST3, prüft Latenz, Verarbeitung, variable Blockgrößen, State-Roundtrip
- `finisher-snapshot` – verarbeitet ein WAV und rendert die GUI als PNG

Messgenauigkeit geprüft gegen `ffmpeg ebur128` (Integrated ±0.1 LU, LRA ±0.1 LU, True Peak ±0.05 dB).

---
© JustCreateit.ch · Just DereadBeats
