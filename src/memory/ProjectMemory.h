#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Memoria dei progetti su file markdown (DECISIONI.md, punto 6).
// Il file e' la fonte: si legge e si corregge a mano. La ricerca e' un
// indice BM25 costruito in memoria all'apertura. Niente server, niente
// database: se un giorno servira' SQLite FTS5, cambia solo search().
//
// Un ricordo = una riga:
//   - [2026-10-22 14:05 | chat | attivo] testo su una riga
// Ogni ricordo ha data, fonte e stato (attivo, annullato, sostituito).
// Un ricordo non si cancella mai: si annulla, cosi' resta la storia.
class ProjectMemory
{
public:
    enum class Status { Active, Cancelled, Replaced };

    struct Entry
    {
        int          id = 0;          // 1, 2, 3... nell'ordine del file, stabile
        juce::String date;            // "2026-10-22 14:05"
        juce::String source;          // chi l'ha scritto: chat, utente, tool
        Status       status = Status::Active;
        juce::String text;            // una riga sola
    };

    struct Hit { Entry entry; double score = 0.0; };

    // `file` e' il markdown del progetto. Se non esiste viene creato alla prima scrittura.
    explicit ProjectMemory (const juce::File& file);

    // Rilegge il file. Le righe che non sono ricordi (titoli, note) restano com'erano.
    void load();

    // Aggiunge in fondo e salva. Vuoto = niente. Torna l'id, 0 se non scrive.
    int add (const juce::String& text, const juce::String& source = "chat");

    // Cambia lo stato di un ricordo e riscrive il file. False se l'id non c'e'.
    bool setStatus (int id, Status status);

    // Solo i ricordi attivi, dal piu' pertinente. Tiene conto di accenti e prefissi.
    std::vector<Hit> search (const juce::String& query, int limit = 5) const;

    const std::vector<Entry>& entries() const { return entriesList; }
    int activeCount() const;

    static juce::String statusName (Status s);

private:
    juce::File file;
    juce::StringArray lines;              // il file intero, riga per riga
    std::vector<int>  lineOfEntry;        // posizione di ogni ricordo in `lines`
    std::vector<Entry> entriesList;

    static juce::String formatLine (const Entry& e);
    static bool parseLine (const juce::String& line, Entry& out);
    static juce::StringArray tokenize (const juce::String& text);
    bool save() const;
};
