#include "NoteComposer.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace
{
    int tonicFromName (const juce::String& n)
    {
        if (n.isEmpty()) return -1;
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto c = juce::CharacterFunctions::toUpperCase (n[0]);
        if (c < 'A' || c > 'G') return -1;
        int v = base[c - 'A'];
        for (int i = 1; i < n.length(); ++i)
        {
            if (n[i] == '#') ++v;
            else if (n[i] == 'b') --v;
            else return -1;
        }
        return ((v % 12) + 12) % 12;
    }
}

bool NoteComposer::Scale::contains (int pitch) const
{
    const int pc = (((pitch - tonic) % 12) + 12) % 12;
    return std::find (degrees.begin(), degrees.end(), pc) != degrees.end();
}

int NoteComposer::Scale::nearest (int pitch) const
{
    if (! valid() || contains (pitch)) return pitch;
    for (int d = 1; d < 12; ++d)
    {
        if (contains (pitch - d)) return pitch - d;
        if (contains (pitch + d)) return pitch + d;
    }
    return pitch;
}

NoteComposer::Scale NoteComposer::parseScale (const juce::String& text)
{
    Scale s;
    auto parts = juce::StringArray::fromTokens (text.trim(), " ", "");
    parts.removeEmptyStrings();
    if (parts.size() < 1) return s;
    const int t = tonicFromName (parts[0]);
    if (t < 0) return s;

    const auto mode = parts.size() > 1 ? parts[1].toLowerCase() : juce::String ("major");
    std::vector<int> d;
    if      (mode == "major" || mode == "ionian" || mode == "maggiore") d = { 0, 2, 4, 5, 7, 9, 11 };
    else if (mode == "minor" || mode == "aeolian" || mode == "minore")  d = { 0, 2, 3, 5, 7, 8, 10 };
    else if (mode == "dorian")      d = { 0, 2, 3, 5, 7, 9, 10 };
    else if (mode == "phrygian")    d = { 0, 1, 3, 5, 7, 8, 10 };
    else if (mode == "lydian")      d = { 0, 2, 4, 6, 7, 9, 11 };
    else if (mode == "mixolydian")  d = { 0, 2, 4, 5, 7, 9, 10 };
    else if (mode == "locrian")     d = { 0, 1, 3, 5, 6, 8, 10 };
    else if (mode == "harmonic")    d = { 0, 2, 3, 5, 7, 8, 11 };
    else return s;

    s.tonic = t;
    s.degrees = std::move (d);
    return s;
}

juce::String NoteComposer::Report::summary() const
{
    return juce::String (total) + " note ricevute: " + juce::String (total - dropped) + " tenute, "
         + juce::String (snapped) + " spostate sulla scala, "
         + juce::String (clipped) + " accorciate, "
         + juce::String (dropped) + " scartate.";
}

std::vector<NoteComposer::Note> NoteComposer::sanitize (const juce::String& json, const Scale& scale,
                                                        double clipBeats, Report& report)
{
    std::vector<Note> out;
    report = {};
    nlohmann::json j = nlohmann::json::parse (json.toStdString(), nullptr, false);
    if (j.is_discarded() || ! j.is_array()) return out;

    auto num = [] (const nlohmann::json& o, const char* k, double def) {
        auto it = o.find (k);
        return (it != o.end() && it->is_number()) ? it->get<double>() : def;
    };

    for (const auto& o : j)
    {
        ++report.total;
        if (! o.is_object()) { ++report.dropped; continue; }
        const double p = num (o, "pitch", -1), st = num (o, "start", -1), len = num (o, "length", 1.0);
        if (! std::isfinite (p) || ! std::isfinite (st) || ! std::isfinite (len)
            || p < 0 || p > 127 || st < 0 || st >= clipBeats || len <= 0)
        { ++report.dropped; continue; }

        Note n;
        n.pitch = (int) std::lround (p);
        const int fixedPitch = std::clamp (scale.nearest (n.pitch), 0, 127);
        if (fixedPitch != n.pitch) ++report.snapped;
        n.pitch = fixedPitch;
        n.startBeat = st;
        n.lengthBeats = len;
        if (n.startBeat + n.lengthBeats > clipBeats) { n.lengthBeats = clipBeats - n.startBeat; ++report.clipped; }
        if (n.lengthBeats < 0.0625) { ++report.dropped; continue; }
        n.velocity = std::clamp ((int) std::lround (num (o, "velocity", 100)), 1, 127);
        out.push_back (n);
    }
    std::stable_sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.startBeat < b.startBeat; });
    return out;
}

