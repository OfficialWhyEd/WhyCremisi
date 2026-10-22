#include "Dreamer.h"
#include "AgentWorkspace.h"
#include <cmath>
#include <map>

namespace
{
    const juce::String marcaAbitudine = "[sogno] Controllo ";

    juce::String normalizza (const juce::String& t)
    {
        juce::StringArray parole;
        parole.addTokens (t.toLowerCase(), " \t\r\n", "");
        parole.removeEmptyStrings();
        return parole.joinIntoString (" ");
    }

    // "User adjusted <id> from a to b"  oppure  "AI ... (<id>: a -> b)". Vuoto se non e' uno dei due.
    bool leggiMovimento (const MemoryEntry& e, juce::String& id, juce::String& valore)
    {
        if (e.category == "interaction")
        {
            const auto t = e.text;
            const auto p = juce::String ("User adjusted ");
            if (! t.startsWith (p)) return false;
            const int da = t.indexOf (p.length(), " from ");
            const int a = t.lastIndexOf (" to ");
            if (da < 0 || a < da) return false;
            id = t.substring (p.length(), da).trim();
            valore = t.substring (a + 4).trim();
            return id.isNotEmpty() && valore.isNotEmpty();
        }
        if (e.category == "ai_action")
        {
            const auto t = e.text;
            const int apre = t.lastIndexOfChar ('(');
            const int due = t.indexOfChar (apre < 0 ? 0 : apre, ':');
            const int freccia = t.lastIndexOf ("->");
            if (apre < 0 || due < apre || freccia < due) return false;
            id = t.substring (apre + 1, due).trim();
            valore = t.substring (freccia + 2).trimCharactersAtEnd (")").trim();
            return id.isNotEmpty() && valore.isNotEmpty();
        }
        return false;
    }

    juce::String dataOra (juce::int64 ms)
    {
        return juce::Time (ms).formatted ("%Y-%m-%d %H:%M");
    }
}

double Dreamer::sleepProbability (const Settings& s, double idleMinutes, double tickMinutes, int unconsolidated)
{
    if (! s.enabled || unconsolidated <= 0) return 0.0;
    const double oltre = idleMinutes - s.graceMinutes;
    if (oltre <= 0.0 || tickMinutes <= 0.0) return 0.0;
    return 1.0 - std::exp (-s.lambda * oltre * tickMinutes);
}

bool Dreamer::shouldFallAsleep (const Settings& s, double idleMinutes, double tickMinutes, int unconsolidated, juce::Random& rng)
{
    return rng.nextDouble() < sleepProbability (s, idleMinutes, tickMinutes, unconsolidated);
}

int Dreamer::countUnconsolidated (const AgentWorkspace& ws, juce::int64 lastDreamMs)
{
    int n = 0;
    for (const auto& e : ws.dailyMemory)
        if (e.timestamp > lastDreamMs) ++n;
    return n;
}

juce::int64 Dreamer::lastDreamFromDiary (const juce::File& diary)
{
    if (! diary.existsAsFile()) return 0;
    const auto testo = diary.loadFileAsString();
    const juce::String chiave = "<!-- last-dream: ";
    const int i = testo.lastIndexOf (chiave);
    if (i < 0) return 0;
    return testo.substring (i + chiave.length()).upToFirstOccurrenceOf (" -->", false, false).getLargeIntValue();
}

