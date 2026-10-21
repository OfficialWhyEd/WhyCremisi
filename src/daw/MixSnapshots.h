#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/** Lo stato del mixer di una traccia, come l'ha letto Live. */
struct MixTrackState
{
    int    index    = 0;
    double volumeDb = 0.0;   // -100 = silenzio
    double pan      = 0.0;   // -1 sinistra, +1 destra
    bool   mute     = false;
};

/** Istantanee del mix con nome: si salvano su disco e si richiamano riga per riga. */
class MixSnapshots
{
public:
    /** Volume normalizzato di Live (0.85 = 0 dB, 1.0 = +6 dB) -> dB. */
    static double dbFromLiveVolume (double v);

    /** Dove vive il file. Di norma la cartella dati dell'utente; i test lo spostano. */
    void setFile (const juce::File& f) { file = f; }
    juce::File getFile() const;

    bool save (const juce::String& name, const std::vector<MixTrackState>& tracks);
    bool recall (const juce::String& name, std::vector<MixTrackState>& out) const;
    juce::StringArray names() const;

private:
    juce::File file;
};
