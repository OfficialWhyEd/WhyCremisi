#include "ReaperDawHandler.h"

void ReaperDawHandler::play()
{
    if (sendCallback) sendCallback("/play", 1.0f);
}

void ReaperDawHandler::stop()
{
    if (sendCallback) sendCallback("/stop", 1.0f);
}

void ReaperDawHandler::record()
{
    if (sendCallback) sendCallback("/record", 1.0f);
}

void ReaperDawHandler::pause()
{
    if (sendCallback) sendCallback("/pause", 1.0f);
}

void ReaperDawHandler::setTempo(float bpm)
{
    if (sendCallback) sendCallback("/tempo", bpm);
}

void ReaperDawHandler::setVolume(int trackId, float value)
{
    if (sendCallback) sendCallback("/track/" + juce::String(trackId) + "/volume", value);
}

void ReaperDawHandler::setPan(int trackId, float value)
{
    if (sendCallback) sendCallback("/track/" + juce::String(trackId) + "/pan", value);
}

void ReaperDawHandler::muteTrack(int trackId, bool muted)
{
    if (sendCallback) sendCallback("/track/" + juce::String(trackId) + "/mute", muted ? 1.0f : 0.0f);
}

void ReaperDawHandler::soloTrack(int trackId, bool soloed)
{
    if (sendCallback) sendCallback("/track/" + juce::String(trackId) + "/solo", soloed ? 1.0f : 0.0f);
}

void ReaperDawHandler::gotoMarker(int index)
{
    if (sendCallback) sendCallback("/marker/goto", (float)index);
}

void ReaperDawHandler::setMarker()
{
    if (sendCallback) sendCallback("/marker/insert", 1.0f);
}

void ReaperDawHandler::prevMarker()
{
    if (sendCallback) sendCallback("/marker/prev", 1.0f);
}

void ReaperDawHandler::nextMarker()
{
    if (sendCallback) sendCallback("/marker/next", 1.0f);
}

void ReaperDawHandler::selectTrack(int index)
{
    if (sendCallback) sendCallback("/track/" + juce::String(index) + "/select", 1.0f);
}

void ReaperDawHandler::setFxParam(int trackId, int fxId, int paramId, float value)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(trackId)
                     + "/fx/" + juce::String(fxId)
                     + "/param/" + juce::String(paramId), value);
}
