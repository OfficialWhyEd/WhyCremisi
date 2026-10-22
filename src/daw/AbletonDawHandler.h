#pragma once

#include "IDawHandler.h"

class AbletonDawHandler : public IDawHandler
{
public:
    void play() override;
    void stop() override;
    void record() override;
    void pause() override;
    void setTempo(float bpm) override;

    void setVolumeDb(int trackId, float db) override;
    void setPan(int trackId, float value) override;
    void muteTrack(int trackId, bool muted) override;
    void soloTrack(int trackId, bool soloed) override;

    void gotoMarker(int index) override;
    void setMarker() override;
    void prevMarker() override;
    void nextMarker() override;

    void selectTrack(int index) override;
    void setFxParam(int trackId, int fxId, int paramId, float value) override;
    void setFxBypass(int trackId, int fxId, bool bypass) override;

    /** Crea una clip MIDI vuota nello slot (AbletonOSC /live/clip_slot/create_clip). Lunghezza in quarti. */
    void createMidiClip (int trackId, int slotIndex, float lengthBeats);

    /** Aggiunge una nota alla clip (AbletonOSC /live/clip/add/notes). Inviato, non confermato. */
    void addMidiNote (int trackId, int slotIndex, int pitch, float startBeat, float lengthBeats, int velocity);

    /** Guadagno lineare (1.0 = 0 dB) -> valore normalizzato 0..1 del fader di Live.
        Curva misurata su Live 11 (22/10): lineare da 0.4 a 1.0 (0.85 = 0 dB,
        1.0 = +6 dB, 40 dB per unita'), piu' ripida sotto 0.4, tabella fra 0.1 e 0.4. */
    static float liveVolumeFromGain (float gain);

    /** dB -> valore normalizzato 0..1 del fader di Live (stessa curva). */
    static float liveVolumeFromDb (float db);

    /** Valore normalizzato del fader di Live -> dB (inverso di liveVolumeFromDb). */
    static float dbFromLiveVolume (float v);

    /** Pan -1..1 (come in Live): fuori scala viene limitato. */
    static float livePanFromPan (float pan);

    juce::String getName() const override { return "Ableton Live"; }
    bool isAbleton() const override { return true; }

private:
    /** Invia con piu' argomenti, ripiegando sul canale a singolo valore
        se quello multi-argomento non e' stato collegato. */
    void sendMulti (const juce::String& address,
                    const std::vector<float>& values,
                    const std::vector<bool>& intMask);
};
