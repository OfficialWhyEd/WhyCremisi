#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

/** Un suono dell'utente, misurato al momento della scansione. */
struct SampleInfo
{
    juce::String path;
    juce::String name;       // nome file senza estensione
    juce::String folder;     // nome della cartella che lo contiene
    juce::String role;       // kick, snare, clap, hat, openhat, perc, bass808, bass, loop, melodic, vocal, fx, other
    double seconds = 0.0;
    float peakDb = -100.0f;
    float rmsDb = -100.0f;
    float centroidHz = 0.0f; // dove sta il "peso" dello spettro: basso = scuro
    float lowRatio = 0.0f;   // quota di energia sotto 200 Hz
    float highRatio = 0.0f;  // quota di energia sopra 5 kHz
    float bpm = 0.0f;        // solo per i loop; 0 = sconosciuto
    juce::String key;        // dal nome file ("Am", "C#"), vuoto se non scritto
    juce::int64 sizeBytes = 0;
    juce::int64 modified = 0;

    nlohmann::json toJson() const;
    static SampleInfo fromJson (const nlohmann::json& j);
};

/** Indice dei suoni: scansiona cartelle, misura ogni file, cerca per ruolo e gusto.
    Tutto locale e in C++, nessun programma esterno. L'indice resta su disco e
    una nuova scansione rilegge solo i file cambiati. */
class SampleLibrary
{
public:
    SampleLibrary();

    struct ScanReport
    {
        int found = 0;       // file audio trovati
        int analyzed = 0;    // misurati adesso
        int reused = 0;      // gia' nell'indice e invariati
        int failed = 0;      // illeggibili
        bool timedOut = false;
        double elapsedSeconds = 0.0;
    };

    /** Scansiona la cartella (anche le sottocartelle). Si ferma dopo `maxFiles`
        file o `budgetSeconds` secondi: una seconda chiamata riprende da dove era. */
    ScanReport scan (const juce::File& folder, int maxFiles = 4000, double budgetSeconds = 25.0);

    /** Misura un solo file. `valid` e' falso se il file non si legge. */
    static SampleInfo analyze (const juce::File& file, juce::AudioFormatManager& formats, bool& valid);

    /** Ruolo dal nome (file poi cartella) e, se il nome non dice niente, dalle misure. */
    static juce::String classify (const SampleInfo& info);

    /** Ruolo normalizzato: "hihat" -> "hat", "bd" -> "kick"... Vuoto se non lo conosce. */
    static juce::String normalizeRole (const juce::String& word);

    struct Query
    {
        juce::String role;          // vuoto = qualsiasi
        juce::String text;          // parole libere: "dark trap", "scuro", "vinile"
        float bpm = 0.0f;           // per i loop: vicino a questo tempo
        int limit = 10;
    };

    std::vector<SampleInfo> search (const Query& q) const;

    /** Sceglie un suono per il ruolo fra i migliori della ricerca (il seme varia la scelta). */
    bool pick (const juce::String& role, const juce::String& text, unsigned seed, SampleInfo& out) const;

    int size() const;
    std::map<juce::String, int> countByRole() const;
    void clear();

    /** Indice su disco. Percorso predefinito: dati applicazione dell'utente. */
    void setIndexFile (const juce::File& f) { indexFile = f; }
    juce::File getIndexFile() const { return indexFile; }
    bool save() const;
    bool load();

    /** Inserimento diretto (test e import). */
    void add (const SampleInfo& s);

private:
    mutable std::mutex mutex;
    std::vector<SampleInfo> samples;
    juce::File indexFile;
};
