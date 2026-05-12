#pragma once

#include <juce_core/juce_core.h>
#include <functional>

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
    virtual void setVolume(int trackId, float value) = 0;
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
    virtual void setFxParam(int trackId, int fxId, int paramId, float value) = 0;

    // Query
    virtual juce::String getName() const = 0;

    // Status
    virtual bool isAbleton() const { return false; }

    // Callback type for raw OSC send (provided by OscBridge)
    using SendOscCallback = std::function<void(const juce::String&, float)>;
    virtual void setSendCallback(SendOscCallback cb) { sendCallback = std::move(cb); }

protected:
    SendOscCallback sendCallback;
};
