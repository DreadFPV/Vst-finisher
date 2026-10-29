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
    static const std::vector<Preset> list {
        { "Neutral (Standard)", {} },

        { "Psy / Goa Club -9",
          { { "hpFreq", 25 }, { "hpSlope", 2 },
            { "p2Freq", 3200 }, { "p2Q", 1.5f }, { "p2Thr", -30 }, { "p2Rng", -2.5f },
            { "hsFreq", 12000 }, { "hsGain", 1.0f }, { "hsMode", 2 },
            { "monoOn", 1 }, { "monoFreq", 120 }, { "widthHi", 115 },
            { "compOn", 1 }, { "thresh", -14 }, { "ratio", 1.8f }, { "attack", 30 }, { "release", 200 },
            { "mbOn", 1 }, { "mbX1", 120 }, { "mbX2", 4000 },
            { "mb1Thr", -20 }, { "mb1Ratio", 2.5f }, { "mb1Atk", 30 }, { "mb1Rel", 150 },
            { "mb2Thr", -20 }, { "mb2Ratio", 1.5f },
            { "mb3Thr", -24 }, { "mb3Ratio", 2.0f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 0 }, { "satDrive", 3 }, { "satMix", 60 },
            { "clipOn", 1 }, { "clipThr", 1.0f }, { "clipSoft", 30 },
            { "drive", 2 }, { "ceiling", -1 }, { "limRel", 60 },
            { "tgtLufs", -9 }, { "tgtTp", -1 }, { "tgtLra", 6 } } },

        { "Full-On / Hi-Tech -8",
          { { "hpFreq", 25 }, { "hpSlope", 2 },
            { "p2Freq", 3500 }, { "p2Q", 1.5f }, { "p2Thr", -30 }, { "p2Rng", -3 },
            { "hsFreq", 12000 }, { "hsGain", 1.0f }, { "hsMode", 2 },
            { "monoOn", 1 }, { "monoFreq", 120 }, { "widthHi", 120 },
            { "trOn", 1 }, { "trAtk", 2 },
            { "compOn", 1 }, { "thresh", -14 }, { "ratio", 2.0f }, { "attack", 25 }, { "release", 160 },
            { "mbOn", 1 }, { "mbX1", 100 }, { "mbX2", 4000 },
            { "mb1Thr", -20 }, { "mb1Ratio", 3.0f }, { "mb1Atk", 20 }, { "mb1Rel", 120 },
            { "mb2Thr", -20 }, { "mb2Ratio", 1.6f },
            { "mb3Thr", -24 }, { "mb3Ratio", 2.0f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 0 }, { "satDrive", 4 }, { "satMix", 60 },
            { "clipOn", 1 }, { "clipThr", 1.5f }, { "clipSoft", 20 },
            { "drive", 3 }, { "ceiling", -1 }, { "limRel", 50 },
            { "tgtLufs", -8 }, { "tgtTp", -1 }, { "tgtLra", 5 } } },

        { "Dark / Forest -9",
          { { "hpFreq", 25 }, { "hpSlope", 2 },
            { "p1Freq", 300 }, { "p1Gain", -1.0f }, { "p1Q", 0.8f },
            { "p2Freq", 3000 }, { "p2Q", 1.2f }, { "p2Thr", -32 }, { "p2Rng", -3 },
            { "monoOn", 1 }, { "monoFreq", 120 }, { "widthHi", 110 },
            { "compOn", 1 }, { "thresh", -14 }, { "ratio", 1.8f }, { "attack", 30 }, { "release", 200 },
            { "mbOn", 1 }, { "mbX1", 120 }, { "mbX2", 3500 },
            { "mb1Thr", -20 }, { "mb1Ratio", 2.5f }, { "mb1Atk", 30 }, { "mb1Rel", 150 },
            { "mb3Thr", -26 }, { "mb3Ratio", 2.0f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 1 }, { "satDrive", 3 }, { "satTone", -20 }, { "satMix", 50 },
            { "clipOn", 1 }, { "clipThr", 1.0f }, { "clipSoft", 30 },
            { "drive", 2 }, { "ceiling", -1 },
            { "tgtLufs", -9 }, { "tgtTp", -1 }, { "tgtLra", 6 } } },

        { "DnB / Neurofunk -8",
          { { "hpFreq", 30 }, { "hpSlope", 2 },
            { "p2Freq", 2800 }, { "p2Q", 1.3f }, { "p2Thr", -28 }, { "p2Rng", -3 },
            { "hsFreq", 10000 }, { "hsGain", 1.0f },
            { "monoOn", 1 }, { "monoFreq", 150 }, { "widthHi", 120 },
            { "trOn", 1 }, { "trAtk", 3 }, { "trSus", -1 },
            { "compOn", 1 }, { "thresh", -12 }, { "ratio", 2.0f }, { "attack", 10 }, { "release", 100 },
            { "mbOn", 1 }, { "mbX1", 150 }, { "mbX2", 4500 },
            { "mb1Thr", -18 }, { "mb1Ratio", 3.0f }, { "mb1Atk", 10 }, { "mb1Rel", 100 },
            { "mb2Thr", -18 }, { "mb2Ratio", 1.8f }, { "mb2Atk", 10 }, { "mb2Rel", 90 },
            { "mb3Thr", -24 }, { "mb3Ratio", 2.0f }, { "mb3Atk", 3 }, { "mb3Rel", 60 },
            { "satOn", 1 }, { "satModel", 1 }, { "satDrive", 4 }, { "satMix", 50 },
            { "clipOn", 1 }, { "clipThr", 1.0f }, { "clipSoft", 15 },
            { "drive", 3 }, { "ceiling", -1 }, { "limRel", 40 },
            { "tgtLufs", -8 }, { "tgtTp", -1 }, { "tgtLra", 5 } } },

        { "Brazil Bounce / Bass -8",
          { { "hpFreq", 28 }, { "hpSlope", 2 },
            { "lsFreq", 70 }, { "lsGain", 1.0f },
            { "p2Freq", 3000 }, { "p2Q", 1.4f }, { "p2Thr", -30 }, { "p2Rng", -2.5f },
            { "monoOn", 1 }, { "monoFreq", 140 }, { "widthHi", 115 },
            { "trOn", 1 }, { "trAtk", 2 },
            { "compOn", 1 }, { "thresh", -13 }, { "ratio", 2.0f }, { "attack", 20 }, { "release", 150 },
            { "mbOn", 1 }, { "mbX1", 130 }, { "mbX2", 4000 },
            { "mb1Thr", -19 }, { "mb1Ratio", 2.5f }, { "mb1Atk", 20 }, { "mb1Rel", 120 },
            { "mb3Thr", -24 }, { "mb3Ratio", 2.0f }, { "mb3Atk", 5 }, { "mb3Rel", 80 },
            { "satOn", 1 }, { "satModel", 2 }, { "satDrive", 3 }, { "satMix", 60 },
            { "clipOn", 1 }, { "clipThr", 1.0f }, { "clipSoft", 25 },
            { "drive", 3 }, { "ceiling", -1 }, { "limRel", 50 },
            { "tgtLufs", -8 }, { "tgtTp", -1 }, { "tgtLra", 6 } } },

        { "Reggae / Chill -12",
          { { "hpFreq", 25 }, { "hpSlope", 1 },
            { "lsFreq", 80 }, { "lsGain", 1.0f },
            { "hsFreq", 10000 }, { "hsGain", 0.5f },
            { "monoOn", 1 }, { "monoFreq", 110 },
            { "compOn", 1 }, { "thresh", -16 }, { "ratio", 1.5f }, { "attack", 40 }, { "release", 300 },
            { "satOn", 1 }, { "satModel", 0 }, { "satDrive", 2 }, { "satMix", 50 },
            { "drive", 0 }, { "ceiling", -1 }, { "limRel", 100 },
            { "tgtLufs", -12 }, { "tgtTp", -1 }, { "tgtLra", 8 } } },

        { "Streaming -14 (Spotify/YouTube)",
          { { "hpFreq", 25 }, { "hpSlope", 1 },
            { "monoOn", 1 }, { "monoFreq", 110 },
            { "compOn", 1 }, { "thresh", -18 }, { "ratio", 1.5f }, { "attack", 30 }, { "release", 250 },
            { "drive", 0 }, { "ceiling", -1 }, { "limRel", 100 },
            { "tgtLufs", -14 }, { "tgtTp", -1 }, { "tgtLra", 8 } } },

        { "Nur Messen (alles aus)",
          { { "eqOn", 0 }, { "monoOn", 0 }, { "limOn", 0 }, { "drive", 0 } } },
    };
    return list;
}
} // namespace presets
