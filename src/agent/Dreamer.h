#pragma once
#include <juce_core/juce_core.h>

class AgentWorkspace;

// Il sogno: quando il plugin resta aperto e nessuno lo usa, riordina i ricordi.
//  - Si addormenta a caso (non a orario fisso): piu' passa il tempo fermo, piu' e' probabile.
//  - Non sogna se non c'e' nulla di nuovo da riordinare.
//  - Non inventa: mette in memoria lunga solo cio' che e' misurabile (quante volte un
//    controllo e' stato mosso, cosa e' stato scritto due volte).
//  - Non cancella: quello che esce dalla memoria corrente finisce prima nel diario DREAMS.md.
class Dreamer
{
public:
    struct Settings
    {
        bool   enabled = true;
        double graceMinutes = 10.0;     // fermo meno di cosi': mai a letto
        double lambda = 0.02;           // quanto in fretta cresce la voglia di dormire
        int    minRepeats = 3;          // quante volte un controllo deve essere mosso per diventare abitudine
        int    keepDays = 7;            // oltre, i ricordi correnti non promossi vanno nel diario
    };

    struct Report
    {
        int  habitsWritten = 0;         // abitudini nuove o aggiornate
        int  repeatsPromoted = 0;       // frasi ripetute portate in memoria lunga
        int  archived = 0;              // ricordi correnti spostati nel diario
        bool slept = false;
        juce::int64 dreamedAtMs = 0;
    };

    // Probabilita' di addormentarsi in questo giro di controllo.
    static double sleepProbability (const Settings& s, double idleMinutes, double tickMinutes, int unconsolidated);

    // Tira il dado con la probabilita' sopra.
    static bool shouldFallAsleep (const Settings& s, double idleMinutes, double tickMinutes, int unconsolidated, juce::Random& rng);

    // Quanti ricordi correnti sono piu' nuovi dell'ultimo sogno.
    static int countUnconsolidated (const AgentWorkspace& ws, juce::int64 lastDreamMs);

    // Riordina. Scrive il diario nel file dato. nowMs e' passato da fuori per poterlo provare.
    static Report dream (AgentWorkspace& ws, const juce::File& diary, juce::int64 lastDreamMs, juce::int64 nowMs, const Settings& s = {});

    // Legge dall'ultimo diario quando e' stato l'ultimo sogno (0 se mai).
    static juce::int64 lastDreamFromDiary (const juce::File& diary);
};
