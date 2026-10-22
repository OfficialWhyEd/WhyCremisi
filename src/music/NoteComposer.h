#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Pezzi da zero: il modello propone le note, il plugin le controlla e le
// corregge PRIMA di scriverle in una clip. Un LLM sbaglia spesso scala e
// durate; qui si sistema cio' che si puo' sistemare e si dice cosa e' stato
// toccato. Solo regole, nessun modello e nessuna installazione.
//
// Ritmi euclidei: algoritmo di Bjorklund (Toussaint 2005).
// Accordi e basso: gradi romani sulla scala, scritti qui da zero.
class NoteComposer
{
public:
    struct Note
    {
        int    pitch = 60;       // MIDI 0..127
        double startBeat = 0.0;  // in quarti dall'inizio della clip
        double lengthBeats = 1.0;
        int    velocity = 100;   // 1..127
    };

    struct Scale
    {
        int tonic = 0;                 // 0 = Do ... 11 = Si
        std::vector<int> degrees;      // semitoni dalla tonica, crescenti
        bool valid() const { return ! degrees.empty(); }
        bool contains (int pitch) const;
        int  nearest (int pitch) const;   // la nota della scala piu' vicina (verso il basso a parita')
    };

    // "C minor", "A dorian", "F# major", "Bb minor". Vuota se non capita.
    static Scale parseScale (const juce::String& text);

    struct Report
    {
        int dropped = 0;      // note scartate (altezza o tempo senza senso)
        int snapped = 0;      // note spostate sulla scala
        int clipped = 0;      // durate accorciate per stare nella clip
        int total = 0;
        juce::String summary() const;
    };

    // `json` e' un array di oggetti {"pitch","start","length","velocity"}.
    // Le note fuori scala vengono spostate sulla piu' vicina, le durate
    // tagliate alla fine della clip, le velocity portate in 1..127.
    static std::vector<Note> sanitize (const juce::String& json, const Scale& scale,
                                       double clipBeats, Report& report);

    // Bjorklund: `pulses` colpi distribuiti su `steps` passi, il piu' uniformi possibile.
    static std::vector<bool> euclidean (int pulses, int steps);

    enum class Layer { Chords, Bass, Arp };

    // Progressione in gradi romani ("i VI III VII", "I V vi IV"), un accordo
    // per battuta (4 quarti). Vuota se un grado non si capisce.
    static std::vector<Note> progression (const Scale& scale, const juce::String& romans,
                                          int bars, Layer layer, int velocity = 90);
};
