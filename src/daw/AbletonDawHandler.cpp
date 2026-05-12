#include "AbletonDawHandler.h"

void AbletonDawHandler::play()
{
    if (sendCallback) sendCallback("/live/song/start_playing", 1.0f);
}

void AbletonDawHandler::stop()
{
    if (sendCallback) sendCallback("/live/song/stop_playing", 1.0f);
}

void AbletonDawHandler::record()
{
    if (sendCallback) sendCallback("/live/song/record", 1.0f);
}

void AbletonDawHandler::pause()
{
    if (sendCallback) sendCallback("/live/song/continue_playing", 1.0f);
}

void AbletonDawHandler::setTempo(float bpm)
{
    if (sendCallback) sendCallback("/live/song/set/tempo", bpm);
}

void AbletonDawHandler::setVolume(int trackId, float value)
{
    if (sendCallback) sendCallback("/live/track/" + juce::String(trackId) + "/set/volume", value);
}

void AbletonDawHandler::setPan(int trackId, float value)
{
    if (sendCallback) sendCallback("/live/track/" + juce::String(trackId) + "/set/panning", value);
}

void AbletonDawHandler::muteTrack(int trackId, bool muted)
{
    if (sendCallback) sendCallback("/live/track/" + juce::String(trackId) + "/set/mute", muted ? 1.0f : 0.0f);
}

void AbletonDawHandler::soloTrack(int trackId, bool soloed)
{
    if (sendCallback) sendCallback("/live/track/" + juce::String(trackId) + "/set/solo", soloed ? 1.0f : 0.0f);
}

void AbletonDawHandler::gotoMarker(int index)
{
    if (sendCallback) sendCallback("/live/song/goto/marker", (float)index);
}

void AbletonDawHandler::setMarker()
{
    if (sendCallback) sendCallback("/live/song/set/marker", 1.0f);
}

void AbletonDawHandler::prevMarker()
{
    if (sendCallback) sendCallback("/live/song/prev/marker", 1.0f);
}

void AbletonDawHandler::nextMarker()
{
    if (sendCallback) sendCallback("/live/song/next/marker", 1.0f);
}

void AbletonDawHandler::selectTrack(int index)
{
    if (sendCallback) sendCallback("/live/track/" + juce::String(index) + "/select", 1.0f);
}

void AbletonDawHandler::setFxParam(int trackId, int fxId, int paramId, float value)
{
    if (sendCallback)
        sendCallback("/live/track/" + juce::String(trackId)
                     + "/fx/" + juce::String(fxId)
                     + "/param/" + juce::String(paramId), value);
}
