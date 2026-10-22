#include "../ProjectMemory.h"
#include <juce_core/juce_core.h>

class ProjectMemoryTest : public juce::UnitTest
{
public:
    ProjectMemoryTest() : juce::UnitTest ("ProjectMemoryTest", "whycremisi") {}

    void runTest() override
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("whycremisi-memoria-test-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
        dir.createDirectory();
        const auto file = dir.getChildFile ("progetto.md");

        beginTest ("Un ricordo si scrive, si salva e si ritrova dopo la riapertura");
        {
            ProjectMemory m (file);
            expectEquals (m.add ("Il kick di questo pezzo e' a 60 Hz"), 1);
            expectEquals (m.add ("Tempo del brano 140 BPM", "utente"), 2);
            ProjectMemory riaperta (file);
            expectEquals ((int) riaperta.entries().size(), 2);
            expectEquals (riaperta.entries()[1].source, juce::String ("utente"));
            expect (file.loadFileAsString().startsWith ("# Memoria del progetto"));
        }

        beginTest ("Testo vuoto, a capo e separatori non rompono il file");
        {
            ProjectMemory m (file);
            expectEquals (m.add ("   "), 0);
            const int id = m.add ("riga uno\nriga due | con barra ] e tab\t.", "a|b]c");
            expect (id > 0);
            ProjectMemory riaperta (file);
            expectEquals ((int) riaperta.entries().size(), id);
            expect (! riaperta.entries().back().text.containsAnyOf ("\n\t"));
            expectEquals (riaperta.entries().back().source, juce::String ("a b c"));
        }

        beginTest ("La ricerca ordina per pertinenza, ignora gli accenti e capisce i prefissi");
        {
            ProjectMemory m (file);
            m.add ("La voce principale va compressa con rapporto 3:1");
            m.add ("Il basso e' troppo forte sotto 80 Hz");
            auto hit = m.search ("compressione voce", 3);
            expect (! hit.empty());
            if (! hit.empty())
                expect (hit[0].entry.text.contains ("voce principale"));
            // Gli accenti sono scritti come byte UTF-8, che e' quello che JUCE si aspetta.
            m.add (juce::String::fromUTF8 ("Perch\xc3\xa9 il synth stona? Tonalit\xc3\xa0 sbagliata"));
            auto accenti = m.search ("perche tonalita", 3);
            expect (! accenti.empty());
            if (! accenti.empty())
                expect (accenti[0].entry.text.startsWith ("Perch"));
            expect (! m.search (juce::String::fromUTF8 ("tonalit\xc3\xa0")).empty());
            expect (m.search ("pianoforte").empty());
            expect (m.search ("").empty());
        }

        beginTest ("Un ricordo annullato non esce piu' ma resta nel file");
        {
            ProjectMemory m (file);
            const int id = m.add ("Il ritornello e' in Do maggiore");
            expect (! m.search ("ritornello").empty());
            expect (m.setStatus (id, ProjectMemory::Status::Cancelled));
            expect (m.search ("ritornello").empty());
            expect (file.loadFileAsString().contains ("annullato] Il ritornello"));
            ProjectMemory riaperta (file);
            expect (riaperta.search ("ritornello").empty());
            expect (riaperta.activeCount() < (int) riaperta.entries().size());
            expect (! m.setStatus (9999, ProjectMemory::Status::Cancelled));
        }

        beginTest ("Le righe scritte a mano nel file restano come sono");
        {
            auto mano = dir.getChildFile ("mano.md");
            mano.replaceWithText ("# Titolo mio\n\nUna nota libera.\n- [2026-10-01 10:00 | utente | attivo] Chitarra in drop D\n");
            ProjectMemory m (mano);
            expectEquals ((int) m.entries().size(), 1);
            m.add ("Aggiunta dopo");
            m.setStatus (1, ProjectMemory::Status::Replaced);
            auto testo = mano.loadFileAsString();
            expect (testo.contains ("# Titolo mio"));
            expect (testo.contains ("Una nota libera."));
            expect (testo.contains ("sostituito] Chitarra in drop D"));
        }

        dir.deleteRecursively();
    }
};

static ProjectMemoryTest projectMemoryTest;
