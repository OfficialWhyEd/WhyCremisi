/*
  ==============================================================================
    SamplesTest.cpp

    Indice dei suoni e costruzione del beat, su file veri scritti in una
    cartella temporanea: il kick e' un seno che scende, l'hat e' rumore breve.
    Cosi' si prova che le misure e la classificazione dicono la verita'.
  ==============================================================================
*/

#include "../SampleLibrary.h"
#include "../BeatBuilder.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <random>

namespace
{
    void writeTestWav (const juce::File& f, const std::vector<float>& data, double sr = 44100.0)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> out (f.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (out.get(), sr, 1, 24, {}, 0));
        out.release();
        juce::AudioBuffer<float> b (1, (int) data.size());
        for (int i = 0; i < (int) data.size(); ++i) b.setSample (0, i, data[(size_t) i]);
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    std::vector<float> kickWave()
    {
        std::vector<float> d (22050);
        double phase = 0.0;
        for (size_t i = 0; i < d.size(); ++i)
        {
            const double t = (double) i / 44100.0;
            const double fr = 45.0 + 110.0 * std::exp (-t * 30.0);
            phase += 2.0 * juce::MathConstants<double>::pi * fr / 44100.0;
            d[i] = (float) (std::sin (phase) * std::exp (-t * 7.0) * 0.9);
        }
        return d;
    }

    std::vector<float> hatWave()
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> n (-1.0f, 1.0f);
        std::vector<float> d (4410);
        float prev = 0.0f;
        for (size_t i = 0; i < d.size(); ++i)
        {
            const float x = n (rng);
            d[i] = (x - prev) * std::exp (-(float) i / 700.0f) * 0.5f;   // derivata: tutto in alto
            prev = x;
        }
        return d;
    }

    std::vector<float> loopWave (double bpm)
    {
        // 2 battute a 4/4 con un colpo per quarto.
        const double sr = 44100.0;
        const int len = (int) (8.0 * 60.0 / bpm * sr);
        std::vector<float> d ((size_t) len, 0.0f);
        const int beat = (int) (60.0 / bpm * sr);
        const auto k = kickWave();
        for (int b = 0; b < 8; ++b)
            for (size_t i = 0; i < 6000 && (size_t) (b * beat) + i < d.size(); ++i) d[(size_t) (b * beat) + i] += k[i];
        return d;
    }
}

class SamplesTest : public juce::UnitTest
{
public:
    SamplesTest() : juce::UnitTest ("SamplesTest", "whycremisi") {}

    void runTest() override
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("whycremisi-samples-test");
        dir.deleteRecursively();
        dir.createDirectory();

        beginTest ("Il ruolo viene dalle parole del nome o della cartella");
        {
            SampleInfo a; a.name = "Kick_Hard_03"; a.folder = "x"; a.seconds = 0.4;
            expect (SampleLibrary::classify (a) == "kick");
            SampleInfo b; b.name = "OH 12"; b.folder = "Drums"; b.seconds = 0.5;
            expect (SampleLibrary::classify (b) == "openhat");
            SampleInfo c; c.name = "take 7"; c.folder = "Snares"; c.seconds = 0.3;
            expect (SampleLibrary::classify (c) == "snare");
            SampleInfo d; d.name = "Hi-Hat closed"; d.folder = "x"; d.seconds = 0.1;
            expect (SampleLibrary::classify (d) == "hat");
            expect (SampleLibrary::normalizeRole ("Cassa") == "kick");
            expect (SampleLibrary::normalizeRole ("boh").isEmpty());
        }

