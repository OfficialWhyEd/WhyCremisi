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

    juce::String getName() const override { return "REAPER"; }
};