Dreamer::Report Dreamer::dream (AgentWorkspace& ws, const juce::File& diary, juce::int64 lastDreamMs, juce::int64 nowMs, const Settings& s)
{
    Report rep;
    rep.dreamedAtMs = nowMs;
    juce::StringArray righeDiario;

    // 1. Abitudini: quante volte e' stato mosso ogni controllo (conta solo i movimenti nuovi
    //    e li somma a quanto gia' scritto, cosi' archiviare i vecchi non fa scendere il numero).
    struct Mosso { int nuovi = 0; juce::String ultimo; juce::int64 tsUltimo = 0; };
    std::map<juce::String, Mosso> mossi;
    for (const auto& e : ws.dailyMemory)
    {
        juce::String id, v;
        if (e.timestamp <= lastDreamMs || ! leggiMovimento (e, id, v)) continue;
        auto& m = mossi[id];
        ++m.nuovi;
        if (e.timestamp >= m.tsUltimo) { m.tsUltimo = e.timestamp; m.ultimo = v; }
    }
    for (const auto& [id, m] : mossi)
    {
        const auto prefisso = marcaAbitudine + id + ":";
        int prima = 0;
        int posto = -1;
        for (int i = 0; i < (int) ws.longTermMemory.size(); ++i)
            if (ws.longTermMemory[(size_t) i].text.startsWith (prefisso))
            {
                posto = i;
                const auto t = ws.longTermMemory[(size_t) i].text;
                prima = t.fromFirstOccurrenceOf ("mosso ", false, false).getIntValue();
            }
        const int totale = prima + m.nuovi;
        if (totale < s.minRepeats) continue;

        MemoryEntry nuova;
        nuova.text = prefisso + " mosso " + juce::String (totale) + " volte, ultimo valore " + m.ultimo + ".";
        nuova.category = "long-term";
        nuova.date = dataOra (nowMs).substring (0, 10);
        nuova.timestamp = nowMs;
        if (posto >= 0) ws.longTermMemory.erase (ws.longTermMemory.begin() + posto);
        ws.longTermMemory.push_back (nuova);
        ++rep.habitsWritten;
        righeDiario.add ("- abitudine: " + nuova.text);
    }

    // 2. Frasi scritte piu' volte (non i movimenti): una copia va in memoria lunga.
    std::map<juce::String, int> conteggio;
    for (const auto& e : ws.dailyMemory)
    {
        juce::String id, v;
        if (e.category == "long-term" || leggiMovimento (e, id, v)) continue;
        ++conteggio[normalizza (e.text)];
    }
    std::map<juce::String, bool> fatto;
    for (const auto& e : ws.dailyMemory)
    {
        juce::String id, v;
        if (e.category == "long-term" || leggiMovimento (e, id, v)) continue;
        const auto chiave = normalizza (e.text);
        if (conteggio[chiave] < 2 || fatto[chiave]) continue;
        fatto[chiave] = true;
        bool giaQui = false;
        for (const auto& l : ws.longTermMemory) if (normalizza (l.text) == chiave) { giaQui = true; break; }
        if (giaQui) continue;
        MemoryEntry nuova = e;
        nuova.category = "long-term";
        nuova.timestamp = nowMs;
        ws.longTermMemory.push_back (nuova);
        ++rep.repeatsPromoted;
        righeDiario.add ("- ripetuto " + juce::String (conteggio[chiave]) + " volte: " + e.text);
    }

    // 3. I ricordi correnti vecchi escono, ma prima vanno nel diario.
    const juce::int64 limite = nowMs - (juce::int64) s.keepDays * 24 * 3600 * 1000;
    juce::StringArray archiviati;
    for (auto it = ws.dailyMemory.begin(); it != ws.dailyMemory.end();)
    {
        if (it->timestamp > 0 && it->timestamp < limite)
        {
            archiviati.add ("  - [" + it->date + "] (" + it->category + ") " + it->text);
            it = ws.dailyMemory.erase (it);
        }
        else ++it;
    }
    rep.archived = archiviati.size();
    if (rep.archived > 0)
    {
        righeDiario.add ("- archiviati " + juce::String (rep.archived) + " ricordi vecchi:");
        righeDiario.addArray (archiviati);
    }
    if (righeDiario.isEmpty()) righeDiario.add ("- nulla da riordinare, tutto gia' al suo posto.");

    // 4. Diario: si aggiunge in fondo, mai riscritto.
    juce::String blocco;
    if (! diary.existsAsFile()) blocco << "# Diario dei sogni" << "\n\n";
    blocco << "## " << dataOra (nowMs) << "\n" << righeDiario.joinIntoString ("\n") << "\n"
           << "<!-- last-dream: " << juce::String (nowMs) << " -->" << "\n\n";
    diary.getParentDirectory().createDirectory();
    rep.slept = diary.appendText (blocco);
    return rep;
}
