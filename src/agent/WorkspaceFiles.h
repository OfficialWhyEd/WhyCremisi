#pragma once
#include <juce_core/juce_core.h>

class AgentWorkspace;

// I file dell'anima: SOUL.md, IDENTITY.md, USER.md e MEMORY.md in una cartella che la persona
// puo' aprire e correggere a mano.
//  - write(): scrive lo stato del plugin nei file. Un file che la persona ha cambiato e che il
//    plugin non ha ancora letto non viene mai sovrascritto.
//  - importEdits(): legge SOUL, IDENTITY e USER se la persona li ha cambiati. MEMORY.md e' solo
//    uno specchio: i ricordi si cambiano coi tool memory.*, mai a mano qui.
class WorkspaceFiles
{
public:
    static void write (const AgentWorkspace& ws, const juce::File& dir);

    // Quanti file letti (0 se nessuno e' stato cambiato a mano).
    static int importEdits (AgentWorkspace& ws, const juce::File& dir);

    // Esposti per i test.
    static juce::String renderSoul (const AgentWorkspace& ws);
    static juce::String renderIdentity (const AgentWorkspace& ws);
    static juce::String renderUser (const AgentWorkspace& ws);
    static juce::String renderMemory (const AgentWorkspace& ws);
};
