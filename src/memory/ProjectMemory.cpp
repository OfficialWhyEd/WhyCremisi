#include "ProjectMemory.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace
{
    juce::juce_wchar senzaAccento (juce::juce_wchar c)
    {
        switch (c)
        {
            case 0xE0: case 0xE1: case 0xE2: case 0xE4: return 'a';
            case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';
            case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i';
            case 0xF2: case 0xF3: case 0xF4: case 0xF6: return 'o';
            case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u';
            default: return c;
        }
    }
}

ProjectMemory::ProjectMemory (const juce::File& f) : file (f) { load(); }

juce::String ProjectMemory::statusName (Status s)
{
    switch (s)
    {
        case Status::Cancelled: return "annullato";
        case Status::Replaced:  return "sostituito";
        default:                return "attivo";
    }
}

juce::String ProjectMemory::formatLine (const Entry& e)
{
    return "- [" + e.date + " | " + e.source + " | " + statusName (e.status) + "] " + e.text;
}

bool ProjectMemory::parseLine (const juce::String& line, Entry& out)
{
    if (! line.startsWith ("- [")) return false;
    const int close = line.indexOfChar (']');
    if (close < 0) return false;
    auto parts = juce::StringArray::fromTokens (line.substring (3, close), "|", "");
    if (parts.size() != 3) return false;

    const auto st = parts[2].trim();
    if      (st == "attivo")     out.status = Status::Active;
    else if (st == "annullato")  out.status = Status::Cancelled;
    else if (st == "sostituito") out.status = Status::Replaced;
    else return false;

    out.date   = parts[0].trim();
    out.source = parts[1].trim();
    out.text   = line.substring (close + 1).trim();
    return out.date.isNotEmpty();
}

void ProjectMemory::load()
{
    lines.clear();
    lineOfEntry.clear();
    entriesList.clear();
    if (! file.existsAsFile()) return;

    lines.addLines (file.loadFileAsString());
    for (int i = 0; i < lines.size(); ++i)
    {
        Entry e;
        if (parseLine (lines[i], e))
        {
            e.id = (int) entriesList.size() + 1;
            entriesList.push_back (e);
            lineOfEntry.push_back (i);
        }
    }
}

bool ProjectMemory::save() const
{
    if (! file.getParentDirectory().createDirectory()) return false;
    // Prima in un file accanto, poi lo scambio: un crash a meta' non lascia mezzo ricordo.
    juce::TemporaryFile tmp (file);
    if (! tmp.getFile().replaceWithText (lines.joinIntoString ("\n") + "\n")) return false;
    return tmp.overwriteTargetFileWithTemporary();
}

int ProjectMemory::add (const juce::String& text, const juce::String& source)
{
    // Una riga sola, e senza il carattere che separa i campi.
    auto pulito = text.replaceCharacters ("\r\n\t", "   ").trim();
    while (pulito.contains ("  ")) pulito = pulito.replace ("  ", " ");
    if (pulito.isEmpty()) return 0;

    Entry e;
    e.id     = (int) entriesList.size() + 1;
    e.date   = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M");
    e.source = source.replaceCharacters ("|]\r\n", "    ").trim();
    if (e.source.isEmpty()) e.source = "chat";
    e.text   = pulito;

    if (lines.isEmpty()) lines.add ("# Memoria del progetto");
    lines.add (formatLine (e));
    lineOfEntry.push_back (lines.size() - 1);
    entriesList.push_back (e);

    if (! save())
    {
        // Non salvato: torno com'ero, cosi' memoria e file non si contraddicono.
        lines.remove (lines.size() - 1);
        lineOfEntry.pop_back();
        entriesList.pop_back();
        return 0;
    }
    return e.id;
}

bool ProjectMemory::setStatus (int id, Status status)
{
    if (id < 1 || id > (int) entriesList.size()) return false;
    auto& e = entriesList[(size_t) id - 1];
    const auto prima = e.status;
    const auto rigaPrima = lines[lineOfEntry[(size_t) id - 1]];
    e.status = status;
    lines.set (lineOfEntry[(size_t) id - 1], formatLine (e));
    if (save()) return true;
    e.status = prima;
    lines.set (lineOfEntry[(size_t) id - 1], rigaPrima);
    return false;
}

int ProjectMemory::activeCount() const
{
    int n = 0;
    for (const auto& e : entriesList) if (e.status == Status::Active) ++n;
    return n;
}

juce::StringArray ProjectMemory::tokenize (const juce::String& text)
{
    juce::StringArray out;
    juce::String corrente;
    auto chiudi = [&] { if (corrente.length() >= 2) out.add (corrente); corrente.clear(); };
    for (auto c : text.toLowerCase())
    {
        c = senzaAccento (c);
        if (juce::CharacterFunctions::isLetterOrDigit (c)) corrente += juce::String::charToString (c);
        else chiudi();
    }
    chiudi();
    return out;
}

std::vector<ProjectMemory::Hit> ProjectMemory::search (const juce::String& query, int limit) const
{
    std::vector<Hit> hits;
    const auto termini = tokenize (query);
    if (termini.isEmpty() || limit < 1) return hits;

    // Documenti attivi, gia' in token.
    std::vector<const Entry*> docs;
    std::vector<juce::StringArray> toks;
    double lunghezzaTotale = 0.0;
    for (const auto& e : entriesList)
    {
        if (e.status != Status::Active) continue;
        docs.push_back (&e);
        toks.push_back (tokenize (e.text));
        lunghezzaTotale += toks.back().size();
    }
    if (docs.empty()) return hits;
    const double n = (double) docs.size();
    const double media = std::max (1.0, lunghezzaTotale / n);

    // In quanti documenti compare ogni termine (esatto o come inizio di parola).
    auto corrisponde = [] (const juce::String& parola, const juce::String& termine) {
        if (parola == termine) return 1.0;
        if (termine.length() >= 3 && parola.startsWith (termine)) return 0.5;
        return 0.0;
    };
    std::map<juce::String, double> idf;
    for (const auto& t : termini)
    {
        int df = 0;
        for (const auto& d : toks)
            for (const auto& p : d) if (corrisponde (p, t) > 0.0) { ++df; break; }
        idf[t] = std::log (1.0 + (n - df + 0.5) / (df + 0.5));
    }

    const double k1 = 1.5, b = 0.75;
    for (size_t i = 0; i < docs.size(); ++i)
    {
        double score = 0.0;
        for (const auto& t : termini)
        {
            double tf = 0.0;
            for (const auto& p : toks[i]) tf += corrisponde (p, t);
            if (tf <= 0.0) continue;
            score += idf[t] * (tf * (k1 + 1.0)) / (tf + k1 * (1.0 - b + b * toks[i].size() / media));
        }
        if (score > 0.0) hits.push_back ({ *docs[i], score });
    }
    std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& c) {
        return a.score != c.score ? a.score > c.score : a.entry.id > c.entry.id;   // a parita', il piu' recente
    });
    if ((int) hits.size() > limit) hits.resize ((size_t) limit);
    return hits;
}
