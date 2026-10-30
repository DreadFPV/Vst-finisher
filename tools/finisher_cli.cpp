// Finisher CLI - misst und mastert WAV-Dateien offline mit derselben Engine wie das Plugin.
//
//   finisher-cli in.wav                       -> nur messen (LUFS, LRA, True Peak)
//   finisher-cli in.wav out.wav [Optionen]    -> mastern und als 32-bit-float-WAV schreiben
//
// Optionen:
//   --lufs X --tp Y --lra Z   Zielwerte (Ziel-Automatik, mehrere Durchgaenge)
//   --passes N                Durchgaenge der Ziel-Automatik (Standard 3)
//   --drive D --ceiling C     manuell (ohne Ziele)
//   --mono F                  Bass-Mono-Frequenz (0 = aus)
//   --full                    alle Module an (Test/CPU)

#include "../Source/dsp/Engine.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
struct Audio
{
    double fs = 48000.0;
    std::vector<float> L, R;
};

template <typename T> T rd (const char* p) { T v; std::memcpy (&v, p, sizeof (T)); return v; }

bool readWav (const std::string& path, Audio& a)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    std::vector<char> d ((std::istreambuf_iterator<char> (f)), {});
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) return false;
    size_t pos = 12;
    int fmt = 0, ch = 0, bits = 0;
    const char* data = nullptr; size_t dataLen = 0;
    while (pos + 8 <= d.size())
    {
        const uint32_t len = rd<uint32_t> (d.data() + pos + 4);
        const char* body = d.data() + pos + 8;
        if (! std::memcmp (d.data() + pos, "fmt ", 4))
        {
            fmt = rd<uint16_t> (body); ch = rd<uint16_t> (body + 2);
            a.fs = rd<uint32_t> (body + 4); bits = rd<uint16_t> (body + 14);
            if (fmt == 0xFFFE) fmt = rd<uint16_t> (body + 24);
        }
        else if (! std::memcmp (d.data() + pos, "data", 4))
        {
            data = body; dataLen = std::min<size_t> (len, d.size() - pos - 8);
        }
        pos += 8 + len + (len & 1);
    }
    if (! data || ch < 1) return false;
    const int bps = bits / 8;
    const size_t frames = dataLen / (size_t) (bps * ch);
    a.L.resize (frames); a.R.resize (frames);
    for (size_t i = 0; i < frames; ++i)
    {
        float v[2] {};
        for (int c = 0; c < std::min (ch, 2); ++c)
        {
            const char* p = data + (i * ch + c) * bps;
            if (fmt == 3 && bits == 32)      v[c] = rd<float> (p);
            else if (fmt == 3 && bits == 64) v[c] = (float) rd<double> (p);
            else if (bits == 16)             v[c] = rd<int16_t> (p) / 32768.0f;
            else if (bits == 24)             v[c] = (float) ((int32_t) ((uint8_t) p[0] << 8 | (uint8_t) p[1] << 16 | (uint32_t) (uint8_t) p[2] << 24) >> 8) / 8388608.0f;
            else if (bits == 32)             v[c] = rd<int32_t> (p) / 2147483648.0f;
            else return false;
        }
        a.L[i] = v[0]; a.R[i] = ch > 1 ? v[1] : v[0];
    }
    return true;
}

bool writeWav (const std::string& path, const Audio& a)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) return false;
    const uint32_t frames = (uint32_t) a.L.size(), dataLen = frames * 8;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataLen); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (16); w16 (3); w16 (2); w32 ((uint32_t) a.fs); w32 ((uint32_t) a.fs * 8); w16 (8); w16 (32);
    f.write ("data", 4); w32 (dataLen);
    for (uint32_t i = 0; i < frames; ++i) { f.write ((const char*) &a.L[i], 4); f.write ((const char*) &a.R[i], 4); }
    return true;
}

