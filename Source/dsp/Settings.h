#pragma once

namespace fin
{
/** Alle Einstellungen der Signalkette. Reihenfolge der Kette:
    Input -> EQ -> Mono-Bass/Width -> Kompressor -> Limiter (Drive + True-Peak-Ceiling) -> Output-Trim */
struct Settings
{
    // Input
    float inGainDb = 0.0f;

    // EQ
    bool  eqOn   = true;
    bool  hpOn   = true;
    float hpFreq = 25.0f;                       // Low-Cut 24 dB/Okt
    float lsFreq = 80.0f,   lsGain = 0.0f;      // Low-Shelf
    float p1Freq = 250.0f,  p1Gain = 0.0f, p1Q = 1.0f;
    float p2Freq = 2500.0f, p2Gain = 0.0f, p2Q = 1.0f;
    float hsFreq = 10000.0f, hsGain = 0.0f;     // High-Shelf

    // Mono-Bass / Stereo
    bool  monoOn   = true;
    float monoFreq = 120.0f;                    // darunter mono (Linkwitz-Riley 24 dB/Okt)
    float width    = 100.0f;                    // Stereobreite oberhalb, in %

    // Kompressor
    bool  compOn   = false;
    float thresh   = -12.0f, ratio = 2.0f, attack = 30.0f, release = 150.0f;
    float knee     = 6.0f,   makeup = 0.0f, mix = 100.0f;

    // Limiter
    bool  limOn      = true;
    float drive      = 0.0f;                    // Gain in den Limiter
    float ceiling    = -1.0f;                   // dBTP
    float limRelease = 60.0f;                   // ms

    // Output
    float outGainDb = 0.0f;                     // nur absenken (-12..0), damit das Ceiling hält
};
} // namespace fin