        beginTest ("Senza parole nel nome il ruolo viene dal suono");
        {
            writeTestWav (dir.getChildFile ("a/s001.wav"), kickWave());
            writeTestWav (dir.getChildFile ("a/s002.wav"), hatWave());
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            bool ok = false;
            auto k = SampleLibrary::analyze (dir.getChildFile ("a/s001.wav"), fm, ok);
            expect (ok);
            expect (k.lowRatio > 0.6f, "kick: energia grave " + juce::String (k.lowRatio));
            expect (k.centroidHz < 400.0f);
            expect (k.role == "kick", "ruolo kick, trovato " + k.role);
            auto h = SampleLibrary::analyze (dir.getChildFile ("a/s002.wav"), fm, ok);
            expect (ok);
            expect (h.highRatio > 0.45f, "hat: energia acuta " + juce::String (h.highRatio));
            expect (h.role == "hat", "ruolo hat, trovato " + h.role);
            expect (h.centroidHz > k.centroidHz);
            expect (k.peakDb < 0.0f && k.peakDb > -3.0f);
        }

        beginTest ("Il tempo di un loop si legge dal nome o dal ritmo");
        {
            writeTestWav (dir.getChildFile ("loops/groove_140bpm.wav"), loopWave (140.0));
            writeTestWav (dir.getChildFile ("loops/senza-numero.wav"), loopWave (120.0));
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            bool ok = false;
            auto l1 = SampleLibrary::analyze (dir.getChildFile ("loops/groove_140bpm.wav"), fm, ok);
            expect (ok && std::abs (l1.bpm - 140.0f) < 0.5f, "dal nome: " + juce::String (l1.bpm));
            expect (l1.role == "loop");
            auto l2 = SampleLibrary::analyze (dir.getChildFile ("loops/senza-numero.wav"), fm, ok);
            expect (ok && std::abs (l2.bpm - 120.0f) < 4.0f, "dal ritmo: " + juce::String (l2.bpm));
        }

        beginTest ("La scansione riusa quello che non e' cambiato e l'indice sopravvive al disco");
        {
            SampleLibrary lib;
            lib.setIndexFile (dir.getChildFile ("index.json"));
            auto r1 = lib.scan (dir);
            expect (r1.found >= 4 && r1.analyzed == r1.found, "prima scansione");
            auto r2 = lib.scan (dir);
            expect (r2.analyzed == 0 && r2.reused == r1.found, "seconda scansione: nulla da rileggere");
            expect (lib.save());

            SampleLibrary lib2;
            lib2.setIndexFile (dir.getChildFile ("index.json"));
            expect (lib2.load());
            expect (lib2.size() == lib.size());
        }

        beginTest ("La ricerca rispetta il ruolo e il gusto, ed e' ripetibile");
        {
            SampleLibrary lib;
            auto mk = [&] (const char* name, const char* role, float centroid, double sec)
            {
                SampleInfo s; s.path = juce::String ("C:/x/") + name + ".wav"; s.name = name; s.folder = "d";
                s.role = role; s.centroidHz = centroid; s.seconds = sec; s.peakDb = -3.0f; lib.add (s);
            };
            mk ("kick_dark", "kick", 200.0f, 0.5);
            mk ("kick_bright", "kick", 3000.0f, 0.2);
            mk ("hat_a", "hat", 9000.0f, 0.1);

            SampleLibrary::Query q; q.role = "kick"; q.text = "scuro";
            auto res = lib.search (q);
            expect (res.size() == 2);
            expect (res[0].name == "kick_dark", "il piu' scuro viene prima");

            q.text = "bright";
            expect (lib.search (q)[0].name == "kick_bright");

            SampleInfo p1, p2;
            expect (lib.pick ("kick", "", 5, p1) && lib.pick ("kick", "", 5, p2));
            expect (p1.path == p2.path, "stesso seme, stessa scelta");
            expect (! lib.pick ("vocal", "", 1, p1), "nessun vocal nell'indice");
        }

