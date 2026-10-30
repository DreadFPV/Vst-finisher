#pragma once

namespace fin
{
/** Kanal-Routing für EQ-Bänder. */
enum ChannelMode { kStereo = 0, kMid = 1, kSide = 2 };

/** EQ-Band mit optionaler Dynamik: Bei range != 0 reagiert das Band auf den Pegel im Band.
    range < 0: absenken, wenn es laut wird (z. B. Resonanzen zähmen)
    range > 0: anheben, wenn es laut wird (z. B. Kick-Attack betonen) */
struct EqBand
{
    float freq = 1000.0f, gain = 0.0f, q = 1.0f;
    int   mode = kStereo;
    float thresh = -20.0f, range = 0.0f;
};

struct MbBand
{
    float thresh = -18.0f, ratio = 2.0f, attack = 20.0f, release = 150.0f, gain = 0.0f;
    bool  solo = false, bypass = false;
};

/** Alle Einstellungen der Signalkette. Feste Reihenfolge:
    Input -> Dyn-EQ -> Stereo -> Transient -> Kompressor -> Multiband -> Drive -> [4x: Saturation -> Clipper]
    -> True-Peak-Limiter -> Output-Trim -> Monitoring (A/B, Delta, Mono) */
struct Settings
{
    // Input
    float inGainDb = 0.0f;

    // Dynamischer EQ
    bool  eqOn    = true;
    bool  hpOn    = true;
    float hpFreq  = 25.0f;
    int   hpSlope = 1;                          // 0 = 12, 1 = 24, 2 = 48 dB/Okt
    EqBand ls { 80.0f,    0.0f, 0.71f, kStereo, -20.0f, 0.0f };
    EqBand p1 { 250.0f,   0.0f, 1.0f,  kStereo, -20.0f, 0.0f };
    EqBand p2 { 2500.0f,  0.0f, 1.0f,  kStereo, -20.0f, 0.0f };
    EqBand hs { 10000.0f, 0.0f, 0.71f, kStereo, -20.0f, 0.0f };
    float eqAttack = 10.0f, eqRelease = 120.0f;

    // Stereo (3 Bänder)
    bool  monoOn    = true;
    float monoFreq  = 120.0f;                   // darunter mono
    float width     = 100.0f;                   // Breite Mitten (%)
    float widthHigh = 100.0f;                   // Breite Höhen (%)
    float widthXover = 4000.0f;                 // Übergang Mitten/Höhen

    // Transient-Shaper
    bool  trOn = false;
    float trAttack = 0.0f, trSustain = 0.0f;    // dB, -12 ... +12

    // Kompressor
    bool  compOn   = false;
    float thresh   = -6.0f,  ratio = 2.0f, attack = 30.0f, release = 150.0f;
    float knee     = 6.0f,   makeup = 0.0f, mix = 100.0f;

    // Multiband-Kompressor
    bool  mbOn = false;
    float mbX1 = 150.0f, mbX2 = 3000.0f;
    // Schwellen bewusst hoch (sanft) – "Ziele anwenden" kalibriert sie auf den Track
    MbBand mb[3] { { -8.0f, 2.0f, 30.0f, 200.0f, 0.0f },
                   { -8.0f, 1.5f, 15.0f, 120.0f, 0.0f },
                   { -8.0f, 1.8f, 5.0f,  80.0f,  0.0f } };

    // Saturation (4x oversampled)
    bool  satOn = false;
    int   satModel = 0;                         // 0 Tape, 1 Tube, 2 Soft
    float satDrive = 3.0f, satTone = 0.0f, satMix = 100.0f;
    bool  satAuto = true;
    bool  satBassFree = true;                   // Bass unter 150 Hz nicht sättigen

    // Clipper (4x oversampled), Schwelle relativ zum Limiter-Ceiling
    bool  clipOn = false;
    float clipThresh = 2.0f;                    // dB über dem Ceiling
    float clipSoft = 50.0f;                     // %

    // Limiter
    bool  limOn      = true;
    float drive      = 0.0f;                    // Gain in Clipper + Limiter
    float ceiling    = -1.0f;                   // dBTP
    float limRelease = 120.0f;                  // ms (zu schnell = Verzerrung im Bass)

    // Output
    float outGainDb = 0.0f;                     // nur absenken (-12..0)

    // Monitoring (nur Abhören, Messung bleibt auf dem bearbeiteten Signal)
    bool abBypass = false;                      // lautheitsgleicher Bypass
    bool delta = false;                         // nur die Veränderung hören
    bool monoListen = false;
};
} // namespace fin