std::vector<bool> NoteComposer::euclidean (int pulses, int steps)
{
    if (steps <= 0) return {};
    pulses = std::clamp (pulses, 0, steps);
    // Bjorklund per gruppi: si accoppiano colpi e vuoti finche' resta un solo tipo di resto.
    std::vector<std::vector<bool>> a ((size_t) pulses, std::vector<bool> { true });
    std::vector<std::vector<bool>> b ((size_t) (steps - pulses), std::vector<bool> { false });
    while (b.size() > 1 && ! a.empty())
    {
        const size_t n = std::min (a.size(), b.size());
        std::vector<std::vector<bool>> merged;
        for (size_t i = 0; i < n; ++i)
        {
            auto g = a[i];
            g.insert (g.end(), b[i].begin(), b[i].end());
            merged.push_back (g);
        }
        std::vector<std::vector<bool>> rest;
        if (a.size() > n) rest.assign (a.begin() + (long) n, a.end());
        else              rest.assign (b.begin() + (long) n, b.end());
        a = merged;
        b = rest;
    }
    std::vector<bool> out;
    for (auto& g : a) out.insert (out.end(), g.begin(), g.end());
    for (auto& g : b) out.insert (out.end(), g.begin(), g.end());
    return out;
}

std::vector<NoteComposer::Note> NoteComposer::progression (const Scale& scale, const juce::String& romans,
                                                           int bars, Layer layer, int velocity)
{
    std::vector<Note> out;
    if (! scale.valid() || scale.degrees.size() != 7 || bars < 1) return out;

    auto chordRoots = juce::StringArray::fromTokens (romans.trim(), " ", "");
    chordRoots.removeEmptyStrings();
    if (chordRoots.isEmpty()) return out;

    // Nota della scala per grado (0 = I), anche oltre l'ottava.
    auto noteAt = [&] (int degreeIndex, int octaveBase) {
        const int oct = degreeIndex / 7, i = degreeIndex % 7;
        return octaveBase + scale.tonic + scale.degrees[(size_t) i] + 12 * oct;
    };

    static const char* names[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    for (int bar = 0; bar < bars; ++bar)
    {
        const auto up = chordRoots[bar % chordRoots.size()].trim().toUpperCase();
        int deg = -1;
        for (int i = 6; i >= 0; --i)      // VII prima di VI prima di V...
            if (up == names[i]) { deg = i; break; }
        if (deg < 0) return {};

        const double start = bar * 4.0;
        const int root = noteAt (deg, 48), third = noteAt (deg + 2, 48), fifth = noteAt (deg + 4, 48);
        auto add = [&] (int p, double s, double l, int v) {
            out.push_back ({ std::clamp (p, 0, 127), s, l, std::clamp (v, 1, 127) });
        };

        switch (layer)
        {
            case Layer::Chords:
                add (root + 12, start, 4.0, velocity);
                add (third + 12, start, 4.0, velocity);
                add (fifth + 12, start, 4.0, velocity);
                break;
            case Layer::Bass:
                add (root - 12, start, 2.0, velocity + 10);
                add (root - 12, start + 2.0, 1.5, velocity);
                add (fifth - 12, start + 3.5, 0.5, velocity - 10);
                break;
            case Layer::Arp:
                for (int s = 0; s < 8; ++s)
                {
                    const int tones[4] = { root, third, fifth, third };
                    add (tones[s % 4] + 12, start + s * 0.5, 0.5, velocity - (s % 2) * 15);
                }
                break;
        }
    }
    return out;
}
