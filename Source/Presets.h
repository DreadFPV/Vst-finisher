#pragma once

#include <vector>
#include <utility>

/** Werks-Presets: Startpunkte pro Genre. Nicht aufgeführte Parameter = Standardwert.
    Drive ist bewusst moderat – die Lautheit stellt danach "Ziele anwenden" passend zum Track ein. */
namespace presets
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& factory()
{
    // Kompressor-/Multiband-Schwellen sind absichtlich NICHT gesetzt: "Ziele anwenden" misst den Track
    // und kalibriert sie (ca. 1.5-2 dB Regelung). Bis dahin arbeiten sie sehr sanft.
    static const std::vector<Preset> list {
        { "Neutral (Standard)", {} },

        { "Psy / Goa Club -9",
          { { "hpFreq", 25 }, { "hpSlope", 2 },
            { "p2Freq", 3200 }, { "p2Q", 1.5f }, { "p2Thr", -24 }, { "p2Rng", -2 },
            { "hsFreq", 12000 }, { "hsGain", 0.8f }, { "hsMode", 2 },
            { "monoOn", 1 }, { "monoFreq", 120 }, { "widthHi", 110 },
            { "compOn", 1 }, { "ratio", 1.8f }, { "attack", 30 }, { "release", 200 },
            { "mbOn", 1 }, { "mbX1", 120 }, { "mbX2", 4000 },
            { "mb1Ratio", 2.0f }, { "mb1Atk", 40 }, { "mb1Rel", 200 },     // langsamer Attack: Kick-Transient bleibt
            { "mb2Ratio", 1.5f }, { "mb3Ratio", 1.8f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 0 }, { "satDrive", 2 }, { "satMix", 40 }, { "satBassFree", 1 },
            { "drive", 1 }, { "ceiling", -1 }, { "limRel", 120 },
            { "tgtLufs", -9 }, { "tgtTp", -1 }, { "tgtLra", 6 } } },

        { "Full-On / Hi-Tech -8",
          { { "hpFreq", 25 }, { "hpSlope", 2 },
            { "p2Freq", 3500 }, { "p2Q", 1.5f }, { "p2Thr", -24 }, { "p2Rng", -2.5f },
            { "hsFreq", 12000 }, { "hsGain", 1.0f }, { "hsMode", 2 },
            { "monoOn", 1 }, { "monoFreq", 120 }, { "widthHi", 115 },
            { "trOn", 1 }, { "trAtk", 1.5f },
            { "compOn", 1 }, { "ratio", 2.0f }, { "attack", 30 }, { "release", 180 },
            { "mbOn", 1 }, { "mbX1", 110 }, { "mbX2", 4000 },
            { "mb1Ratio", 2.0f }, { "mb1Atk", 40 }, { "mb1Rel", 180 },
            { "mb2Ratio", 1.5f }, { "mb3Ratio", 1.8f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 0 }, { "satDrive", 2.5f }, { "satMix", 45 }, { "satBassFree", 1 },
            { "clipOn", 1 }, { "clipThr", 2.0f }, { "clipSoft", 60 },
            { "drive", 2 }, { "ceiling", -1 }, { "limRel", 100 },
            { "tgtLufs", -8 }, { "tgtTp", -1 }, { "tgtLra", 5 } } },

        { "Dark / Forest -9",
          { { "hpFreq", 25 }, { "hpSlope", 2 },
            { "p1Freq", 300 }, { "p1Gain", -1.0f }, { "p1Q", 0.8f },
            { "p2Freq", 3000 }, { "p2Q", 1.2f }, { "p2Thr", -24 }, { "p2Rng", -2.5f },
            { "monoOn", 1 }, { "monoFreq", 120 }, { "widthHi", 110 },
            { "compOn", 1 }, { "ratio", 1.8f }, { "attack", 30 }, { "release", 200 },
            { "mbOn", 1 }, { "mbX1", 120 }, { "mbX2", 3500 },
            { "mb1Ratio", 2.0f }, { "mb1Atk", 40 }, { "mb1Rel", 200 },
            { "mb2Ratio", 1.5f }, { "mb3Ratio", 1.8f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 1 }, { "satDrive", 2 }, { "satTone", -20 }, { "satMix", 40 }, { "satBassFree", 1 },
            { "drive", 1 }, { "ceiling", -1 }, { "limRel", 120 },
            { "tgtLufs", -9 }, { "tgtTp", -1 }, { "tgtLra", 6 } } },

        { "DnB / Neurofunk -8",
          { { "hpFreq", 30 }, { "hpSlope", 2 },
            { "p2Freq", 2800 }, { "p2Q", 1.3f }, { "p2Thr", -24 }, { "p2Rng", -2.5f },
            { "hsFreq", 10000 }, { "hsGain", 0.8f },
            { "monoOn", 1 }, { "monoFreq", 150 }, { "widthHi", 115 },
            { "trOn", 1 }, { "trAtk", 1.5f },
            { "compOn", 1 }, { "ratio", 2.0f }, { "attack", 20 }, { "release", 120 },
            { "mbOn", 1 }, { "mbX1", 150 }, { "mbX2", 4500 },
            { "mb1Ratio", 2.0f }, { "mb1Atk", 30 }, { "mb1Rel", 150 },
            { "mb2Ratio", 1.6f }, { "mb2Atk", 15 }, { "mb2Rel", 100 },
            { "mb3Ratio", 1.8f }, { "mb3Atk", 3 }, { "mb3Rel", 60 },
            { "satOn", 1 }, { "satModel", 1 }, { "satDrive", 2.5f }, { "satMix", 40 }, { "satBassFree", 1 },
            { "clipOn", 1 }, { "clipThr", 2.0f }, { "clipSoft", 60 },
            { "drive", 2 }, { "ceiling", -1 }, { "limRel", 100 },
            { "tgtLufs", -8 }, { "tgtTp", -1 }, { "tgtLra", 5 } } },

        { "Brazil Bounce / Bass -8",
          { { "hpFreq", 28 }, { "hpSlope", 2 },
            { "lsFreq", 70 }, { "lsGain", 0.8f },
            { "p2Freq", 3000 }, { "p2Q", 1.4f }, { "p2Thr", -24 }, { "p2Rng", -2 },
            { "monoOn", 1 }, { "monoFreq", 140 }, { "widthHi", 110 },
            { "trOn", 1 }, { "trAtk", 1.0f },
            { "compOn", 1 }, { "ratio", 2.0f }, { "attack", 30 }, { "release", 150 },
            { "mbOn", 1 }, { "mbX1", 130 }, { "mbX2", 4000 },
            { "mb1Ratio", 2.0f }, { "mb1Atk", 40 }, { "mb1Rel", 180 },
            { "mb2Ratio", 1.5f }, { "mb3Ratio", 1.8f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 2 }, { "satDrive", 2 }, { "satMix", 40 }, { "satBassFree", 1 },
            { "drive", 2 }, { "ceiling", -1 }, { "limRel", 110 },
            { "tgtLufs", -8 }, { "tgtTp", -1 }, { "tgtLra", 6 } } },

        { "Reggae / Chill -12",
          { { "hpFreq", 25 }, { "hpSlope", 1 },
            { "lsFreq", 80 }, { "lsGain", 0.8f },
            { "hsFreq", 10000 }, { "hsGain", 0.5f },
            { "monoOn", 1 }, { "monoFreq", 110 },
            { "compOn", 1 }, { "ratio", 1.5f }, { "attack", 40 }, { "release", 300 },
            { "satOn", 1 }, { "satModel", 0 }, { "satDrive", 1.5f }, { "satMix", 40 }, { "satBassFree", 1 },
            { "drive", 0 }, { "ceiling", -1 }, { "limRel", 150 },
            { "tgtLufs", -12 }, { "tgtTp", -1 }, { "tgtLra", 8 } } },

        { "Streaming -14 (Spotify/YouTube)",
          { { "hpFreq", 25 }, { "hpSlope", 1 },
            { "monoOn", 1 }, { "monoFreq", 110 },
            { "compOn", 1 }, { "ratio", 1.5f }, { "attack", 30 }, { "release", 250 },
            { "drive", 0 }, { "ceiling", -1 }, { "limRel", 150 },
            { "tgtLufs", -14 }, { "tgtTp", -1 }, { "tgtLra", 8 } } },

        { "Nur Messen (alles aus)",
          { { "eqOn", 0 }, { "monoOn", 0 }, { "limOn", 0 }, { "drive", 0 } } },
    };
    return list;
}
} // namespace presets
