#pragma once

#include "IDawHandler.h"

class ReaperDawHandler : public IDawHandler
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

    juce::String getName() const override { return "REAPER"; }
    bool isReaper() const override { return true; }

    /** dB -> posizione del fader (0..1) secondo la curva misurata di REAPER.
        Pubblica per poterla provare senza DAW. */
    static float faderPositionForDb (float db);
};
