#include "../NoteComposer.h"
#include <juce_core/juce_core.h>

class NoteComposerTest : public juce::UnitTest
{
public:
    NoteComposerTest() : juce::UnitTest ("NoteComposerTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("Le scale si leggono, un nome senza senso resta vuoto");
        {
            auto s = NoteComposer::parseScale ("C minor");
            expect (s.valid());
            expect (s.contains (63) && ! s.contains (64));   // Mib si, Mi no
            expectEquals (NoteComposer::parseScale ("Bb major").tonic, 10);
            expect (! NoteComposer::parseScale ("H blues").valid());
            expect (! NoteComposer::parseScale ("C bluesy").valid());
        }

        beginTest ("Le note fuori scala vengono spostate, quelle senza senso scartate");
        {
            auto s = NoteComposer::parseScale ("C major");
            NoteComposer::Report r;
            auto n = NoteComposer::sanitize (
                "[{\"pitch\":61,\"start\":0,\"length\":1},"
                 "{\"pitch\":64,\"start\":1,\"length\":10},"
                 "{\"pitch\":200,\"start\":0,\"length\":1},"
                 "{\"pitch\":60,\"start\":99,\"length\":1},"
                 "{\"pitch\":62,\"start\":2,\"length\":1,\"velocity\":999},"
                 "\"male\"]", s, 4.0, r);
            expectEquals (r.total, 6);
            expectEquals (r.dropped, 3);
            expectEquals (r.snapped, 1);
            expectEquals (r.clipped, 1);
            expectEquals ((int) n.size(), 3);
            expectEquals (n[0].pitch, 60);
            expectEquals (n[1].lengthBeats, 3.0);
            expectEquals (n[2].velocity, 127);
        }

        beginTest ("Un testo che non e' un array JSON non produce note");
        {
            NoteComposer::Report r;
            expect (NoteComposer::sanitize ("non json", NoteComposer::parseScale ("C major"), 4.0, r).empty());
            expect (NoteComposer::sanitize ("{\"a\":1}", NoteComposer::parseScale ("C major"), 4.0, r).empty());
        }

        beginTest ("Bjorklund: i ritmi euclidei noti");
        {
            auto str = [] (const std::vector<bool>& v) { juce::String s; for (bool b : v) s << (b ? "x" : "."); return s; };
            expectEquals (str (NoteComposer::euclidean (3, 8)), juce::String ("x..x..x."));   // tresillo
            expectEquals (str (NoteComposer::euclidean (4, 4)), juce::String ("xxxx"));
            expectEquals (str (NoteComposer::euclidean (0, 4)), juce::String ("...."));
            int colpi = 0;
            for (bool b : NoteComposer::euclidean (5, 13)) colpi += b ? 1 : 0;
            expectEquals (colpi, 5);
            expectEquals ((int) NoteComposer::euclidean (5, 13).size(), 13);
        }

        beginTest ("La progressione e' tutta dentro la scala e dura quanto le battute");
        {
            auto s = NoteComposer::parseScale ("A minor");
            for (auto layer : { NoteComposer::Layer::Chords, NoteComposer::Layer::Bass, NoteComposer::Layer::Arp })
            {
                auto n = NoteComposer::progression (s, "i VI III VII", 4, layer);
                expect (! n.empty());
                for (auto& x : n)
                {
                    expect (s.contains (x.pitch), "Fuori scala: " + juce::String (x.pitch));
                    expect (x.startBeat + x.lengthBeats <= 16.0 + 1e-9);
                    expect (x.pitch >= 0 && x.pitch <= 127 && x.velocity >= 1 && x.velocity <= 127);
                }
            }
            expect (NoteComposer::progression (s, "i XX", 2, NoteComposer::Layer::Chords).empty());
            // Accordo del primo grado in La minore: La, Do, Mi
            auto c = NoteComposer::progression (s, "i", 1, NoteComposer::Layer::Chords);
            expectEquals ((int) c.size(), 3);
            expectEquals (c[0].pitch % 12, 9);
            expectEquals (c[1].pitch % 12, 0);
            expectEquals (c[2].pitch % 12, 4);
        }
    }
};

static NoteComposerTest noteComposerTest;
