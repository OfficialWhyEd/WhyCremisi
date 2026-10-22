#include "../WorkspaceFiles.h"
#include "../AgentWorkspace.h"
#include <juce_core/juce_core.h>

class WorkspaceFilesTest : public juce::UnitTest
{
public:
    WorkspaceFilesTest() : juce::UnitTest ("WorkspaceFilesTest", "whycremisi") {}

    void runTest() override
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("wc-anima-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));

        beginTest ("I quattro file vengono scritti e leggibili");
        {
            AgentWorkspace ws;
            ws.user.name = "Edo";
            ws.user.pronouns = "loro";
            ws.addMemoryEntry ("Il brano e' in La minore", "general");
            ws.longTermMemory.push_back ({ "2026-10-22", "Preferisce il basso corto", "long-term", 1 });
            WorkspaceFiles::write (ws, dir);
            expect (dir.getChildFile ("SOUL.md").existsAsFile());
            expect (dir.getChildFile ("IDENTITY.md").existsAsFile());
            expect (dir.getChildFile ("USER.md").loadFileAsString().contains ("Nome: Edo"));
            expect (dir.getChildFile ("MEMORY.md").loadFileAsString().contains ("Preferisce il basso corto"));
            expectEquals (WorkspaceFiles::importEdits (ws, dir), 0);   // nulla cambiato a mano
        }

        beginTest ("Una modifica a mano viene letta una volta sola");
        {
            AgentWorkspace ws;
            WorkspaceFiles::write (ws, dir);
            auto f = dir.getChildFile ("USER.md");
            f.replaceWithText ("# Chi e' la persona\n\n- Nome: Giulia\n- Pronomi: lei\n- Fuso orario: Europe/Rome\n- Note: lavora di notte\n");
            expectEquals (WorkspaceFiles::importEdits (ws, dir), 1);
            expectEquals (ws.user.name, juce::String ("Giulia"));
            expectEquals (ws.user.timezone, juce::String ("Europe/Rome"));
            expectEquals (ws.user.contextNotes, juce::String ("lavora di notte"));
            expectEquals (WorkspaceFiles::importEdits (ws, dir), 0);   // gia' letto
        }

        beginTest ("Il plugin non cancella una modifica non ancora letta");
        {
            AgentWorkspace ws;
            WorkspaceFiles::write (ws, dir);
            auto f = dir.getChildFile ("SOUL.md");
            f.replaceWithText ("# Anima\n\n- Stile: Direct\n- Atmosfera: secca\n\n## Confini\n- mai toccare il master senza dirlo\n");
            ws.soul.vibeDescription = "cambiata dal plugin";
            WorkspaceFiles::write (ws, dir);   // il file di Whyed resta
            expect (f.loadFileAsString().contains ("secca"));
            expectEquals (WorkspaceFiles::importEdits (ws, dir), 1);
            expectEquals (ws.soul.vibeDescription, juce::String ("secca"));
            expectEquals ((int) ws.soul.boundaries.size(), 1);
            expect (ws.soul.boundaries[0].contains ("master"));
        }

        beginTest ("Dopo l'importazione il plugin riscrive il file col suo formato");
        {
            AgentWorkspace ws;
            WorkspaceFiles::write (ws, dir);
            auto f = dir.getChildFile ("IDENTITY.md");
            f.replaceWithText ("- Nome: Cremisi\n- Creatura: volpe\n- Vibe: calma\n- Emoji: x\n");
            WorkspaceFiles::importEdits (ws, dir);
            WorkspaceFiles::write (ws, dir);
            expect (f.loadFileAsString().startsWith ("# Identit"));
            expect (f.loadFileAsString().contains ("Creatura: volpe"));
        }

        dir.deleteRecursively();
    }
};

static WorkspaceFilesTest workspaceFilesTest;