/** Läuft die Engine über das ganze File. Gibt die Messung zurück; out (optional) bekommt das Ergebnis ohne Latenz. */
fin::Measurement run (const Audio& in, const fin::Settings& s, Audio* out)
{
    fin::Engine e;
    e.setSettings (s);
    e.prepare (in.fs, 512);
    e.setSettings (s);
    const int lat = e.latency();
    const size_t total = in.L.size() + (size_t) lat;
    std::vector<float> L (512), R (512);
    if (out) { out->fs = in.fs; out->L.clear(); out->R.clear(); out->L.reserve (in.L.size()); out->R.reserve (in.L.size()); }
    size_t written = 0;
    for (size_t pos = 0; pos < total; pos += 512)
    {
        const int n = (int) std::min<size_t> (512, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const size_t k = pos + (size_t) i;
            L[(size_t) i] = k < in.L.size() ? in.L[k] : 0.0f;
            R[(size_t) i] = k < in.R.size() ? in.R[k] : 0.0f;
        }
        e.process (L.data(), R.data(), n);
        if (out)
            for (int i = 0; i < n; ++i)
            {
                const size_t k = pos + (size_t) i;
                if (k >= (size_t) lat && written < in.L.size()) { out->L.push_back (L[(size_t) i]); out->R.push_back (R[(size_t) i]); ++written; }
            }
    }
    return e.measurement();
}

void print (const char* label, const fin::Measurement& m)
{
    std::printf ("%-10s I: %6.1f LUFS   LRA: %4.1f LU   TP: %5.2f dBTP   (%.0f s)\n",
                 label, m.integrated, m.lra, m.truePeak, m.seconds);
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "Usage: finisher-cli in.wav [out.wav] [--lufs X --tp Y --lra Z --passes N | --drive D --ceiling C] [--mono F]\n");
        return 1;
    }
    std::string inPath = argv[1], outPath;
    fin::Settings s;
    fin::Targets t;
    bool useTargets = false;
    int passes = 3;

    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::stof (argv[++i]) : 0.0f; };
        if (a == "--lufs")         { t.lufs = next(); useTargets = true; }
        else if (a == "--tp")      { t.truePeak = next(); useTargets = true; }
        else if (a == "--lra")     { t.lra = next(); useTargets = true; }
        else if (a == "--passes")  passes = (int) next();
        else if (a == "--drive")   s.drive = next();
        else if (a == "--ceiling") s.ceiling = next();
        else if (a == "--mono")    { const float f = next(); s.monoOn = f > 0.0f; if (f > 0.0f) s.monoFreq = f; }
        else if (a == "--clip")    { s.clipOn = true; s.clipThresh = next(); }
        else if (a == "--soft")    s.clipSoft = next();
        else if (a == "--rel")     s.limRelease = next();
        else if (a == "--full")    { s.trOn = s.compOn = s.mbOn = s.satOn = s.clipOn = true; s.p1.range = -3.0f; }
        else if (a[0] != '-')      outPath = a;
    }

    Audio in;
    if (! readWav (inPath, in)) { std::fprintf (stderr, "WAV konnte nicht gelesen werden: %s\n", inPath.c_str()); return 1; }

    // Messung des Originals: Kette komplett neutral
    fin::Settings neutral;
    neutral.eqOn = false; neutral.monoOn = false; neutral.compOn = false; neutral.limOn = false;
    print ("Original", run (in, neutral, nullptr));

    if (useTargets)
    {
        for (int p = 0; p < passes; ++p)
        {
            const auto m = run (in, s, nullptr);
            const auto r = fin::Engine::computeTargets (s, m, t);
            s = r.settings;
            std::printf ("Durchgang %d: Drive %.1f dB, Ceiling %.1f, Comp %s (Thr %.1f, Ratio %.2f)\n", p + 1, s.drive, s.ceiling,
                         s.compOn ? "an" : "aus", s.thresh, s.ratio);
        }
    }

    Audio out;
    const auto m = run (in, s, outPath.empty() ? nullptr : &out);
    print ("Finisher", m);

    if (! outPath.empty())
    {
        if (! writeWav (outPath, out)) { std::fprintf (stderr, "Schreiben fehlgeschlagen\n"); return 1; }
        std::printf ("Geschrieben: %s\n", outPath.c_str());
    }
    return 0;
}
