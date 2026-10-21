#include "BeatBuilder.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace
{
    // 16 passi per battuta. 'x' colpo pieno, 'o' colpo leggero (ghost), '.' niente.
    struct LanePattern { const char* lane; const char* steps; };
    struct StyleDef
    {
        const char* name;
        double bpm;
        double swing;   // 0..0.5 ritardo dei sedicesimi dispari, frazione di sedicesimo
        std::vector<LanePattern> lanes;
    };

    const std::vector<StyleDef>& styles()
    {
        static const std::vector<StyleDef> s {
            { "trap", 140.0, 0.0, {
                { "kick",    "x.....x...x....." },
                { "snare",   "........x......." },
                { "clap",    "........x......." },
                { "hat",     "x.x.x.x.x.x.x.xx" },
                { "openhat", "......x.......x." },
                { "bass808", "x.....x...x....." } } },
            { "drill", 142.0, 0.0, {
                { "kick",    "x.....x....x...." },
                { "snare",   "............x..." },
                { "hat",     "x.xxx.xxx.xxx.xx" },
                { "openhat", ".......x........" },
                { "perc",    "..o...o....o...." },
                { "bass808", "x.....x....x...." } } },
            { "boombap", 90.0, 0.18, {
                { "kick",    "x.....o...x..o.." },
                { "snare",   "....x.......x..." },
                { "hat",     "x.x.x.x.x.x.x.x." },
                { "openhat", "..............x." } } },
            { "lofi", 78.0, 0.25, {
                { "kick",    "x.....o...x....." },
                { "snare",   "....x.......x..." },
                { "hat",     "x.x.x.x.x.x.x.x." },
                { "perc",    "o...o...o...o..." } } },
            { "house", 124.0, 0.0, {
                { "kick",    "x...x...x...x..." },
                { "clap",    "....x.......x..." },
                { "hat",     "..x...x...x...x." },
                { "openhat", "..x...x...x...x." },
                { "perc",    ".o..o..o.o..o..o" } } },
            { "techno", 130.0, 0.0, {
                { "kick",    "x...x...x...x..." },
                { "clap",    "....x.......x..." },
                { "hat",     "xxxxxxxxxxxxxxxx" },
                { "openhat", "..x...x...x...x." },
                { "perc",    "...o..o....o..o." } } },
            { "dnb", 174.0, 0.0, {
                { "kick",    "x.........x....." },
                { "snare",   "....x.......x..." },
                { "hat",     "x.x.x.x.x.x.x.x." },
                { "openhat", "..............x." } } }
        };
        return s;
    }

    const StyleDef* findStyle (const juce::String& name)
    {
        for (const auto& s : styles())
            if (name.equalsIgnoreCase (s.name)) return &s;
        return nullptr;
    }

    int laneVelocity (const juce::String& lane, char mark, std::mt19937& rng)
    {
        int base = mark == 'o' ? 60 : 100;
        if (lane == "hat") base = mark == 'o' ? 55 : 82;
        if (lane == "kick" || lane == "bass808") base = mark == 'o' ? 75 : 112;
        if (lane == "snare" || lane == "clap") base = 108;
        std::uniform_int_distribution<int> jitter (-7, 7);
        return juce::jlimit (1, 127, base + jitter (rng));
    }

    /** Semitoni per il 808 attorno al suono: giri bassi in una scala minore semplice. */
    int bassSemitone (int index, std::mt19937& rng)
    {
        static const int steps[] { 0, 0, -2, 3, 0, 5, -4 };
        (void) rng;
        return steps[index % 7];
    }

    struct Audio { juce::AudioBuffer<float> buf; double sr = 44100.0; bool ok = false; };

    Audio loadSample (const juce::String& path, juce::AudioFormatManager& fm, double maxSeconds)
    {
        Audio a;
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (path)));
        if (r == nullptr || r->lengthInSamples <= 0) return a;
        const int n = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * maxSeconds));
        a.buf.setSize (2, n);
        a.buf.clear();
        juce::AudioBuffer<float> tmp ((int) std::max (1u, std::min (2u, r->numChannels)), n);
        if (! r->read (&tmp, 0, n, 0, true, tmp.getNumChannels() > 1)) return a;
        a.buf.copyFrom (0, 0, tmp, 0, 0, n);
        a.buf.copyFrom (1, 0, tmp, tmp.getNumChannels() > 1 ? 1 : 0, 0, n);
        a.sr = r->sampleRate;
        a.ok = true;
        return a;
    }

    /** Somma un colpo nel buffer di corsia, con ricampionamento lineare per tempo e intonazione. */
    void mixHit (juce::AudioBuffer<float>& lane, const Audio& s, double outSr, double startSec,
                 float gain, double pitchRatio, double maxLenSec)
    {
        const double ratio = (s.sr / outSr) * pitchRatio;     // campioni sorgente per campione d'uscita
        const int start = (int) std::llround (startSec * outSr);
        const int srcN = s.buf.getNumSamples();
        int outN = (int) std::floor ((srcN - 1) / ratio);
        outN = std::min (outN, (int) (maxLenSec * outSr));
        const int fade = (int) (0.004 * outSr);
        for (int i = 0; i < outN; ++i)
        {
            const int o = start + i;
            if (o < 0 || o >= lane.getNumSamples()) continue;
            const double pos = i * ratio;
            const int i0 = (int) pos;
            const float fr = (float) (pos - i0);
            float env = gain;
            if (i < 64) env *= (float) i / 64.0f;                        // niente click all'attacco
            if (outN - i < fade) env *= (float) (outN - i) / (float) fade; // taglio morbido in coda
            for (int c = 0; c < 2; ++c)
            {
                const float a = s.buf.getSample (c, i0);
                const float b = s.buf.getSample (c, std::min (i0 + 1, srcN - 1));
                lane.addSample (c, o, (a + (b - a) * fr) * env);
            }
        }
    }

    bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
    {
        f.deleteFile();
        std::unique_ptr<juce::FileOutputStream> out (f.createOutputStream());
        if (out == nullptr) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (out.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
        if (w == nullptr) return false;
        out.release();   // il writer ne prende possesso
        return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

juce::StringArray BeatPlan::lanes() const
{
    juce::StringArray l;
    for (const auto& n : notes) l.addIfNotAlreadyThere (n.lane);
    return l;
}

juce::StringArray BeatBuilder::styleNames()
{
    juce::StringArray n;
    for (const auto& s : styles()) n.add (s.name);
    return n;
}

juce::String BeatBuilder::styleFromText (const juce::String& text)
{
    const auto t = text.toLowerCase();
    auto has = [&] (std::initializer_list<const char*> ws) { for (auto* w : ws) if (t.contains (w)) return true; return false; };
    if (has ({ "drill" }))                                    return "drill";
    if (has ({ "boom bap", "boombap", "boom-bap", "hip hop classico", "90s" })) return "boombap";
    if (has ({ "lofi", "lo-fi", "lo fi", "chill" }))          return "lofi";
    if (has ({ "techno" }))                                   return "techno";
    if (has ({ "house", "casa", "deep house" }))              return "house";
    if (has ({ "dnb", "drum and bass", "drum & bass", "jungle" })) return "dnb";
    if (has ({ "trap", "808", "rap", "hip hop", "hiphop" }))  return "trap";
    return {};
}

double BeatBuilder::defaultBpm (const juce::String& style)
{
    auto* s = findStyle (style);
    return s != nullptr ? s->bpm : 140.0;
}

int BeatBuilder::pitchForLane (const juce::String& lane)
{
    if (lane == "kick" || lane == "bass808") return 36;
    if (lane == "snare") return 38;
    if (lane == "clap")  return 39;
    if (lane == "hat")   return 42;
    if (lane == "openhat") return 46;
    if (lane == "perc")  return 37;
    return 36;
}

BeatPlan BeatBuilder::plan (const juce::String& styleIn, double bpm, int bars, unsigned seed)
{
    const StyleDef* def = findStyle (styleIn);
    if (def == nullptr) def = findStyle ("trap");

    BeatPlan p;
    p.style = def->name;
    p.bpm = bpm > 0.0 ? juce::jlimit (40.0, 240.0, bpm) : def->bpm;
    p.bars = juce::jlimit (1, 32, bars);
    p.seed = seed;

    std::mt19937 rng (seed * 2654435761u + 17u);
    std::uniform_real_distribution<double> chance (0.0, 1.0);
    int bassIdx = 0;

    for (int bar = 0; bar < p.bars; ++bar)
    {
        const bool fillBar = (bar % 4 == 3) && p.bars > 1;   // ultima battuta del giro: piccolo fill
        for (const auto& lp : def->lanes)
        {
            const juce::String lane (lp.lane);
            for (int step = 0; step < 16; ++step)
            {
                char mark = lp.steps[step];
                if (mark == '.')
                {
                    // Pochi colpi in piu' che cambiano da battuta a battuta, senza rompere il groove.
                    if (lane == "hat" && step % 2 == 1 && chance (rng) < 0.10) mark = 'o';
                    else if (lane == "kick" && step % 2 == 1 && step > 8 && chance (rng) < 0.07) mark = 'o';
                    else if (lane == "snare" && fillBar && step >= 12 && chance (rng) < 0.55) mark = 'o';
                    else continue;
                }
                // Qualche colpo manca ogni tanto: il beat respira.
                if (mark == 'o' && chance (rng) < 0.2) continue;

                BeatNote n;
                n.lane = lane;
                n.pitch = pitchForLane (lane);
                n.velocity = laneVelocity (lane, mark, rng);
                double stepPos = step;
                if (step % 2 == 1) stepPos += def->swing;     // swing sui sedicesimi dispari
                n.startBeat = bar * 4.0 + stepPos * 0.25;
                n.lengthBeats = lane == "bass808" ? 0.5 : (lane == "openhat" ? 0.5 : 0.25);
                if (lane == "bass808") n.semitones = bassSemitone (bassIdx++ / 2, rng);
                p.notes.push_back (n);
            }
        }
    }
    std::stable_sort (p.notes.begin(), p.notes.end(), [] (const BeatNote& a, const BeatNote& b)
    {
        return a.startBeat < b.startBeat;
    });
    return p;
}

BeatBuilder::RenderResult BeatBuilder::render (const BeatPlan& plan, const SampleLibrary& library,
                                              const juce::String& tasteText, const juce::File& outFolder)
{
    RenderResult res;
    constexpr double sr = 44100.0;
    if (! outFolder.createDirectory()) { res.error = "Cartella di uscita non creabile"; return res; }
    res.folder = outFolder;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    const int tailSamples = (int) (1.5 * sr);   // coda per gli ultimi colpi
    const int total = (int) std::ceil (plan.lengthSeconds() * sr);
    const int renderLen = total + tailSamples;

    std::map<juce::String, Audio> loaded;
    const auto lanes = plan.lanes();
    for (const auto& lane : lanes)
    {
        SampleInfo s;
        if (! library.pick (lane, tasteText, plan.seed + (unsigned) lane.hashCode(), s))
        {
            // 808 senza suono dedicato: cade su un kick lungo; il resto resta senza.
            if (! (lane == "bass808" && library.pick ("kick", tasteText, plan.seed, s)))
            { res.missing.add (lane); continue; }
        }
        auto a = loadSample (s.path, fm, lane == "bass808" ? 4.0 : 3.0);
        if (! a.ok) { res.missing.add (lane); continue; }
        loaded[lane] = std::move (a);
        res.used[lane] = s;
    }
    if (loaded.empty()) { res.error = "Nessun suono nell'indice per questo beat: scansiona prima una cartella di sample"; return res; }

    std::map<juce::String, juce::AudioBuffer<float>> stems;
    for (const auto& kv : loaded)
    {
        stems[kv.first].setSize (2, renderLen);
        stems[kv.first].clear();
    }

    const double secPerBeat = 60.0 / plan.bpm;
    // Per il choke dell'open hat: quando suona un hat chiuso, l'aperto si ferma.
    std::vector<double> hatTimes;
    for (const auto& n : plan.notes) if (n.lane == "hat") hatTimes.push_back (n.startBeat * secPerBeat);

    for (const auto& n : plan.notes)
    {
        auto it = loaded.find (n.lane);
        if (it == loaded.end()) continue;
        const double t = n.startBeat * secPerBeat;
        double maxLen = n.lane == "bass808" ? n.lengthBeats * secPerBeat * 2.0 : 3.0;
        if (n.lane == "openhat")
        {
            maxLen = 0.6;
            for (double h : hatTimes) if (h > t + 0.01 && h - t < maxLen) { maxLen = h - t; break; }
        }
        if (n.lane == "hat") maxLen = 0.25;
        const float gain = std::pow ((float) n.velocity / 127.0f, 1.6f);
        const double pitch = std::pow (2.0, n.semitones / 12.0);
        mixHit (stems[n.lane], it->second, sr, t, gain, pitch, maxLen);
    }

    // Mix: somma, poi un solo guadagno per tutto (le proporzioni fra corsie restano).
    juce::AudioBuffer<float> mix (2, renderLen);
    mix.clear();
    for (auto& kv : stems)
        for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, kv.second, c, 0, renderLen);

    float peak = mix.getMagnitude (0, renderLen);
    const float target = std::pow (10.0f, -1.0f / 20.0f);
    const float g = peak > 1.0e-6f ? target / peak : 1.0f;
    mix.applyGain (g);
    for (auto& kv : stems) kv.second.applyGain (g);
    res.peakDb = peak > 1.0e-6f ? -1.0f : -100.0f;

    // Lo stem e il mix hanno la stessa lunghezza: la coda in piu' e' il tail dell'ultimo colpo.
    res.mix = outFolder.getChildFile ("mix.wav");
    if (! writeWav (res.mix, mix, sr)) { res.error = "Scrittura di mix.wav fallita"; return res; }
    for (auto& kv : stems)
    {
        auto f = outFolder.getChildFile (kv.first + ".wav");
        if (! writeWav (f, kv.second, sr)) { res.error = "Scrittura di " + kv.first + ".wav fallita"; return res; }
        res.stems[kv.first] = f;
    }
    res.seconds = (double) renderLen / sr;
    res.ok = true;
    return res;
}
