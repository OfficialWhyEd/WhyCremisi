#include "SampleLibrary.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <regex>
#include <set>

// ═══════════════════════════════════════════════════════════════════
//  Indice dei suoni
// ═══════════════════════════════════════════════════════════════════

namespace
{
    constexpr int kFftOrder = 11;                    // 2048 campioni
    constexpr int kFftSize  = 1 << kFftOrder;
    constexpr double kMaxReadSeconds = 8.0;          // oltre non serve per capire cos'e'

    juce::StringArray tokens (const juce::String& s)
    {
        // "Kick_Hard-01 (Dark)" -> kick hard 01 dark. I numeri restano: servono a poco, ma non danno fastidio.
        juce::String cleaned;
        for (auto c : s)
            cleaned += juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (juce::CharacterFunctions::toLowerCase (c)) : " ";
        juce::StringArray t;
        t.addTokens (cleaned, " ", "");
        t.removeEmptyStrings();
        return t;
    }

    float toDb (float lin) { return lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -120.0f; }

    bool hasAny (const juce::StringArray& t, std::initializer_list<const char*> words)
    {
        for (auto* w : words)
            if (t.contains (w)) return true;
        return false;
    }

    /** Ruolo dalle sole parole del nome (vuoto se non dice niente). */
    juce::String roleFromWords (const juce::String& text)
    {
        const auto t = tokens (text);
        const auto glued = juce::String (text).toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");

        const bool open = hasAny (t, { "open", "oh", "openhat", "ohat", "ohh" }) || glued.contains ("openhat") || glued.contains ("openhh");

        if (hasAny (t, { "kick", "kicks", "bd", "bassdrum", "kik" }) || glued.contains ("kickdrum")) return "kick";
        if (hasAny (t, { "snare", "snares", "snr", "sd" }) || glued.contains ("snare"))               return "snare";
        if (hasAny (t, { "clap", "claps", "clp", "handclap" }))                                       return "clap";
        if (hasAny (t, { "rim", "rimshot", "sidestick" }))                                            return "rim";
        if ((hasAny (t, { "hat", "hats", "hihat", "hihats", "hh", "hi", "cymbal" }) || glued.contains ("hihat")) && open)
            return "openhat";
        if (hasAny (t, { "oh", "openhat", "ohat" }) || glued.contains ("openhat"))                    return "openhat";
        if (hasAny (t, { "hat", "hats", "hihat", "hihats", "hh", "chh", "closedhat", "closed" }) || glued.contains ("hihat"))
            return "hat";
        if (hasAny (t, { "808", "808s" }))                                                            return "bass808";
        if (hasAny (t, { "bass", "sub", "reese", "bassline" }))                                       return "bass";
        if (hasAny (t, { "crash", "ride", "splash" }))                                                return "cymbal";
        if (hasAny (t, { "perc", "percussion", "shaker", "conga", "bongo", "tom", "toms", "tambourine", "cowbell", "clave" }))
            return "perc";
        if (hasAny (t, { "vocal", "vocals", "vox", "acapella", "acappella", "chant", "shout" }))      return "vocal";
        if (hasAny (t, { "fx", "sfx", "riser", "impact", "sweep", "noise", "texture", "downlifter", "uplifter", "foley" }))
            return "fx";
        if (hasAny (t, { "pad", "pads", "chord", "chords", "lead", "synth", "keys", "piano", "guitar", "pluck", "arp", "melody", "melodic", "strings", "rhodes" }))
            return "melodic";
        if (hasAny (t, { "loop", "loops", "break", "breaks", "groove", "drumloop" }))                 return "loop";
        return {};
    }

    /** Tempo dal nome: "140bpm", "bpm140", "_140_" in un intervallo credibile. */
    float bpmFromName (const juce::String& name)
    {
        const std::string s = name.toStdString();
        static const std::regex r1 (R"((\d{2,3})\s*bpm)", std::regex::icase);
        static const std::regex r2 (R"(bpm\s*[_-]?\s*(\d{2,3}))", std::regex::icase);
        static const std::regex r3 (R"((?:^|[_\s-])(\d{2,3})(?:$|[_\s-]))");
        std::smatch m;
        for (const auto* re : { &r1, &r2, &r3 })
            if (std::regex_search (s, m, *re))
            {
                const int v = std::stoi (m[1].str());
                if (v >= 60 && v <= 200) return (float) v;
            }
        return 0.0f;
    }

