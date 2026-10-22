#include "WorkspaceFiles.h"
#include "AgentWorkspace.h"

namespace
{
    const char* const fileStato = ".scritto";   // quale contenuto abbiamo scritto noi, per file

    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    juce::String chiave (const juce::String& nome) { return nome + "="; }

    juce::String hashDi (const juce::String& testo) { return juce::String (testo.hashCode64()); }

    juce::String leggiScritto (const juce::File& dir, const juce::String& nome)
    {
        juce::StringArray righe;
        righe.addLines (dir.getChildFile (fileStato).loadFileAsString());
        for (const auto& r : righe)
            if (r.startsWith (chiave (nome))) return r.fromFirstOccurrenceOf ("=", false, false).trim();
        return {};
    }

    void segnaScritto (const juce::File& dir, const juce::String& nome, const juce::String& hash)
    {
        juce::StringArray righe;
        righe.addLines (dir.getChildFile (fileStato).loadFileAsString());
        righe.removeEmptyStrings();
        bool trovata = false;
        for (auto& r : righe)
            if (r.startsWith (chiave (nome))) { r = chiave (nome) + hash; trovata = true; }
        if (! trovata) righe.add (chiave (nome) + hash);
        dir.getChildFile (fileStato).replaceWithText (righe.joinIntoString ("\n") + "\n");
    }

    // "- Nome: valore" -> valore. Vuoto se la riga non c'e'.
    juce::String campo (const juce::String& testo, const juce::String& nome)
    {
        juce::StringArray righe;
        righe.addLines (testo);
        for (const auto& r : righe)
        {
            const auto t = r.trim();
            if (t.startsWith ("- " + nome + ":") || t.startsWith (nome + ":"))
                return t.fromFirstOccurrenceOf (":", false, false).trim();
        }
        return {};
    }

    // Elenco "- voce" sotto un titolo "## titolo".
    std::vector<juce::String> elenco (const juce::String& testo, const juce::String& titolo)
    {
        std::vector<juce::String> out;
        juce::StringArray righe;
        righe.addLines (testo);
        bool dentro = false;
        for (const auto& r : righe)
        {
            const auto t = r.trim();
            if (t.startsWith ("## ")) { dentro = (t.substring (3).trim() == titolo); continue; }
            if (dentro && t.startsWith ("- ") && t.length() > 2) out.push_back (t.substring (2).trim());
        }
        return out;
    }

    juce::String voci (const std::vector<juce::String>& v)
    {
        juce::String s;
        for (const auto& x : v) s << "- " << x << "\n";
        return s;
    }

    bool scrivi (const juce::File& dir, const juce::String& nome, const juce::String& contenuto)
    {
        auto f = dir.getChildFile (nome);
        if (f.existsAsFile())
        {
            const auto sulDisco = f.loadFileAsString();
            if (sulDisco == contenuto) { segnaScritto (dir, nome, hashDi (contenuto)); return true; }
            // La persona l'ha cambiato e il plugin non l'ha ancora letto: non si tocca.
            if (hashDi (sulDisco) != leggiScritto (dir, nome)) return false;
        }
        if (! f.replaceWithText (contenuto, false, false, "\n")) return false;
        segnaScritto (dir, nome, hashDi (f.loadFileAsString()));
        return true;
    }
}

juce::String WorkspaceFiles::renderIdentity (const AgentWorkspace& ws)
{
    juce::String s;
    s << u8 ("# Identit\xc3\xa0\n\n")
      << "- Nome: " << ws.identity.name << "\n"
      << "- Creatura: " << ws.identity.creature << "\n"
      << "- Vibe: " << ws.identity.vibe << "\n"
      << "- Emoji: " << ws.identity.emoji << "\n";
    return s;
}

juce::String WorkspaceFiles::renderSoul (const AgentWorkspace& ws)
{
    juce::String s;
    s << "# Anima\n\n"
      << "- Stile: " << AgentSoul::styleToString (ws.soul.style) << "\n"
      << "- Atmosfera: " << ws.soul.vibeDescription << "\n\n"
      << u8 ("## Verit\xc3\xa0 di fondo\n") << voci (ws.soul.coreTruths) << "\n"
      << "## Confini\n" << voci (ws.soul.boundaries);
    return s;
}

juce::String WorkspaceFiles::renderUser (const AgentWorkspace& ws)
{
    juce::String s;
    s << u8 ("# Chi \xc3\xa8 la persona\n\n")
      << "- Nome: " << ws.user.name << "\n"
      << "- Pronomi: " << ws.user.pronouns << "\n"
      << "- Fuso orario: " << ws.user.timezone << "\n"
      << "- Note: " << ws.user.contextNotes.replace ("\n", " ") << "\n";
    return s;
}

juce::String WorkspaceFiles::renderMemory (const AgentWorkspace& ws)
{
    juce::String s;
    s << "# Memoria (specchio, non si modifica qui)\n\n## Lunga\n";
    for (const auto& e : ws.longTermMemory) s << "- [" << e.date << "] " << e.text << "\n";
    s << "\n## Recente\n";
    const int dal = juce::jmax (0, (int) ws.dailyMemory.size() - 50);
    for (int i = dal; i < (int) ws.dailyMemory.size(); ++i)
        s << "- [" << ws.dailyMemory[(size_t) i].date << "] " << ws.dailyMemory[(size_t) i].text << "\n";
    return s;
}

void WorkspaceFiles::write (const AgentWorkspace& ws, const juce::File& dir)
{
    if (! dir.createDirectory()) return;
    scrivi (dir, "IDENTITY.md", renderIdentity (ws));
    scrivi (dir, "SOUL.md", renderSoul (ws));
    scrivi (dir, "USER.md", renderUser (ws));
    scrivi (dir, "MEMORY.md", renderMemory (ws));
}

int WorkspaceFiles::importEdits (AgentWorkspace& ws, const juce::File& dir)
{
    int letti = 0;
    auto cambiato = [&] (const juce::String& nome, juce::String& testo)
    {
        auto f = dir.getChildFile (nome);
        if (! f.existsAsFile()) return false;
        testo = f.loadFileAsString();
        return hashDi (testo) != leggiScritto (dir, nome);
    };

    juce::String t;
    if (cambiato ("IDENTITY.md", t))
    {
        ws.identity.name = campo (t, "Nome");
        ws.identity.creature = campo (t, "Creatura");
        ws.identity.vibe = campo (t, "Vibe");
        ws.identity.emoji = campo (t, "Emoji");
        segnaScritto (dir, "IDENTITY.md", hashDi (t));
        ++letti;
    }
    if (cambiato ("SOUL.md", t))
    {
        const auto stile = campo (t, "Stile");
        if (stile.isNotEmpty()) ws.soul.style = AgentSoul::styleFromString (stile);
        ws.soul.vibeDescription = campo (t, "Atmosfera");
        ws.soul.coreTruths = elenco (t, juce::String::fromUTF8 ("Verit\xc3\xa0 di fondo"));
        ws.soul.boundaries = elenco (t, "Confini");
        segnaScritto (dir, "SOUL.md", hashDi (t));
        ++letti;
    }
    if (cambiato ("USER.md", t))
    {
        ws.user.name = campo (t, "Nome");
        ws.user.pronouns = campo (t, "Pronomi");
        ws.user.timezone = campo (t, "Fuso orario");
        ws.user.contextNotes = campo (t, "Note");
        segnaScritto (dir, "USER.md", hashDi (t));
        ++letti;
    }
    return letti;
}
