#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

class IDawHandler
{
public:
    virtual ~IDawHandler() = default;

    // Transport
    virtual void play() = 0;
    virtual void stop() = 0;
    virtual void record() = 0;
    virtual void pause() = 0;
    virtual void setTempo(float bpm) = 0;

    // Mixer
    //
    //  Unità e indici sono FISSI nel contratto: trackId 0-based, volume in
    //  dB. Ogni adattatore converte (Reaper: +1 sull'indice e /volume/db;
    //  Live: la sua curva del fader). Prima il valore era "lineare" e la
    //  scala cambiava a seconda del percorso: due semantica per lo stesso
    //  comando.
    virtual void setVolumeDb(int trackId, float db) = 0;
    virtual void setPan(int trackId, float value) = 0;
    virtual void muteTrack(int trackId, bool muted) = 0;
    virtual void soloTrack(int trackId, bool soloed) = 0;

    // Markers
    virtual void gotoMarker(int index) = 0;
    virtual void setMarker() = 0;
    virtual void prevMarker() = 0;
    virtual void nextMarker() = 0;

    // Track / FX
    virtual void selectTrack(int index) = 0;

    /** Crea una traccia nuova. Stesso verbo per tutte le DAW: cambia solo
        come si traduce (Reaper: azione 40001; Live: create_midi_track). */
    virtual void createTrack (bool midi) { juce::ignoreUnused (midi); }
    virtual void setFxParam(int trackId, int fxId, int paramId, float value) = 0;

    /** Bypass di un FX: indice 0-based nel contratto. Chi non ha un comando
        diretto lo implementa come puo' (Live: parametro "Device On"). */
    virtual void setFxBypass(int trackId, int fxId, bool bypass) { juce::ignoreUnused(trackId, fxId, bypass); }

    // Query
    virtual juce::String getName() const = 0;

    // Status
    virtual bool isAbleton() const { return false; }

    /** La conferma su REAPER si fa sul feedback in dB, non sull'API di Live:
        serve sapere di che adattatore si tratta, non solo "non Live". */
    virtual bool isReaper() const { return false; }

    // Callback type for raw OSC send (provided by OscBridge)
    using SendOscCallback = std::function<void(const juce::String&, float)>;
    virtual void setSendCallback(SendOscCallback cb) { sendCallback = std::move(cb); }

    // Invio con piu' argomenti: AbletonOSC passa l'indice della traccia
    // come argomento, non nell'indirizzo, quindi la forma a un solo float
    // non basta a esprimere i comandi sul mixer.
    using SendOscMultiCallback =
        std::function<void(const juce::String&, const std::vector<float>&, const std::vector<bool>&)>;
    virtual void setSendMultiCallback(SendOscMultiCallback cb) { sendMultiCallback = std::move(cb); }

protected:
    SendOscCallback sendCallback;
    SendOscMultiCallback sendMultiCallback;
};