    juce::String keyFromName (const juce::String& name)
    {
        const std::string s = name.toStdString();
        static const std::regex r (R"((?:^|[_\s-])([A-G][#b]?)(m|min|minor|maj|major)?(?:$|[_\s.-]))");
        std::smatch m;
        if (std::regex_search (s, m, r))
        {
            juce::String k (m[1].str());
            const juce::String q (m[2].str());
            if (q == "m" || q == "min" || q == "minor") k += "m";
            return k;
        }
        return {};
    }

    /** Tempo dal ritmo del file: autocorrelazione dell'energia che sale. 0 se non c'e' pulsazione. */
    float estimateLoopBpm (const juce::AudioBuffer<float>& mono, double sr, double seconds)
    {
        const int hop = 512;
        const int n = mono.getNumSamples() / hop;
        if (n < 32) return 0.0f;

        std::vector<float> env ((size_t) n, 0.0f);
        const float* d = mono.getReadPointer (0);
        for (int i = 0; i < n; ++i)
        {
            double e = 0.0;
            for (int k = 0; k < hop; ++k) e += (double) d[i * hop + k] * d[i * hop + k];
            env[(size_t) i] = (float) std::sqrt (e / hop);
        }
        std::vector<float> flux ((size_t) n, 0.0f);
        for (int i = 1; i < n; ++i) flux[(size_t) i] = std::max (0.0f, env[(size_t) i] - env[(size_t) i - 1]);

        const double frameRate = sr / hop;
        auto autocorr = [&] (int lag)
        {
            double s = 0.0;
            for (int i = lag; i < n; ++i) s += (double) flux[(size_t) i] * flux[(size_t) (i - lag)];
            return s / std::max (1, n - lag);
        };
        const int minLag = (int) std::floor (frameRate * 60.0 / 200.0);
        const int maxLag = std::min (n - 2, (int) std::ceil (frameRate * 60.0 / 70.0));
        double best = 0.0, total = 0.0;
        int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const double a = autocorr (lag);
            total += a;
            if (a > best) { best = a; bestLag = lag; }
        }
        if (bestLag <= 0 || total <= 0.0) return 0.0f;
        double estimate = 60.0 * frameRate / bestLag;

        // Il battito vero e' spesso a meta' o al doppio: si sceglie il tempo che fa tornare
        // la durata in un numero intero di battute (1, 2, 4, 8, 16), il piu' vicino alla stima.
        double chosen = estimate, bestDist = 1.0e9;
        for (int bars : { 1, 2, 4, 8, 16 })
        {
            const double bpm = bars * 240.0 / seconds;
            if (bpm < 70.0 || bpm > 180.0) continue;
            const double dist = std::abs (bpm - estimate);
            if (dist < bestDist) { bestDist = dist; chosen = bpm; }
        }
        if (bestDist > 12.0) return 0.0f;   // niente combacia: meglio non dire un numero
        return (float) chosen;
    }
}

nlohmann::json SampleInfo::toJson() const
{
    return {
        { "path", path.toStdString() }, { "name", name.toStdString() }, { "folder", folder.toStdString() },
        { "role", role.toStdString() }, { "seconds", seconds }, { "peakDb", peakDb }, { "rmsDb", rmsDb },
        { "centroidHz", centroidHz }, { "lowRatio", lowRatio }, { "highRatio", highRatio },
        { "bpm", bpm }, { "key", key.toStdString() }, { "size", (double) sizeBytes }, { "modified", (double) modified }
    };
}

SampleInfo SampleInfo::fromJson (const nlohmann::json& j)
{
    SampleInfo s;
    auto str = [&] (const char* k) { return j.contains (k) && j[k].is_string() ? juce::String (j[k].get<std::string>()) : juce::String(); };
    auto num = [&] (const char* k) { return j.contains (k) && j[k].is_number() ? j[k].get<double>() : 0.0; };
    s.path = str ("path"); s.name = str ("name"); s.folder = str ("folder"); s.role = str ("role"); s.key = str ("key");
    s.seconds = num ("seconds"); s.peakDb = (float) num ("peakDb"); s.rmsDb = (float) num ("rmsDb");
    s.centroidHz = (float) num ("centroidHz"); s.lowRatio = (float) num ("lowRatio"); s.highRatio = (float) num ("highRatio");
    s.bpm = (float) num ("bpm"); s.sizeBytes = (juce::int64) num ("size"); s.modified = (juce::int64) num ("modified");
    return s;
}

SampleLibrary::SampleLibrary()
{
    indexFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("WhyCremisi").getChildFile ("samples-index.json");
}

