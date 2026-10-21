#pragma once

#include "SampleLibrary.h"
#include <map>
#include <vector>

/** Un colpo della batteria: dove cade, quanto forte, su quale corsia. */
struct BeatNote
{
    juce::String lane;       // kick, snare, clap, hat, openhat, perc, bass808
    int pitch = 36;          // nota MIDI (mappa del Drum Rack di Live)
    double startBeat = 0.0;  // in quarti dall'inizio
    double lengthBeats = 0.25;
    int velocity = 100;
    int semitones = 0;       // solo 808: intonazione rispetto al suono originale
};

/** Un beat pensato a tavolino: stile, tempo e colpi. Si genera dal seme, sempre uguale a parita' di seme. */
struct BeatPlan
{
    juce::String style;
    double bpm = 140.0;
    int bars = 4;
    unsigned seed = 1;
    std::vector<BeatNote> notes;

    double lengthBeats() const { return bars * 4.0; }
    double lengthSeconds() const { return lengthBeats() * 60.0 / bpm; }
    juce::StringArray lanes() const;
};

class BeatBuilder
{
public:
    /** Stili noti. */
    static juce::StringArray styleNames();

    /** Riconosce lo stile da parole libere ("trap scuro", "boom bap", "casa"). Vuoto se non capisce. */
    static juce::String styleFromText (const juce::String& text);

    /** Tempo tipico dello stile. */
    static double defaultBpm (const juce::String& style);

    /** Nota MIDI del Drum Rack per la corsia. */
    static int pitchForLane (const juce::String& lane);

    /** Crea il disegno del beat. Stile sconosciuto -> trap. bpm <= 0 -> tempo tipico. */
    static BeatPlan plan (const juce::String& style, double bpm, int bars, unsigned seed);

    struct RenderResult
    {
        bool ok = false;
        juce::String error;
        juce::File folder;
        juce::File mix;
        std::map<juce::String, juce::File> stems;
        std::map<juce::String, SampleInfo> used;   // suono scelto per ogni corsia
        juce::StringArray missing;                 // corsie senza suono nell'indice
        double seconds = 0.0;
        float peakDb = -100.0f;
    };

    /** Sceglie i suoni dall'indice, mette ogni colpo sulla griglia e scrive mix.wav + uno stem per corsia
        (stessa lunghezza, tutti allineati alla griglia: si trascinano in Live e partono insieme). */
    static RenderResult render (const BeatPlan& plan, const SampleLibrary& library,
                                const juce::String& tasteText, const juce::File& outFolder);
};
