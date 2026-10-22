#include "../SkillLibrary.h"
#include <juce_core/juce_core.h>

class SkillLibraryTest : public juce::UnitTest
{
public:
    SkillLibraryTest() : juce::UnitTest ("SkillLibraryTest", "whycremisi") {}

    static void scrivi (const juce::File& cartella, const juce::String& nome, const juce::String& contenuto)
    {
        auto d = cartella.getChildFile (nome);
        d.createDirectory();
        d.getChildFile ("SKILL.md").replaceWithText (contenuto);
    }

    void runTest() override
    {
        auto base = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("whycremisi-skill-test-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
        auto a = base.getChildFile ("a");
        auto b = base.getChildFile ("b");
        a.createDirectory(); b.createDirectory();

        scrivi (a, "mix-voce", "---\nname: mix-voce\ndescription: Come mixare una voce con compressore ed EQ\n---\n# Voce\nPrima l'EQ.\n");
        scrivi (a, "multi", "---\nname: \"mastering\"\ndescription: >\n  Master finale\n  con limiter e LUFS\n---\nTesto del master.\n");
        scrivi (a, "rotta", "niente frontmatter\n");
        scrivi (a.getChildFile ("plugin"), "annidata", "---\nname: annidata\ndescription: Skill dentro un plugin\n---\nCorpo.\n");
        scrivi (b, "mix-voce", "---\nname: mix-voce\ndescription: Doppione di terzi\n---\nNon deve vincere.\n");
        scrivi (b, "stem", "---\nname: stem\ndescription: Separare gli stem\n---\nUsa un separatore.\n");

        beginTest ("Si leggono le skill, anche annidate; quelle rotte restano fuori");
        {
            SkillLibrary lib;
            lib.addRoot (a, "mia", true);
            lib.addRoot (b, "terzi", false);
            expectEquals (lib.scan(), 4);   // mix-voce, mastering, annidata, stem
            expect (lib.find ("rotta") == nullptr);
            expect (lib.find ("annidata") != nullptr);
        }

        beginTest ("Con due nomi uguali vince la prima radice");
        {
            SkillLibrary lib;
            lib.addRoot (a, "mia", true);
            lib.addRoot (b, "terzi", false);
            lib.scan();
            auto* s = lib.find ("MIX-VOCE");
            expect (s != nullptr);
            if (s != nullptr) { expect (s->trusted); expectEquals (s->source, juce::String ("mia")); }
            auto* stem = lib.find ("stem");
            expect (stem != nullptr && ! stem->trusted, "quella di terzi resta marcata");
        }

        beginTest ("Descrizione su piu' righe e nome tra virgolette");
        {
            SkillLibrary lib;
            lib.addRoot (a, "mia", true);
            lib.scan();
            auto* s = lib.find ("mastering");
            expect (s != nullptr);
            if (s != nullptr) expectEquals (s->description, juce::String ("Master finale con limiter e LUFS"));
        }

        beginTest ("La ricerca pesa il nome piu' della descrizione");
        {
            SkillLibrary lib;
            lib.addRoot (a, "mia", true);
            lib.addRoot (b, "terzi", false);
            lib.scan();
            auto hit = lib.search ("separare stem", 3);
            expect (! hit.empty());
            if (! hit.empty()) expectEquals (hit[0]->name, juce::String ("stem"));
            expect (lib.search ("pianoforte").empty());
        }

        beginTest ("Il testo si carica senza frontmatter e si taglia");
        {
            SkillLibrary lib;
            lib.addRoot (a, "mia", true);
            lib.scan();
            auto corpo = lib.loadBody ("mix-voce");
            expect (corpo.startsWith ("# Voce"));
            expect (! corpo.contains ("description:"));
            expectEquals (lib.loadBody ("mix-voce", 3).length(), 3);
            expect (lib.loadBody ("inesistente").isEmpty());
        }

        base.deleteRecursively();
    }
};

static SkillLibraryTest skillLibraryTest;