juce::String SampleLibrary::normalizeRole (const juce::String& word)
{
    const auto w = word.trim().toLowerCase();
    if (w.isEmpty()) return {};
    static const std::map<juce::String, juce::String> aliases {
        { "kick", "kick" }, { "cassa", "kick" }, { "bd", "kick" }, { "bassdrum", "kick" },
        { "snare", "snare" }, { "rullante", "snare" }, { "snr", "snare" },
        { "clap", "clap" }, { "battito", "clap" },
        { "rim", "rim" }, { "rimshot", "rim" },
        { "hat", "hat" }, { "hihat", "hat" }, { "hi-hat", "hat" }, { "charleston", "hat" }, { "hh", "hat" },
        { "openhat", "openhat" }, { "open", "openhat" }, { "oh", "openhat" },
        { "perc", "perc" }, { "percussione", "perc" }, { "percussioni", "perc" }, { "shaker", "perc" },
        { "808", "bass808" }, { "bass808", "bass808" },
        { "bass", "bass" }, { "basso", "bass" }, { "sub", "bass" },
        { "cymbal", "cymbal" }, { "piatto", "cymbal" }, { "crash", "cymbal" },
        { "loop", "loop" },
        { "melodic", "melodic" }, { "melodia", "melodic" }, { "synth", "melodic" }, { "pad", "melodic" }, { "keys", "melodic" },
        { "vocal", "vocal" }, { "voce", "vocal" }, { "vox", "vocal" },
        { "fx", "fx" }, { "effetto", "fx" }, { "other", "other" }
    };
    auto it = aliases.find (w);
    return it != aliases.end() ? it->second : juce::String();
}

SampleInfo SampleLibrary::analyze (const juce::File& file, juce::AudioFormatManager& formats, bool& valid)
{
    valid = false;
    SampleInfo info;
    info.path = file.getFullPathName();
    info.name = file.getFileNameWithoutExtension();
    info.folder = file.getParentDirectory().getFileName();
    info.sizeBytes = file.getSize();
    info.modified = file.getLastModificationTime().toMilliseconds();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0 || reader->numChannels == 0)
        return info;

    const double sr = reader->sampleRate;
    info.seconds = (double) reader->lengthInSamples / sr;

    const int toRead = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (sr * kMaxReadSeconds));
    const int ch = (int) std::min<unsigned> (reader->numChannels, 2u);
    juce::AudioBuffer<float> raw (ch, toRead);
    if (! reader->read (&raw, 0, toRead, 0, true, ch > 1))
        return info;

    juce::AudioBuffer<float> mono (1, toRead);
    mono.clear();
    for (int c = 0; c < ch; ++c) mono.addFrom (0, 0, raw, c, 0, toRead, 1.0f / (float) ch);

    const float* d = mono.getReadPointer (0);
    float peak = 0.0f;
    double sumSq = 0.0;
    for (int i = 0; i < toRead; ++i) { peak = std::max (peak, std::abs (d[i])); sumSq += (double) d[i] * d[i]; }
    info.peakDb = toDb (peak);
    info.rmsDb  = toDb ((float) std::sqrt (sumSq / toRead));

    // Finestra piu' energica (di norma l'attacco) per misurare dove sta il peso dello spettro.
    int bestStart = 0;
    double bestEnergy = -1.0;
    const int step = kFftSize / 2;
    for (int s = 0; s + 256 < toRead; s += step)
    {
        double e = 0.0;
        const int n = std::min (kFftSize, toRead - s);
        for (int i = 0; i < n; ++i) e += (double) d[s + i] * d[s + i];
        if (e > bestEnergy) { bestEnergy = e; bestStart = s; }
    }

    juce::dsp::FFT fft (kFftOrder);
    std::vector<float> buf ((size_t) kFftSize * 2, 0.0f);
    const int n = std::min (kFftSize, toRead - bestStart);
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) std::max (1, n - 1));
        buf[(size_t) i] = d[bestStart + i] * w;
    }
    fft.performFrequencyOnlyForwardTransform (buf.data());

    double sumMag = 0.0, sumFm = 0.0, eLow = 0.0, eHigh = 0.0, eAll = 0.0;
    for (int k = 1; k < kFftSize / 2; ++k)
    {
        const double f = k * sr / kFftSize;
        const double m = buf[(size_t) k];
        sumMag += m; sumFm += f * m;
        const double e = m * m;
        eAll += e;
        if (f < 200.0) eLow += e;
        if (f > 5000.0) eHigh += e;
    }
    if (sumMag > 0.0 && eAll > 0.0)
    {
        info.centroidHz = (float) (sumFm / sumMag);
        info.lowRatio   = (float) (eLow / eAll);
        info.highRatio  = (float) (eHigh / eAll);
    }

    info.key = keyFromName (info.name);
    if (info.seconds >= 1.5)
    {
        info.bpm = bpmFromName (info.name);
        if (info.bpm <= 0.0f) info.bpm = estimateLoopBpm (mono, sr, info.seconds);
    }
    info.role = classify (info);
    valid = true;
    return info;
}

