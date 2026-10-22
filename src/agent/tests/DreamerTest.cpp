#include "../Dreamer.h"
#include "../AgentWorkspace.h"
#include <juce_core/juce_core.h>

class DreamerTest : public juce::UnitTest
{
public:
    DreamerTest() : juce::UnitTest ("DreamerTest", "whycremisi") {}

    static MemoryEntry voce (const juce::String& testo, const juce::String& categoria, juce::int64 ts)
    {
        MemoryEntry e;
        e.text = testo; e.category = categoria; e.timestamp = ts;
        e.date = juce::Time (ts).formatted ("%Y-%m-%d");
        return e;
    }

    void runTest() override
    {
        const juce::int64 ora = 1800000000000LL;   // un istante fisso
        const juce::int64 min = 60 * 1000;
        auto diario = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("wc-sogni-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)))
                          .getChildFile ("DREAMS.md");

        beginTest ("La voglia di dormire cresce col tempo fermo e non c'e' senza nulla da riordinare");
        {
            Dreamer::Settings s;
            expectEquals (Dreamer::sleepProbability (s, 5.0, 1.0, 10), 0.0);     // ancora nel periodo di grazia
            const double p20 = Dreamer::sleepProbability (s, 20.0, 1.0, 10);
            const double p60 = Dreamer::sleepProbability (s, 60.0, 1.0, 10);
            expect (p20 > 0.0 && p60 > p20 && p60 < 1.0, "cresce ma resta una probabilita'");
            expectEquals (Dreamer::sleepProbability (s, 60.0, 1.0, 0), 0.0);     // niente di nuovo
            s.enabled = false;
            expectEquals (Dreamer::sleepProbability (s, 60.0, 1.0, 10), 0.0);
        }

        beginTest ("Il dado e' davvero a caso: stesso tempo fermo, esiti diversi");
        {
            Dreamer::Settings s;
            juce::Random rng (42);
            int dorme = 0;
            for (int i = 0; i < 1000; ++i)
                if (Dreamer::shouldFallAsleep (s, 25.0, 1.0, 5, rng)) ++dorme;
            const double attesa = Dreamer::sleepProbability (s, 25.0, 1.0, 5) * 1000.0;
            expect (dorme > 0 && dorme < 1000, "ne' sempre ne' mai");
            expect (std::abs (dorme - attesa) < 80.0, "vicino alla probabilita' dichiarata");
        }

        beginTest ("Le mosse ripetute di un controllo diventano un'abitudine, contate davvero");
        {
            AgentWorkspace ws;
            ws.dailyMemory.clear(); ws.longTermMemory.clear();
            ws.dailyMemory.push_back (voce ("User adjusted master.gain from 0.50 to 0.60", "interaction", ora - 30 * min));
            ws.dailyMemory.push_back (voce ("User adjusted master.gain from 0.60 to 0.70", "interaction", ora - 20 * min));
            ws.dailyMemory.push_back (voce ("AI taglia i bassi (master.gain: 0.70 -> 0.65)", "ai_action", ora - 10 * min));
            ws.dailyMemory.push_back (voce ("User adjusted reverb.mix from 0.10 to 0.20", "interaction", ora - 5 * min));
            expectEquals (Dreamer::countUnconsolidated (ws, 0), 4);
            auto r = Dreamer::dream (ws, diario, 0, ora);
            expect (r.slept);
            expectEquals (r.habitsWritten, 1);   // reverb.mix: una volta sola, non e' un'abitudine
            bool trovata = false;
            for (const auto& l : ws.longTermMemory)
                if (l.text.contains ("master.gain") && l.text.contains ("mosso 3 volte") && l.text.contains ("0.65")) trovata = true;
            expect (trovata, "master.gain 3 volte, ultimo valore 0.65");
            expectEquals (Dreamer::lastDreamFromDiary (diario), ora);
            expectEquals (Dreamer::countUnconsolidated (ws, ora), 0);
        }

        beginTest ("Un secondo sogno somma i movimenti nuovi senza contarli due volte");
        {
            AgentWorkspace ws;
            ws.dailyMemory.clear(); ws.longTermMemory.clear();
            for (int i = 0; i < 3; ++i)
                ws.dailyMemory.push_back (voce ("User adjusted eq.low from 0.1 to 0." + juce::String (i + 2), "interaction", ora - (30 - i) * min));
            Dreamer::dream (ws, diario, 0, ora);
            ws.dailyMemory.push_back (voce ("User adjusted eq.low from 0.4 to 0.9", "interaction", ora + 5 * min));
            auto r = Dreamer::dream (ws, diario, ora, ora + 10 * min);
            expectEquals (r.habitsWritten, 1);
            int quante = 0; bool quattro = false;
            for (const auto& l : ws.longTermMemory)
                if (l.text.contains ("eq.low")) { ++quante; quattro = l.text.contains ("mosso 4 volte") && l.text.contains ("0.9"); }
            expectEquals (quante, 1);   // sostituita, non duplicata
            expect (quattro, "3 + 1 = 4");
        }

        beginTest ("Una frase scritta due volte va in memoria lunga, una scritta una volta resta dov'e'");
        {
            AgentWorkspace ws;
            ws.dailyMemory.clear(); ws.longTermMemory.clear();
            ws.dailyMemory.push_back (voce ("Il brano e' in La minore", "general", ora - 40 * min));
            ws.dailyMemory.push_back (voce ("il brano  e' in la MINORE", "general", ora - 20 * min));
            ws.dailyMemory.push_back (voce ("Il cantante vuole piu' aria", "general", ora - 10 * min));
            auto r = Dreamer::dream (ws, diario, 0, ora);
            expectEquals (r.repeatsPromoted, 1);
            expectEquals ((int) ws.longTermMemory.size(), 1);
            expect (ws.longTermMemory[0].text.containsIgnoreCase ("la minore"));
            auto r2 = Dreamer::dream (ws, diario, ora, ora + min);
            expectEquals (r2.repeatsPromoted, 0);   // gia' in memoria lunga
        }

        beginTest ("Cio' che esce dalla memoria corrente finisce prima nel diario, mai perso");
        {
            AgentWorkspace ws;
            ws.dailyMemory.clear(); ws.longTermMemory.clear();
            const juce::int64 giorno = 24 * 3600 * 1000LL;
            ws.dailyMemory.push_back (voce ("Vecchia nota unica", "general", ora - 10 * giorno));
            ws.dailyMemory.push_back (voce ("Nota di oggi", "general", ora - min));
            auto r = Dreamer::dream (ws, diario, 0, ora);
            expectEquals (r.archived, 1);
            expectEquals ((int) ws.dailyMemory.size(), 1);
            expect (ws.dailyMemory[0].text == "Nota di oggi");
            expect (diario.loadFileAsString().contains ("Vecchia nota unica"), "la nota vecchia e' nel diario");
        }

        beginTest ("Senza nulla da riordinare il diario lo dice, e non si riscrive");
        {
            AgentWorkspace ws;
            ws.dailyMemory.clear(); ws.longTermMemory.clear();
            const auto prima = diario.loadFileAsString();
            Dreamer::dream (ws, diario, 0, ora + 99 * min);
            const auto dopo = diario.loadFileAsString();
            expect (dopo.startsWith (prima), "si aggiunge in fondo");
            expect (dopo.contains ("nulla da riordinare"));
        }

        diario.getParentDirectory().deleteRecursively();
    }
};

static DreamerTest dreamerTest;
