#include "SkillLibrary.h"
#include <algorithm>

namespace
{
    juce::String senzaVirgolette (juce::String s)
    {
        s = s.trim();
        if (s.length() >= 2 && ((s.startsWith ("\"") && s.endsWith ("\"")) || (s.startsWith ("'") && s.endsWith ("'"))))
            s = s.substring (1, s.length() - 1);
        return s.trim();
    }

    juce::StringArray parole (const juce::String& t)
    {
        juce::StringArray out;
        for (auto& p : juce::StringArray::fromTokens (t.toLowerCase(), " \t\r\n.,;:!?()[]\"'/-_", ""))
            if (p.length() >= 3) out.add (p);
        return out;
    }
}

bool SkillLibrary::parseFrontmatter (const juce::String& text, juce::String& name, juce::String& description, juce::String& body)
{
    juce::StringArray righe;
    righe.addLines (text);
    if (righe.isEmpty() || righe[0].trim() != "---") return false;

    int fine = -1;
    for (int i = 1; i < righe.size(); ++i)
        if (righe[i].trim() == "---") { fine = i; break; }
    if (fine < 0) return false;

    name.clear();
    description.clear();
    for (int i = 1; i < fine; ++i)
    {
        const auto& r = righe[i];
        const int due = r.indexOfChar (':');
        if (due <= 0 || r[0] == ' ' || r[0] == '\t') continue;
        const auto chiave = r.substring (0, due).trim().toLowerCase();
        auto valore = r.substring (due + 1).trim();
        if (chiave != "name" && chiave != "description") continue;

        // Testo su piu' righe: "description: >" o "|" seguito da righe rientrate.
        if (valore == ">" || valore == "|" || valore == ">-" || valore == "|-")
        {
            juce::StringArray parti;
            for (int k = i + 1; k < fine && (righe[k].startsWithChar (' ') || righe[k].startsWithChar ('\t')); ++k)
                parti.add (righe[k].trim());
            valore = parti.joinIntoString (" ");
        }
        valore = senzaVirgolette (valore);
        if (chiave == "name") name = valore; else description = valore;
    }
    if (name.isEmpty()) return false;

    juce::StringArray resto;
    for (int i = fine + 1; i < righe.size(); ++i) resto.add (righe[i]);
    body = resto.joinIntoString ("\n").trim();
    return true;
}

void SkillLibrary::addRoot (const juce::File& dir, const juce::String& label, bool trusted)
{
    roots.push_back ({ dir, label, trusted });
}

void SkillLibrary::scanDir (const Root& r, const juce::File& dir, int depth)
{
    if (depth > 3 || ! dir.isDirectory()) return;

    const auto skillFile = dir.getChildFile ("SKILL.md");
    if (skillFile.existsAsFile())
    {
        juce::String nome, desc, corpo;
        if (parseFrontmatter (skillFile.loadFileAsString(), nome, desc, corpo) && find (nome) == nullptr)
            skills.push_back ({ nome, desc, skillFile, r.label, r.trusted });
        return;   // dentro una skill non se ne cercano altre
    }

    for (const auto& sub : dir.findChildFiles (juce::File::findDirectories, false))
    {
        const auto n = sub.getFileName();
        if (n == "node_modules" || n.startsWithChar ('.')) continue;
        scanDir (r, sub, depth + 1);
    }
}

int SkillLibrary::scan()
{
    skills.clear();
    for (const auto& r : roots) scanDir (r, r.dir, 0);
    std::stable_sort (skills.begin(), skills.end(), [] (const Skill& a, const Skill& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    return (int) skills.size();
}

const SkillLibrary::Skill* SkillLibrary::find (const juce::String& name) const
{
    for (const auto& s : skills) if (s.name.equalsIgnoreCase (name.trim())) return &s;
    return nullptr;
}

std::vector<const SkillLibrary::Skill*> SkillLibrary::search (const juce::String& query, int limit) const
{
    const auto cerca = parole (query);
    std::vector<std::pair<int, const Skill*>> punteggi;
    for (const auto& s : skills)
    {
        int p = 0;
        const auto nome = s.name.toLowerCase();
        const auto desc = s.description.toLowerCase();
        for (const auto& w : cerca)
        {
            if (nome.contains (w)) p += 3;
            if (desc.contains (w)) p += 1;
        }
        if (p > 0) punteggi.push_back ({ p, &s });
    }
    std::stable_sort (punteggi.begin(), punteggi.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<const Skill*> out;
    for (const auto& pr : punteggi) { if ((int) out.size() >= limit) break; out.push_back (pr.second); }
    return out;
}

juce::String SkillLibrary::loadBody (const juce::String& name, int maxChars) const
{
    const auto* s = find (name);
    if (s == nullptr) return {};
    juce::String n, d, corpo;
    if (! parseFrontmatter (s->file.loadFileAsString(), n, d, corpo)) return {};
    return corpo.substring (0, maxChars);
}
