#include "MackieDawHandler.h"

void MackieDawHandler::press (int note)
{
    if (! send) return;
    send (McuProtocol::buttonDown (note));
    send (McuProtocol::buttonUp (note));
}

void MackieDawHandler::handleIncoming (const juce::MidiMessage& m)
{
    int strip = 0, note = 0;
    float v = 0.0f;
    bool on = false;
    if (McuProtocol::parseFader (m, strip, v))
    {
        if (strip < McuProtocol::stripCount) fadersSeen[(size_t) strip] = v;
        return;
    }
    if (McuProtocol::parseLed (m, note, on))
    {
        for (int s = 0; s < McuProtocol::stripCount; ++s)
        {
            if (note == McuProtocol::noteMute (s)) mute[(size_t) s] = on ? 1 : 0;
            if (note == McuProtocol::noteSolo (s)) solo[(size_t) s] = on ? 1 : 0;
        }
        if (note == McuProtocol::notePlay) playLed = on ? 1 : 0;
    }
}

void MackieDawHandler::play()   { press (McuProtocol::notePlay); }
void MackieDawHandler::stop()   { press (McuProtocol::noteStop); }
void MackieDawHandler::record() { press (McuProtocol::noteRecord); }

void MackieDawHandler::pause()
{
    // MCU non ha "pausa": a console ferma si lascia stare, altrimenti Stop.
    if (playLed != 0) press (McuProtocol::noteStop);
}

void MackieDawHandler::setVolume (int trackId, float value)
{
    if (! inRange (trackId) || ! send) return;
    send (McuProtocol::fader (trackId, value));
}

void MackieDawHandler::toggleTo (int strip, bool wanted, std::array<int, 8>& stato, int (*nota) (int))
{
    if (! inRange (strip)) return;
    if (stato[(size_t) strip] == (wanted ? 1 : 0)) return;   // gia' cosi'
    press (nota (strip));   // se lo stato non e' noto si preme una volta: la conferma arriva dal LED
}

void MackieDawHandler::muteTrack (int trackId, bool muted) { toggleTo (trackId, muted, mute, McuProtocol::noteMute); }
void MackieDawHandler::soloTrack (int trackId, bool soloed) { toggleTo (trackId, soloed, solo, McuProtocol::noteSolo); }

void MackieDawHandler::selectTrack (int index)
{
    if (inRange (index)) press (McuProtocol::noteSelect (index));
}