juce::String SampleLibrary::classify (const SampleInfo& info)
{
    // Prima le parole: chi ha organizzato la cartella sa meglio di un'analisi.
    auto r = roleFromWords (info.name);
    if (r.isEmpty()) r = roleFromWords (info.folder);
    if (r.isNotEmpty())
    {
        // "Kick Loop" resta un loop; un file lungo chiamato "snare" e' comunque una frase.
        if (info.seconds >= 2.5 && (r == "kick" || r == "snare" || r == "clap" || r == "hat" || r == "openhat" || r == "rim"))
            return "loop";
        return r;
    }

    // Niente nel nome: si decide dal suono.
    if (info.seconds >= 1.5 && info.bpm > 0.0f) return "loop";
    if (info.seconds >= 2.5) return "other";
    if (info.lowRatio > 0.55f) return info.seconds < 1.2 ? "kick" : "bass808";
    if (info.highRatio > 0.45f && info.seconds < 0.35) return "hat";
    if (info.highRatio > 0.45f) return "openhat";
    if (info.centroidHz > 1500.0f && info.centroidHz < 7000.0f && info.seconds < 0.8) return "snare";
    return "other";
}

SampleLibrary::ScanReport SampleLibrary::scan (const juce::File& folder, int maxFiles, double budgetSeconds)
{
    ScanReport rep;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    if (! folder.isDirectory()) return rep;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    // Quello che e' gia' nell'indice e non e' cambiato non si rilegge.
    std::map<juce::String, SampleInfo> known;
    {
        std::lock_guard<std::mutex> lock (mutex);
        for (const auto& s : samples) known[s.path] = s;
    }

    juce::Array<juce::File> files;
    folder.findChildFiles (files, juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    rep.found = files.size();

    int processed = 0;
    std::vector<SampleInfo> results;
    for (const auto& f : files)
    {
        if (processed >= maxFiles || (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 > budgetSeconds)
        {
            rep.timedOut = true;
            break;
        }
        const auto key = f.getFullPathName();
        auto it = known.find (key);
        if (it != known.end() && it->second.sizeBytes == f.getSize()
            && it->second.modified == f.getLastModificationTime().toMilliseconds())
        {
            ++rep.reused;
            continue;
        }
        bool ok = false;
        auto info = analyze (f, formats, ok);
        ++processed;
        if (ok) { results.push_back (info); ++rep.analyzed; }
        else    { ++rep.failed; }
    }

    {
        std::lock_guard<std::mutex> lock (mutex);
        for (auto& r : results)
        {
            auto it = std::find_if (samples.begin(), samples.end(), [&] (const SampleInfo& s) { return s.path == r.path; });
            if (it != samples.end()) *it = r; else samples.push_back (r);
        }
    }
    rep.elapsedSeconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    return rep;
}

std::vector<SampleInfo> SampleLibrary::search (const Query& q) const
{
    const auto wanted = normalizeRole (q.role);
    const auto words = tokens (q.text);

    const bool dark   = hasAny (words, { "dark", "scuro", "scura", "cupo", "deep", "profondo", "sub", "heavy", "pesante" });
    const bool bright = hasAny (words, { "bright", "chiaro", "chiara", "luminoso", "crisp", "sottile", "light", "leggero" });
    const bool shortS = hasAny (words, { "short", "corto", "corta", "tight", "secco", "dry" });
    const bool longS  = hasAny (words, { "long", "lungo", "lunga", "boomy", "sustain" });

    struct Scored { double score; SampleInfo s; };
    std::vector<Scored> out;
    {
        std::lock_guard<std::mutex> lock (mutex);
        for (const auto& s : samples)
        {
            if (wanted.isNotEmpty() && s.role != wanted)
            {
                // Un battito di mani e un rullante si sostituiscono, un kick no.
                const bool compatible = (wanted == "snare" && (s.role == "clap" || s.role == "rim"))
                                     || (wanted == "clap"  && s.role == "snare")
                                     || (wanted == "bass808" && s.role == "bass")
                                     || (wanted == "bass"    && s.role == "bass808");
                if (! compatible) continue;
            }
            double score = (wanted.isNotEmpty() && s.role != wanted) ? -2.0 : 0.0;

            const auto hay = tokens (s.name + " " + s.folder);
            for (const auto& w : words)
                if (hay.contains (w)) score += 2.0;

            // Il gusto "scuro" o "chiaro" si misura sul suono, non solo sul nome.
            if (dark)   score += (s.centroidHz < 500.0f ? 2.0 : s.centroidHz < 1500.0f ? 1.0 : -1.0);
            if (bright) score += (s.centroidHz > 4000.0f ? 2.0 : s.centroidHz > 2000.0f ? 1.0 : -1.0);
            if (shortS) score += (s.seconds < 0.3 ? 1.5 : s.seconds < 0.7 ? 0.5 : -1.0);
            if (longS)  score += (s.seconds > 1.0 ? 1.5 : s.seconds > 0.5 ? 0.5 : -1.0);

            if (q.bpm > 0.0f && s.bpm > 0.0f)
            {
                const double diff = std::abs (s.bpm - q.bpm);
                score += diff < 2.0 ? 3.0 : diff < 6.0 ? 1.5 : diff > 20.0 ? -3.0 : 0.0;
            }
            // Suoni quasi muti o clippati sono un cattivo punto di partenza.
            if (s.peakDb < -30.0f) score -= 2.0;

            out.push_back ({ score, s });
        }
    }
    std::stable_sort (out.begin(), out.end(), [] (const Scored& a, const Scored& b)
    {
        if (a.score != b.score) return a.score > b.score;
        return a.s.path < b.s.path;  // a parita' l'ordine e' stabile: stessa domanda, stesso risultato
    });
    std::vector<SampleInfo> res;
    for (size_t i = 0; i < out.size() && (int) i < std::max (1, q.limit); ++i) res.push_back (out[i].s);
    return res;
}

bool SampleLibrary::pick (const juce::String& role, const juce::String& text, unsigned seed, SampleInfo& out) const
{
    Query q;
    q.role = role; q.text = text; q.limit = 24;
    auto found = search (q);
    if (found.empty()) return false;

    // Fra i migliori a pari merito si varia col seme; i peggiori non entrano mai.
    const size_t pool = std::min<size_t> (found.size(), 5);
    std::mt19937 rng (seed ^ 0x9e3779b9u);
    out = found[rng() % pool];
    return true;
}

int SampleLibrary::size() const
{
    std::lock_guard<std::mutex> lock (mutex);
    return (int) samples.size();
}

std::map<juce::String, int> SampleLibrary::countByRole() const
{
    std::lock_guard<std::mutex> lock (mutex);
    std::map<juce::String, int> m;
    for (const auto& s : samples) ++m[s.role];
    return m;
}

void SampleLibrary::clear()
{
    std::lock_guard<std::mutex> lock (mutex);
    samples.clear();
}

void SampleLibrary::add (const SampleInfo& s)
{
    std::lock_guard<std::mutex> lock (mutex);
    samples.push_back (s);
}

bool SampleLibrary::save() const
{
    nlohmann::json arr = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock (mutex);
        for (const auto& s : samples) arr.push_back (s.toJson());
    }
    nlohmann::json root;
    root["version"] = 1;
    root["samples"] = std::move (arr);

    if (! indexFile.getParentDirectory().createDirectory()) return false;
    // Prima in un file accanto e poi si rinomina: un crash a meta' non lascia un indice rotto.
    auto tmp = indexFile.getSiblingFile (indexFile.getFileName() + ".tmp");
    if (! tmp.replaceWithText (juce::String (root.dump()))) return false;
    return tmp.moveFileTo (indexFile);
}

bool SampleLibrary::load()
{
    if (! indexFile.existsAsFile()) return false;
    try
    {
        auto root = nlohmann::json::parse (indexFile.loadFileAsString().toStdString());
        if (! root.contains ("samples") || ! root["samples"].is_array()) return false;
        std::vector<SampleInfo> loaded;
        for (const auto& j : root["samples"])
        {
            auto s = SampleInfo::fromJson (j);
            if (s.path.isNotEmpty()) loaded.push_back (std::move (s));
        }
        std::lock_guard<std::mutex> lock (mutex);
        samples = std::move (loaded);
        return true;
    }
    catch (...) { return false; }
}
