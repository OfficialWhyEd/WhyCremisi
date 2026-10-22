#include "../MackieDawHandler.h"
#include <juce_core/juce_core.h>

class MackieDawHandlerTest : public juce::UnitTest
{
public:
    MackieDawHandlerTest() : juce::UnitTest ("MackieDawHandlerTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("Messaggi del protocollo");
        {
            auto f = McuProtocol::fader (2, 1.0f);
            expect (f.isPitchWheel());
            expectEquals (f.getChannel(), 3);
            expectEquals (f.getPitchWheelValue(), 16383);
            expectEquals (McuProtocol::fader (0, 0.0f).getPitchWheelValue(), 0);
            expectEquals (McuProtocol::fader (0, 2.5f).getPitchWheelValue(), 16383);   // fuori scala: limitato

            auto d = McuProtocol::buttonDown (McuProtocol::notePlay);
            expectEquals (d.getNoteNumber(), 0x5E);
            expectEquals ((int) d.getVelocity(), 127);

            auto cw = McuProtocol::vpotTurn (1, 3);
            expectEquals (cw.getControllerNumber(), 0x11);
            expectEquals (cw.getControllerValue(), 3);
            expectEquals (McuProtocol::vpotTurn (0, -5).getControllerValue(), 0x45);

            int off = 0; juce::String text;
            const juce::uint8 lcd[] = { 0x00, 0x00, 0x66, 0x14, 0x12, 0x00, 'K', 'i', 'c', 'k' };
            expect (McuProtocol::parseLcd (juce::MidiMessage::createSysExMessage (lcd, sizeof (lcd)), off, text));
            expectEquals (text, juce::String ("Kick"));
        }

        beginTest ("Trasporto, fader e tasti inviati");
        {
            MackieDawHandler h;
            std::vector<juce::MidiMessage> sent;
            h.setMidiSender ([&] (const juce::MidiMessage& m) { sent.push_back (m); });
            h.play();
            expectEquals ((int) sent.size(), 2);
            expectEquals (sent[0].getNoteNumber(), McuProtocol::notePlay);
            sent.clear();
            h.setVolume (3, 0.5f);
            expectEquals ((int) sent.size(), 1);
            expectEquals (sent[0].getChannel(), 4);
            sent.clear();
            h.setVolume (12, 0.5f);   // fuori dal banco: niente, non si finge
            h.setPan (0, 0.5f);
            h.setTempo (120.0f);
            expect (sent.empty());
        }

        beginTest ("Mute e solo: si preme solo se lo stato noto e' diverso");
        {
            MackieDawHandler h;
            int premuti = 0;
            h.setMidiSender ([&] (const juce::MidiMessage& m) { if (m.getVelocity() == 127) ++premuti; });
            h.muteTrack (1, true);   // non lo sappiamo: una pressione
            expectEquals (premuti, 1);
            h.handleIncoming (juce::MidiMessage::noteOn (1, McuProtocol::noteMute (1), (juce::uint8) 127));   // LED acceso
            expectEquals (h.knownMute (1), 1);
            h.muteTrack (1, true);   // gia' muta: niente
            expectEquals (premuti, 1);
            h.muteTrack (1, false);
            expectEquals (premuti, 2);
            expectEquals (h.knownMute (4), -1);
        }

        beginTest ("Fader e LED di Play letti dalla DAW");
        {
            MackieDawHandler h;
            h.handleIncoming (McuProtocol::fader (5, 0.25f));
            expect (std::abs (h.knownFader (5) - 0.25f) < 0.001f);
            expect (! h.playing());
            h.handleIncoming (juce::MidiMessage::noteOn (1, McuProtocol::notePlay, (juce::uint8) 127));
            expect (h.playing());
        }
    }
};

static MackieDawHandlerTest mackieDawHandlerTest;
