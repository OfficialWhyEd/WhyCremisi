#pragma once
#include "IDawHandler.h"
#include "McuProtocol.h"
#include <array>
#include <functional>

// Rete di sicurezza per ogni DAW che sa fare da "console Mackie": manda messaggi MCU dove la
// persona ha scelto la porta. Limiti detti chiaramente, niente finzioni:
//  - solo gli otto canali del banco attuale (trackId da 0 a 7), non c'e' selezione di banco;
//  - mute e solo sono tasti a scatto: si preme solo se lo stato noto (dal LED che la DAW
//    rimanda) e' diverso, e se non lo conosciamo il comando e' segnalato come non confermato;
//  - il pan assoluto, il tempo e i marker non esistono nel protocollo: non si fingono.
class MackieDawHandler : public IDawHandler
{
public:
    using SendMidi = std::function<void (const juce::MidiMessage&)>;
    void setMidiSender (SendMidi s) { send = std::move (s); }

    // Da chiamare con ogni messaggio che arriva dalla DAW (LED e fader).
    void handleIncoming (const juce::MidiMessage& m);

    // Stato noto dai messaggi della DAW. -1 = non lo sappiamo.
    int knownMute (int strip) const { return inRange (strip) ? mute[(size_t) strip] : -1; }
    int knownSolo (int strip) const { return inRange (strip) ? solo[(size_t) strip] : -1; }
    float knownFader (int strip) const { return inRange (strip) ? fadersSeen[(size_t) strip] : -1.0f; }
    bool playing() const { return playLed == 1; }

    void play() override;
    void stop() override;
    void record() override;
    void pause() override;
    void setTempo (float) override {}   // non esiste in MCU

    void setVolume (int trackId, float value) override;
    void setPan (int, float) override {}   // il pan assoluto non esiste in MCU
    void muteTrack (int trackId, bool muted) override;
    void soloTrack (int trackId, bool soloed) override;

    void gotoMarker (int) override {}
    void setMarker() override {}
    void prevMarker() override { press (McuProtocol::noteRewind); }
    void nextMarker() override { press (McuProtocol::noteForward); }

    void selectTrack (int index) override;
    void setFxParam (int, int, int, float) override {}   // non esiste in MCU

    juce::String getName() const override { return "Mackie Control"; }

private:
    static bool inRange (int strip) { return strip >= 0 && strip < McuProtocol::stripCount; }
    void press (int note);
    void toggleTo (int strip, bool wanted, std::array<int, 8>& stato, int (*nota) (int));

    SendMidi send;
    std::array<int, 8> mute { { -1, -1, -1, -1, -1, -1, -1, -1 } };
    std::array<int, 8> solo { { -1, -1, -1, -1, -1, -1, -1, -1 } };
    std::array<float, 8> fadersSeen { { -1, -1, -1, -1, -1, -1, -1, -1 } };
    int playLed = -1;
};
