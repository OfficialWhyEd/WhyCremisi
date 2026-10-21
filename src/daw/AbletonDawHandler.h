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

    void setVolume(int trackId, float value) override;
    void setPan(int trackId, float value) override;
    void muteTrack(int trackId, bool muted) override;
    void soloTrack(int trackId, bool soloed) override;

    void gotoMarker(int index) override;
    void setMarker() override;
    void prevMarker() override;
    void nextMarker() override;

    void selectTrack(int index) override;
    void setFxParam(int trackId, int fxId, int paramId, float value) override;

    /** Guadagno lineare (1.0 = 0 dB) -> valore normalizzato 0..1 del fader di Live.
        Ancoraggi certi: 0 -> 0, 0 dB -> 0.85, +6 dB -> 1.0. Fra i punti la curva
        e' un'APPROSSIMAZIONE non calibrata: chi la usa deve dirlo. */
    static float liveVolumeFromGain (float gain);

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
