#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Biblioteca di skill in formato SKILL.md (cartella con frontmatter name/description).
// E' lo stesso formato di Claude Code, Codex, OpenCode, OpenClaw e Hermes: una sola
// lettura serve a tutti. Si legge solo nome e descrizione; il testo intero si carica
// quando serve (caricamento a due livelli, come fanno loro).
// Le cartelle di terzi sono marcate: il loro testo e' materiale da leggere, non ordini.
class SkillLibrary
{
public:
    struct Skill
    {
        juce::String name;
        juce::String description;
        juce::File   file;       // il SKILL.md
        juce::String source;     // etichetta della cartella radice: "claude", "codex"...
        bool         trusted = false;
    };

    // Aggiunge una cartella radice. Le prime aggiunte hanno la precedenza sui nomi doppi.
    void addRoot (const juce::File& dir, const juce::String& label, bool trusted);

    // Rilegge le radici. Torna quante skill ha trovato.
    int scan();

    const std::vector<Skill>& all() const { return skills; }
    const Skill* find (const juce::String& name) const;

    // Le piu' pertinenti a una richiesta (nome e descrizione).
    std::vector<const Skill*> search (const juce::String& query, int limit = 5) const;

    // Testo della skill senza frontmatter, tagliato a maxChars. Vuoto se non esiste.
    juce::String loadBody (const juce::String& name, int maxChars = 20000) const;

    // Legge nome e descrizione da un SKILL.md. False se manca il frontmatter o il nome.
    static bool parseFrontmatter (const juce::String& text, juce::String& name, juce::String& description, juce::String& body);

private:
    struct Root { juce::File dir; juce::String label; bool trusted; };
    std::vector<Root>  roots;
    std::vector<Skill> skills;

    void scanDir (const Root& r, const juce::File& dir, int depth);
};