        beginTest ("Il disegno del beat dipende solo dal seme e rispetta stile e lunghezza");
        {
            auto a = BeatBuilder::plan ("trap", 0.0, 4, 42);
            auto b = BeatBuilder::plan ("trap", 0.0, 4, 42);
            auto c = BeatBuilder::plan ("trap", 0.0, 4, 43);
            expect (a.bpm == 140.0 && a.bars == 4);
            expect (a.notes.size() == b.notes.size());
            bool same = true;
            for (size_t i = 0; i < a.notes.size() && same; ++i)
                same = a.notes[i].startBeat == b.notes[i].startBeat && a.notes[i].velocity == b.notes[i].velocity
                       && a.notes[i].lane == b.notes[i].lane;
            expect (same, "stesso seme, stessi colpi");
            expect (a.lengthSeconds() > 6.8 && a.lengthSeconds() < 6.9, "4 battute a 140 BPM = 6,86 s");
            bool diverso = a.notes.size() != c.notes.size();
            for (size_t i = 0; i < a.notes.size() && ! diverso; ++i)
                diverso = a.notes[i].velocity != c.notes[i].velocity;
            expect (diverso, "un altro seme cambia le velocity");
            for (const auto& n : a.notes)
                expect (n.startBeat >= 0.0 && n.startBeat < a.lengthBeats(), "ogni colpo cade dentro le battute");
            expect (BeatBuilder::styleFromText ("voglio un pezzo boom bap") == "boombap");
            expect (BeatBuilder::styleFromText ("dark drill").equalsIgnoreCase ("drill"));
            expect (BeatBuilder::plan ("boh", 0.0, 2, 1).style == "trap");
            // la kick del trap batte sul primo quarto
            bool kickAlloUno = false;
            for (const auto& n : a.notes) if (n.lane == "kick" && n.startBeat == 0.0) kickAlloUno = true;
            expect (kickAlloUno);
        }

        beginTest ("Il render scrive mix e stem della stessa lunghezza, a -1 dBFS");
        {
            SampleLibrary lib;
            auto add = [&] (const juce::File& f, const char* role)
            {
                SampleInfo s; s.path = f.getFullPathName(); s.name = f.getFileNameWithoutExtension(); s.folder = "t";
                s.role = role; s.peakDb = -3.0f; s.seconds = 0.4; lib.add (s);
            };
            add (dir.getChildFile ("a/s001.wav"), "kick");
            add (dir.getChildFile ("a/s002.wav"), "hat");
            add (dir.getChildFile ("a/s001.wav"), "bass808");

            auto plan = BeatBuilder::plan ("trap", 140.0, 2, 9);
            auto out = dir.getChildFile ("render");
            auto res = BeatBuilder::render (plan, lib, "", out);
            expect (res.ok, res.error);
            expect (res.missing.contains ("snare") && res.missing.contains ("clap"), "le corsie senza suono sono dichiarate");
            expect (res.mix.existsAsFile());
            expect (res.stems.count ("kick") == 1 && res.stems.count ("hat") == 1);

            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rm (fm.createReaderFor (res.mix));
            std::unique_ptr<juce::AudioFormatReader> rk (fm.createReaderFor (res.stems["kick"]));
            expect (rm != nullptr && rk != nullptr);
            if (rm != nullptr && rk != nullptr)
            {
                expect (rm->lengthInSamples == rk->lengthInSamples, "stessa lunghezza");
                const double minSec = plan.lengthSeconds();
                expect ((double) rm->lengthInSamples / rm->sampleRate >= minSec, "non piu' corto delle battute");
                juce::AudioBuffer<float> b (2, (int) rm->lengthInSamples);
                rm->read (&b, 0, (int) rm->lengthInSamples, 0, true, true);
                const float peak = b.getMagnitude (0, b.getNumSamples());
                expect (std::abs (juce::Decibels::gainToDecibels (peak) + 1.0f) < 0.2f, "picco vicino a -1 dBFS");

                // Il primo colpo del kick cade sul sample 0 (griglia): c'e' segnale entro 20 ms.
                juce::AudioBuffer<float> kb (2, (int) rk->lengthInSamples);
                rk->read (&kb, 0, (int) rk->lengthInSamples, 0, true, true);
                expect (kb.getMagnitude (0, 0, 882) > 0.001f, "il kick parte sul primo quarto");
            }

            // Indice vuoto: errore chiaro, nessun file finto.
            SampleLibrary vuota;
            auto r2 = BeatBuilder::render (plan, vuota, "", dir.getChildFile ("render2"));
            expect (! r2.ok && r2.error.isNotEmpty());
        }

        dir.deleteRecursively();
    }
};

static SamplesTest samplesTest;
