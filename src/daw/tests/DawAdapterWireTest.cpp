/*
  ==============================================================================
  DawAdapterWireTest.cpp
  PercheCremisi — adattatori DAW provati sul filo

  Qui non serve nessuna DAW: si verifica COSA l'adattatore scriverebbe sulla
  rete, cioe' indirizzo e valori. E' il modo per avere certezze su REAPER e
  Ableton anche senza Ableton installato.

  Il contratto, per tutti: traccia 0-based, volume in dB.
  REAPER aggiunge +1 e usa /volume/db; Live converte nella curva del fader.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../ReaperDawHandler.h"
#include "../AbletonDawHandler.h"
#include "../DawCapabilities.h"
#include "../../ai/ToolRegistry.h"

class DawAdapterWireTest : public juce::UnitTest
{
public:
    DawAdapterWireTest() : juce::UnitTest ("DawAdapterWire", "whycremisi") {}

    void runTest() override
    {
        testReaperIndirizzi();
        testReaperMarkerNonDisponibili();
        testAbletonIndirizzi();
        testCapacita();
        testResaEsiti();
    }

private:
    struct Invio { juce::String addr; float val; };

    static std::vector<Invio> registraReaper (std::function<void (ReaperDawHandler&)> f)
    {
        std::vector<Invio> inviati;
        ReaperDawHandler h;
        h.setSendCallback ([&inviati] (const juce::String& a, float v) { inviati.push_back ({ a, v }); });
        f (h);
        return inviati;
    }

    struct InvioMulti { juce::String addr; std::vector<float> vals; };

    static std::vector<InvioMulti> registraAbleton (std::function<void (AbletonDawHandler&)> f)
    {
        std::vector<InvioMulti> inviati;
        AbletonDawHandler h;
        h.setSendMultiCallback ([&inviati] (const juce::String& a, const std::vector<float>& v,
                                            const std::vector<bool>&) { inviati.push_back ({ a, v }); });
        f (h);
        return inviati;
    }

    void testReaperIndirizzi()
    {
        beginTest ("REAPER: indice +1, volume in dB, tempo raw, fxparam e bypass");

        auto v = registraReaper ([] (ReaperDawHandler& h) { h.setVolumeDb (1, -6.0f); });
        expectEquals ((int) v.size(), 1);
        expectEquals (v[0].addr, juce::String ("/track/2/volume/db"), "traccia 1 (0-based) = /track/2");
        expectWithinAbsoluteError (v[0].val, -6.0f, 0.001f);

        auto p = registraReaper ([] (ReaperDawHandler& h) { h.setPan (0, -0.5f); });
        expectEquals (p[0].addr, juce::String ("/track/1/pan"));

        auto m = registraReaper ([] (ReaperDawHandler& h) { h.muteTrack (2, true); });
        expectEquals (m[0].addr, juce::String ("/track/3/mute"));
        expectWithinAbsoluteError (m[0].val, 1.0f, 0.001f);

        auto t = registraReaper ([] (ReaperDawHandler& h) { h.setTempo (100.0f); });
        expectEquals (t[0].addr, juce::String ("/tempo/raw"), "i BPM vanno su /tempo/raw, non su /tempo");
        expectWithinAbsoluteError (t[0].val, 100.0f, 0.001f);

        auto f = registraReaper ([] (ReaperDawHandler& h) { h.setFxParam (2, 0, 3, 0.25f); });
        expectEquals (f[0].addr, juce::String ("/track/3/fx/1/fxparam/4/value"));

        auto b = registraReaper ([] (ReaperDawHandler& h) { h.setFxBypass (0, 1, true); });
        expectEquals (b[0].addr, juce::String ("/track/1/fx/2/bypass"));
    }

    void testReaperMarkerNonDisponibili()
    {
        beginTest ("REAPER: i marker non mandano niente (solo lettura nella sua OSC)");

        auto v = registraReaper ([] (ReaperDawHandler& h) {
            h.gotoMarker (3);
            h.setMarker();
            h.prevMarker();
            h.nextMarker();
        });
        expectEquals ((int) v.size(), 0);
    }

    void testAbletonIndirizzi()
    {
        beginTest ("Ableton: indice come argomento, volume convertito, bypass e cue point");

        auto v = registraAbleton ([] (AbletonDawHandler& h) { h.setVolumeDb (1, -6.0f); });
        expectEquals ((int) v.size(), 1);
        expectEquals (v[0].addr, juce::String ("/live/track/set/volume"));
        expectEquals ((int) v[0].vals.size(), 2);
        expectWithinAbsoluteError (v[0].vals[0], 1.0f, 0.001f);
        expectWithinAbsoluteError (v[0].vals[1], AbletonDawHandler::liveVolumeFromDb (-6.0f), 0.001f);

        auto m = registraAbleton ([] (AbletonDawHandler& h) { h.muteTrack (1, true); });
        expectEquals (m[0].addr, juce::String ("/live/track/set/mute"));

        auto b = registraAbleton ([] (AbletonDawHandler& h) { h.setFxBypass (0, 2, true); });
        expectEquals (b[0].addr, juce::String ("/live/device/set/parameter/value"));
        expectWithinAbsoluteError (b[0].vals[3], 0.0f, 0.001f);   // 0 = device spento

        auto c = registraAbleton ([] (AbletonDawHandler& h) { h.gotoMarker (2); });
        expectEquals (c[0].addr, juce::String ("/live/song/cue_point/jump"));
    }

    void testCapacita()
    {
        beginTest ("Capacita': cosa si offre al modello per DAW");

        expect (DawCaps::supports ("daw.track.setVolume", DawType::Reaper));
        expect (DawCaps::supports ("daw.track.setVolume", DawType::Ableton));
        expect (DawCaps::supports ("dsp.getState", DawType::Reaper));

        expect (! DawCaps::supports ("device.list", DawType::Reaper), "device.* e' solo di Live");
        expect (DawCaps::supports ("device.list", DawType::Ableton));

        expect (! DawCaps::supports ("daw.marker.set", DawType::Reaper), "REAPER: marker solo in lettura");
        expect (DawCaps::supports ("daw.marker.set", DawType::Ableton), "Live: cue point");

        expect (! DawCaps::supports ("daw.plugin.setParam", DawType::Ableton), "tool sospeso");
        expect (! DawCaps::supports ("daw.plugin.setParam", DawType::Reaper), "tool sospeso");

        const auto perReaper = DawCaps::toolsFor (DawType::Reaper);
        expect (perReaper.contains ("daw.track.setVolume"));
        expect (! perReaper.contains ("music.compose"));

        const auto blocco = DawCaps::promptBlock (DawType::Reaper);
        expect (blocco.contains ("REAPER"));
        expect (blocco.contains ("music.compose"), "il prompt deve dire cosa NON esiste");
    }

    void testResaEsiti()
    {
        beginTest ("Resa degli esiti: stessa forma per ogni DAW");

        ToolResult ok;
        ok.name = "daw.track.setVolume";
        ok.success = true;
        ok.outcome = ToolOutcome::Done;
        ok.confirmation = ToolConfirmation::Confirmed;
        ok.output = "traccia 2 a -6.0 dB";
        auto testo = renderToolResult (ok);
        expect (testo.startsWith ("OK:"));
        expect (testo.contains ("[confermato dalla DAW]"));

        ToolResult nonConf;
        nonConf.name = "daw.transport.record";
        nonConf.outcome = ToolOutcome::Unconfirmed;
        nonConf.confirmation = ToolConfirmation::Unconfirmed;
        nonConf.output = "toggle inviato";
        expect (renderToolResult (nonConf).contains ("[la DAW non ha confermato]"));

        ToolResult unsup;
        unsup.name = "device.list";
        unsup.outcome = ToolOutcome::Unsupported;
        unsup.output = DawCaps::reasonFor ("device.list", DawType::Reaper);
        auto t2 = renderToolResult (unsup);
        expect (t2.startsWith ("NON SUPPORTATO:"));
        expect (t2.contains ("REAPER"));

        ToolResult rej;
        rej.name = "daw.track.setVolume";
        rej.outcome = ToolOutcome::Rejected;
        rej.output = "volume deve essere un numero";
        expect (renderToolResult (rej).startsWith ("RIFIUTATO:"));
    }
};

static DawAdapterWireTest dawAdapterWireTest;
