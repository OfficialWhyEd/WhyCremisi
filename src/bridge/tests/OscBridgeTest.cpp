/*
  ==============================================================================
    OscBridgeTest.cpp
    Unit tests for OscBridge

    Le regressioni sugli strumenti AI daw.* invocano il percorso produttivo
    (OscBridge::tryExecuteDawTool -> dispatchDawCommand) con un IDawHandler
    che si limita a registrare le chiamate ricevute. Il mapping non e'
    duplicato qui: si osserva cosa arriva davvero al gestore della DAW.

    Contratto dell'esito:
      result.output e' un oggetto JSON con
        status     "confirmed" | "mismatch" | "submitted" | "rejected" | "unsupported"
        confirmed  true solo se Live ha riletto il valore atteso
        dawState   "unknown", oppure {read, expected} dopo una rilettura
        message    testo leggibile
      Gestori diversi da Live: sempre "submitted" (nessun readback).
      Live (AbletonOSC): dopo il comando si rilegge il valore e si confronta.
        confirmed -> success true; mismatch / silenzio -> success false.
      rejected/unsupported -> success == false e nessun invio alla DAW.
  ==============================================================================
*/

#include "../OscBridge.h"
#include "ToolRegistry.h"
#include "IDawHandler.h"
#include "AbletonDawHandler.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <nlohmann/json.hpp>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace
{
    // Registra le chiamate al gestore DAW. Nessuna logica: e' un sink.
    struct RecordingDawHandler : public IDawHandler
    {
        struct Call
        {
            std::string fn;
            int   track = -1;
            float value = 0.0f;
            bool  flag  = false;
        };

        std::vector<Call> calls;

        void add (std::string fn, int track = -1, float value = 0.0f, bool flag = false)
        {
            calls.push_back ({ std::move (fn), track, value, flag });
        }

        void play() override                          { add ("play"); }
        void stop() override                          { add ("stop"); }
        void record() override                        { add ("record"); }
        void pause() override                         { add ("pause"); }
        void setTempo (float bpm) override            { add ("setTempo", -1, bpm); }
        void setVolume (int t, float v) override      { add ("setVolume", t, v); }
        void setPan (int t, float v) override         { add ("setPan", t, v); }
        void muteTrack (int t, bool m) override       { add ("muteTrack", t, 0.0f, m); }
        void soloTrack (int t, bool s) override       { add ("soloTrack", t, 0.0f, s); }
        void gotoMarker (int i) override              { add ("gotoMarker", i); }
        void setMarker() override                     { add ("setMarker"); }
        void prevMarker() override                    { add ("prevMarker"); }
        void nextMarker() override                    { add ("nextMarker"); }
        void selectTrack (int i) override             { add ("selectTrack", i); }
        void setFxParam (int t, int, int, float v) override { add ("setFxParam", t, v); }
        juce::String getName() const override         { return "Recording"; }
    };
}

class OscBridgeTest : public juce::UnitTest
{
public:
    // juce::UnitTestCategory non esiste: la categoria e' una stringa.
    OscBridgeTest() : juce::UnitTest ("OscBridgeTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("Construction and destruction");
        {
            OscBridge bridge (9000, 8080);
        }

        beginTest ("Port getters");
        {
            OscBridge bridge (9001, 8081);
            expect (bridge.getOscPort() == 9001, "OSC port should be 9001");
            expect (bridge.getWebSocketPort() == 8081, "WebSocket port should be 8081");
        }

        beginTest ("Initial state");
        {
            OscBridge bridge (9000, 8080);
            expect (! bridge.isRunning(), "Bridge should not be running initially");
        }

        testMixerMapping();
        testMixerRejectsMalformed();
        testMarkerGoto();
        testMarkerSet();
        testTransportAndTempo();
        testUnconfirmedOutcome();
        testLiveReadback();
        testLiveConversions();
        testUnsupportedAndUnknown();
        testUiDispatcherCompatibility();
    }

private:
    //==========================================================================
    // Infrastruttura: bridge reale + gestore che registra
    //==========================================================================
    struct Rig
    {
        OscBridge bridge { 9000, 8080 };
        RecordingDawHandler* daw = nullptr;
    };

    void install (Rig& rig)
    {
        auto h = std::make_unique<RecordingDawHandler>();
        rig.daw = h.get();
        rig.bridge.dawHandler = std::move (h);
    }

    struct Outcome
    {
        bool handled = false;
        bool threw   = false;
        ToolResult result;
        nlohmann::json out;   // output parsato (discarded se non JSON)
    };

    Outcome runTool (Rig& rig, const juce::String& name, const nlohmann::json& args)
    {
        Outcome o;
        ToolCall call;
        call.id = "t1";
        call.name = name;
        call.arguments = args;
        o.result.toolCallId = call.id;
        o.result.name = call.name;

        try
        {
            o.handled = rig.bridge.tryExecuteDawTool (call, o.result);
        }
        catch (...)
        {
            o.threw = true;
        }

        o.out = nlohmann::json::parse (o.result.output.toStdString(), nullptr, false);
        return o;
    }

    bool dispatchUi (Rig& rig, const nlohmann::json& payload)
    {
        try { rig.bridge.dispatchDawCommand (payload); return true; }
        catch (...) { return false; }
    }

    static std::string field (const nlohmann::json& j, const char* key)
    {
        if (j.is_object() && j.contains (key) && j[key].is_string())
            return j[key].get<std::string>();
        return {};
    }

    void expectSentUnconfirmed (const Outcome& o, const juce::String& what)
    {
        expect (! o.threw, what + ": nessuna eccezione");
        expect (o.handled, what + ": strumento DAW gestito");
        expect (o.out.is_object(), what + ": output JSON strutturato");
        expectEquals (juce::String (field (o.out, "status")), juce::String ("submitted"), what + ": status");
        expect (! o.result.success, what + ": success deve essere false (nessuna conferma)");
        expect (o.out.is_object() && o.out.contains ("accepted")
                  && o.out["accepted"].is_boolean() && o.out["accepted"].get<bool>(),
                what + ": accepted deve essere true");
        expect (o.out.is_object() && o.out.contains ("confirmed")
                  && o.out["confirmed"].is_boolean() && ! o.out["confirmed"].get<bool>(),
                what + ": confirmed deve essere false");
        expectEquals (juce::String (field (o.out, "dawState")), juce::String ("unknown"), what + ": dawState");
        expect (! field (o.out, "message").empty(), what + ": messaggio leggibile");
    }

    void expectNotSent (const Rig& rig, const Outcome& o, const char* status, const juce::String& what)
    {
        expect (! o.threw, what + ": nessuna eccezione");
        expect (o.handled, what + ": strumento DAW gestito");
        expect (! o.result.success, what + ": success deve essere false");
        expect (rig.daw->calls.empty(), what + ": nessun invio alla DAW");
        expectEquals (juce::String (field (o.out, "status")), juce::String (status), what + ": status");
        expect (o.out.is_object() && o.out.contains ("accepted")
                  && o.out["accepted"].is_boolean() && ! o.out["accepted"].get<bool>(),
                what + ": accepted deve essere false");
        expect (o.out.is_object() && o.out.contains ("confirmed")
                  && o.out["confirmed"].is_boolean() && ! o.out["confirmed"].get<bool>(),
                what + ": confirmed deve essere false");
    }

    void expectSingleCall (const Rig& rig, const char* fn, const juce::String& what)
    {
        expectEquals ((int) rig.daw->calls.size(), 1, what + ": una sola chiamata al gestore");
        if (! rig.daw->calls.empty())
            expectEquals (juce::String (rig.daw->calls[0].fn), juce::String (fn), what + ": funzione");
    }

    //==========================================================================
    // Mixer: campi giusti fino al gestore DAW
    //==========================================================================
    void testMixerMapping()
    {
        beginTest ("AI setVolume arriva al gestore con traccia e guadagno del percorso UI");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.track.setVolume", { { "track", 3 }, { "volume", -6.0 } });
            expectSentUnconfirmed (o, "setVolume");
            expectSingleCall (rig, "setVolume", "setVolume");
            if (! rig.daw->calls.empty())
            {
                expectEquals (rig.daw->calls[0].track, 3);
                // Stessa scala del comando UI {trackId, valueDb}: dB -> guadagno lineare.
                expectWithinAbsoluteError (rig.daw->calls[0].value,
                                           juce::Decibels::decibelsToGain (-6.0f), 1.0e-5f);
            }
        }

        beginTest ("AI setVolume ai limiti dello schema (-96 e +12 dB)");
        {
            Rig rig; install (rig);
            runTool (rig, "daw.track.setVolume", { { "track", 0 }, { "volume", -96.0 } });
            runTool (rig, "daw.track.setVolume", { { "track", 255 }, { "volume", 12.0 } });
            expectEquals ((int) rig.daw->calls.size(), 2);
            if (rig.daw->calls.size() == 2)
            {
                expectEquals (rig.daw->calls[0].track, 0);
                expectEquals (rig.daw->calls[1].track, 255);
                expectWithinAbsoluteError (rig.daw->calls[1].value,
                                           juce::Decibels::decibelsToGain (12.0f), 1.0e-4f);
            }
        }

        beginTest ("AI setPan arriva al gestore con traccia e valore");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.track.setPan", { { "track", 2 }, { "pan", -0.5 } });
            expectSentUnconfirmed (o, "setPan");
            expectSingleCall (rig, "setPan", "setPan");
            if (! rig.daw->calls.empty())
            {
                expectEquals (rig.daw->calls[0].track, 2);
                expectWithinAbsoluteError (rig.daw->calls[0].value, -0.5f, 1.0e-6f);
            }
        }

        beginTest ("AI mute on/off arriva al gestore");
        {
            Rig rig; install (rig);
            auto on  = runTool (rig, "daw.track.mute", { { "track", 1 }, { "mute", true } });
            auto off = runTool (rig, "daw.track.mute", { { "track", 1 }, { "mute", false } });
            expectSentUnconfirmed (on, "mute on");
            expectSentUnconfirmed (off, "mute off");
            expectEquals ((int) rig.daw->calls.size(), 2);
            if (rig.daw->calls.size() == 2)
            {
                expectEquals (juce::String (rig.daw->calls[0].fn), juce::String ("muteTrack"));
                expectEquals (rig.daw->calls[0].track, 1);
                expect (rig.daw->calls[0].flag, "primo: muted=true");
                expect (! rig.daw->calls[1].flag, "secondo: muted=false");
            }
        }

        beginTest ("AI solo arriva al gestore");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.track.solo", { { "track", 4 }, { "solo", true } });
            expectSentUnconfirmed (o, "solo");
            expectSingleCall (rig, "soloTrack", "solo");
            if (! rig.daw->calls.empty())
            {
                expectEquals (rig.daw->calls[0].track, 4);
                expect (rig.daw->calls[0].flag, "soloed=true");
            }
        }
    }

    //==========================================================================
    // Mixer: argomenti malformati -> nessun invio, nessuna eccezione
    //==========================================================================
    void testMixerRejectsMalformed()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();

        struct Case { const char* tool; nlohmann::json args; const char* what; };
        const std::vector<Case> cases {
            { "daw.track.setVolume", { { "volume", -6.0 } },                    "volume: track mancante" },
            { "daw.track.setVolume", { { "track", 1 } },                        "volume: volume mancante" },
            { "daw.track.setVolume", { { "track", "1" }, { "volume", -6.0 } },  "volume: track stringa" },
            { "daw.track.setVolume", { { "track", 1.5 }, { "volume", -6.0 } },  "volume: track non intera" },
            { "daw.track.setVolume", { { "track", -1 }, { "volume", -6.0 } },   "volume: track negativa" },
            { "daw.track.setVolume", { { "track", 256 }, { "volume", -6.0 } },  "volume: track oltre 255" },
            { "daw.track.setVolume", { { "track", true }, { "volume", -6.0 } }, "volume: track booleana" },
            { "daw.track.setVolume", { { "track", 1 }, { "volume", nan } },     "volume: NaN" },
            { "daw.track.setVolume", { { "track", 1 }, { "volume", inf } },     "volume: infinito" },
            { "daw.track.setVolume", { { "track", 1 }, { "volume", 12.5 } },    "volume: oltre +12 dB" },
            { "daw.track.setVolume", { { "track", 1 }, { "volume", -97.0 } },   "volume: sotto -96 dB" },
            { "daw.track.setVolume", { { "track", 1 }, { "volume", "-6" } },    "volume: stringa" },
            { "daw.track.setPan",    { { "track", 1 }, { "pan", 1.5 } },        "pan: oltre 1" },
            { "daw.track.setPan",    { { "track", 1 }, { "pan", -1.01 } },      "pan: sotto -1" },
            { "daw.track.setPan",    { { "track", 1 }, { "pan", nan } },        "pan: NaN" },
            { "daw.track.setPan",    { { "track", 1 }, { "pan", false } },      "pan: booleano" },
            { "daw.track.setPan",    { { "pan", 0.0 } },                        "pan: track mancante" },
            { "daw.track.mute",      { { "track", 1 }, { "mute", 1 } },         "mute: numero invece di bool" },
            { "daw.track.mute",      { { "track", 1 }, { "mute", "true" } },    "mute: stringa" },
            { "daw.track.mute",      { { "track", 1 } },                        "mute: flag mancante" },
            { "daw.track.mute",      { { "track", -2 }, { "mute", true } },     "mute: track negativa" },
            { "daw.track.solo",      { { "track", 1 }, { "solo", 0 } },         "solo: numero invece di bool" },
            { "daw.track.solo",      { { "track", 1 } },                        "solo: flag mancante" },
            { "daw.track.solo",      { { "track", 300 }, { "solo", true } },    "solo: track oltre 255" },
            { "daw.track.setVolume", nlohmann::json::array ({ 1, -6.0 }),       "volume: argomenti array" },
            { "daw.track.setVolume", nlohmann::json(),                          "volume: argomenti null" },
        };

        for (const auto& c : cases)
        {
            beginTest (juce::String ("Rifiuto: ") + c.what);
            Rig rig; install (rig);
            auto o = runTool (rig, c.tool, c.args);
            expectNotSent (rig, o, "rejected", c.what);
        }
    }

    //==========================================================================
    // Marker
    //==========================================================================
    void testMarkerGoto()
    {
        beginTest ("gotoMarker: indice numerico in stringa arriva come intero");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.goto", { { "marker", "3" } });
            expectSentUnconfirmed (o, "goto \"3\"");
            expectSingleCall (rig, "gotoMarker", "goto \"3\"");
            if (! rig.daw->calls.empty())
                expectEquals (rig.daw->calls[0].track, 3);
        }

        beginTest ("gotoMarker: indice intero JSON");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.goto", { { "marker", 2 } });
            expectSentUnconfirmed (o, "goto 2");
            expectSingleCall (rig, "gotoMarker", "goto 2");
            if (! rig.daw->calls.empty())
                expectEquals (rig.daw->calls[0].track, 2);
        }

        beginTest ("gotoMarker: nome non risolvibile non salta al marker 0");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.goto", { { "marker", "Chorus" } });
            expectNotSent (rig, o, "unsupported", "goto \"Chorus\"");
        }

        struct Case { nlohmann::json args; const char* what; };
        const std::vector<Case> bad {
            { { { "marker", "-1" } },   "goto: indice negativo" },
            { { { "marker", -1 } },     "goto: intero negativo" },
            { { { "marker", "1.5" } },  "goto: decimale in stringa" },
            { { { "marker", 1.5 } },    "goto: decimale" },
            { { { "marker", "2abc" } }, "goto: cifre con coda" },
            { { { "marker", "" } },     "goto: stringa vuota" },
            { { { "marker", true } },   "goto: booleano" },
            { { { "marker", std::numeric_limits<double>::quiet_NaN() } }, "goto: NaN" },
            { nlohmann::json::object(), "goto: marker mancante" },
        };

        for (const auto& c : bad)
        {
            beginTest (juce::String ("Rifiuto: ") + c.what);
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.goto", c.args);
            expect (! o.threw, juce::String (c.what) + ": nessuna eccezione");
            expect (! o.result.success, juce::String (c.what) + ": success false");
            expect (rig.daw->calls.empty(), juce::String (c.what) + ": nessun invio");
            const auto st = field (o.out, "status");
            expect (st == "rejected" || st == "unsupported",
                    juce::String (c.what) + ": status rejected/unsupported, era '"
                        + juce::String (st) + "'");
        }
    }

    void testMarkerSet()
    {
        beginTest ("setMarker senza nome: inviato, non confermato");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.set", nlohmann::json::object());
            expectSentUnconfirmed (o, "setMarker");
            expectSingleCall (rig, "setMarker", "setMarker");
        }

        beginTest ("setMarker con nome: il nome non e' supportato, risposta onesta");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.set", { { "name", "Drop" } });
            expectNotSent (rig, o, "unsupported", "setMarker \"Drop\"");
            expect (o.result.output.indexOf ("\"Drop\" inserito") < 0,
                    "non deve dire che il marker con nome e' stato inserito");
        }

        beginTest ("setMarker con nome non stringa: rifiutato senza eccezione");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.marker.set", { { "name", 42 } });
            expectNotSent (rig, o, "rejected", "setMarker name=42");
        }
    }

    //==========================================================================
    // Trasporto: solo validazione e esito, non la semantica di record
    //==========================================================================
    void testTransportAndTempo()
    {
        beginTest ("setTempo valido arriva al gestore");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.transport.setTempo", { { "bpm", 120.0 } });
            expectSentUnconfirmed (o, "setTempo");
            expectSingleCall (rig, "setTempo", "setTempo");
            if (! rig.daw->calls.empty())
                expectWithinAbsoluteError (rig.daw->calls[0].value, 120.0f, 1.0e-4f);
        }

        struct Case { nlohmann::json args; const char* what; };
        const std::vector<Case> bad {
            { { { "bpm", "120" } }, "tempo: stringa" },
            { { { "bpm", std::numeric_limits<double>::quiet_NaN() } }, "tempo: NaN" },
            { { { "bpm", 10.0 } },  "tempo: sotto 20" },
            { { { "bpm", 1000.0 } }, "tempo: oltre 999" },
            { nlohmann::json::object(), "tempo: bpm mancante" },
        };
        for (const auto& c : bad)
        {
            beginTest (juce::String ("Rifiuto: ") + c.what);
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.transport.setTempo", c.args);
            expectNotSent (rig, o, "rejected", c.what);
        }

        beginTest ("record con arm non booleano: rifiutato senza eccezione");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.transport.record", { { "arm", "yes" } });
            expectNotSent (rig, o, "rejected", "record arm=\"yes\"");
        }
    }

    //==========================================================================
    // Esito: mai "eseguito", anche se la DAW ha parlato di recente
    //==========================================================================
    void testUnconfirmedOutcome()
    {
        beginTest ("play: inviato ma non confermato");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.transport.play", nlohmann::json::object());
            expectSentUnconfirmed (o, "play");
            expectSingleCall (rig, "play", "play");
        }

        beginTest ("Traffico OSC recente non e' prova dell'azione");
        {
            Rig rig; install (rig);
            rig.bridge.lastOscFromDawMs.store (juce::Time::currentTimeMillis());
            expect (rig.bridge.dawHaRisposto(), "precondizione: DAW con traffico recente");

            auto o = runTool (rig, "daw.track.mute", { { "track", 0 }, { "mute", true } });
            expectSentUnconfirmed (o, "mute con DAW attiva");
            expect (o.result.output.indexOf ("silenziata") < 0
                      || o.result.output.containsIgnoreCase ("non confermat"),
                    "non deve affermare che la traccia e' silenziata");
        }
    }

    //==========================================================================
    // Finto Live: applica i set, risponde ai get come AbletonOSC
    //==========================================================================
    struct FakeLive
    {
        enum class Modo { Applica, Ignora, Muto };

        Modo modo = Modo::Applica;
        std::map<std::string, double> stato;                 // chiave "get[#traccia]"
        std::vector<std::pair<std::string, std::vector<float>>> inviati;   // set ricevuti

        static std::string chiaveGet (const juce::String& setAddr, const std::vector<float>& v)
        {
            auto get = setAddr.replace ("/set/", "/get/");
            const bool traccia = get.startsWith ("/live/track/") && v.size() >= 2;
            return OscBridge::chiaveRisposta (get, traccia ? (int) v[0] : -1);
        }
    };

    void collega (Rig& rig, FakeLive& live)
    {
        auto handler = std::make_unique<AbletonDawHandler>();
        auto* h = handler.get();
        rig.bridge.dawHandler = std::move (handler);
        rig.daw = nullptr;

        h->setSendMultiCallback ([&live] (const juce::String& a, const std::vector<float>& v, const std::vector<bool>&)
        {
            live.inviati.emplace_back (a.toStdString(), v);
            if (live.modo != FakeLive::Modo::Applica) return;
            live.stato[FakeLive::chiaveGet (a, v)] = v.back();
        });
        h->setSendCallback ([&live] (const juce::String& a, float)
        {
            if (live.modo != FakeLive::Modo::Applica) return;
            if (a == "/live/song/start_playing") live.stato["/live/song/get/is_playing"] = 1.0;
            if (a == "/live/song/stop_playing")  live.stato["/live/song/get/is_playing"] = 0.0;
        });

        rig.bridge.inviaDomandaDaw = [&rig, &live] (const juce::String& a, const std::vector<float>& v, const std::vector<bool>&)
        {
            if (live.modo == FakeLive::Modo::Muto) return;
            const bool traccia = a.startsWith ("/live/track/") && ! v.empty();
            const auto chiave = OscBridge::chiaveRisposta (a, traccia ? (int) v[0] : -1);
            auto it = live.stato.find (chiave);
            const double valore = it != live.stato.end() ? it->second : 0.85;
            if (traccia) rig.bridge.registraRispostaDaw (a, { v[0], (float) valore });
            else         rig.bridge.registraRispostaDaw (a, { (float) valore });
        };
    }

    void testLiveReadback()
    {
        beginTest ("Live: volume riletto uguale -> confirmed");
        {
            Rig rig; FakeLive live; collega (rig, live);
            auto o = runTool (rig, "daw.track.setVolume", { { "track", 2 }, { "volume", 0.0 } });
            expectEquals (juce::String (field (o.out, "status")), juce::String ("confirmed"));
            expect (o.result.success, "success vero solo con conferma");
            expect (o.out["confirmed"].get<bool>());
            expectWithinAbsoluteError (o.out["dawState"]["read"].get<double>(), 0.85, 1.0e-4);
        }

        beginTest ("Live: valore diverso riletto -> mismatch, non successo");
        {
            Rig rig; FakeLive live; live.modo = FakeLive::Modo::Ignora; collega (rig, live);
            auto o = runTool (rig, "daw.track.setVolume", { { "track", 2 }, { "volume", -6.0 } });
            expectEquals (juce::String (field (o.out, "status")), juce::String ("mismatch"));
            expect (! o.result.success, "mismatch non e' successo");
            expect (! o.out["confirmed"].get<bool>());
            expect (o.out["accepted"].get<bool>());
        }

        beginTest ("Live: nessuna risposta -> submitted, stato sconosciuto");
        {
            Rig rig; FakeLive live; live.modo = FakeLive::Modo::Muto; collega (rig, live);
            auto o = runTool (rig, "daw.track.mute", { { "track", 1 }, { "mute", true } });
            expectEquals (juce::String (field (o.out, "status")), juce::String ("submitted"));
            expect (! o.result.success);
            expectEquals (juce::String (field (o.out, "dawState")), juce::String ("unknown"));
        }

        beginTest ("Live: pan negativo arriva a Live come negativo e si conferma");
        {
            Rig rig; FakeLive live; collega (rig, live);
            auto o = runTool (rig, "daw.track.setPan", { { "track", 0 }, { "pan", -0.5 } });
            expectEquals (juce::String (field (o.out, "status")), juce::String ("confirmed"));
            expect (o.result.success);
            expect (! live.inviati.empty());
            if (! live.inviati.empty())
            {
                expectEquals (juce::String (live.inviati.back().first), juce::String ("/live/track/set/panning"));
                expectWithinAbsoluteError (live.inviati.back().second.back(), -0.5f, 1.0e-6f);
            }
        }

        beginTest ("Live: tempo e trasporto confermati dalla rilettura");
        {
            Rig rig; FakeLive live; collega (rig, live);
            auto t = runTool (rig, "daw.transport.setTempo", { { "bpm", 128.0 } });
            expectEquals (juce::String (field (t.out, "status")), juce::String ("confirmed"));
            auto p = runTool (rig, "daw.transport.play", nlohmann::json::object());
            expectEquals (juce::String (field (p.out, "status")), juce::String ("confirmed"));
            auto s = runTool (rig, "daw.transport.stop", nlohmann::json::object());
            expectEquals (juce::String (field (s.out, "status")), juce::String ("confirmed"));
            expect (s.result.success);
        }

        beginTest ("Risposte registrate: chiave per traccia, seq crescente, solo /get/");
        {
            OscBridge bridge (9000, 8080);
            bridge.registraRispostaDaw ("/live/track/get/volume", { 3.0f, 0.5f });
            bridge.registraRispostaDaw ("/live/track/get/volume", { 4.0f, 0.7f });
            bridge.registraRispostaDaw ("/live/song/get/tempo", { 120.0f });
            bridge.registraRispostaDaw ("/live/track/set/volume", { 3.0f, 0.9f });   // non e' una risposta
            const auto a = bridge.ultimaRispostaDaw (OscBridge::chiaveRisposta ("/live/track/get/volume", 3));
            const auto b = bridge.ultimaRispostaDaw (OscBridge::chiaveRisposta ("/live/track/get/volume", 4));
            const auto c = bridge.ultimaRispostaDaw ("/live/song/get/tempo");
            expectWithinAbsoluteError (a.value, 0.5, 1.0e-6);
            expectWithinAbsoluteError (b.value, 0.7, 1.0e-6);
            expectWithinAbsoluteError (c.value, 120.0, 1.0e-6);
            expect (a.seq < b.seq && b.seq < c.seq, "numeri progressivi");
            expectEquals ((int) bridge.ultimaRispostaDaw (OscBridge::chiaveRisposta ("/live/track/set/volume", 3)).seq, 0);
        }
    }

    void testLiveConversions()
    {
        beginTest ("Volume Live: ancoraggi 0 -> 0, 0 dB -> 0.85, +6 dB -> 1.0");
        {
            expectWithinAbsoluteError (AbletonDawHandler::liveVolumeFromGain (0.0f), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (AbletonDawHandler::liveVolumeFromGain (1.0f), 0.85f, 1.0e-5f);
            expectWithinAbsoluteError (AbletonDawHandler::liveVolumeFromGain (juce::Decibels::decibelsToGain (6.0f)), 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (AbletonDawHandler::liveVolumeFromGain (juce::Decibels::decibelsToGain (12.0f)), 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (AbletonDawHandler::liveVolumeFromGain (-1.0f), 0.0f, 1.0e-6f);
            const float meno6 = AbletonDawHandler::liveVolumeFromGain (juce::Decibels::decibelsToGain (-6.0f));
            expect (meno6 > 0.0f && meno6 < 0.85f, "-6 dB fra 0 e 0.85");
        }

        beginTest ("Pan Live: -1..1 conservato, fuori scala limitato");
        {
            expectWithinAbsoluteError (AbletonDawHandler::livePanFromPan (-0.5f), -0.5f, 1.0e-6f);
            expectWithinAbsoluteError (AbletonDawHandler::livePanFromPan (-3.0f), -1.0f, 1.0e-6f);
            expectWithinAbsoluteError (AbletonDawHandler::livePanFromPan (2.0f), 1.0f, 1.0e-6f);
        }
    }

    //==========================================================================
    // Non supportati e sconosciuti
    //==========================================================================
    void testUnsupportedAndUnknown()
    {
        beginTest ("Plugin esterni: unsupported, nessun setFxParam");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.plugin.setParam",
                              { { "track", 0 }, { "plugin", "Pro-Q 3" }, { "param", "Gain" }, { "value", 0.5 } });
            expectNotSent (rig, o, "unsupported", "plugin.setParam");
        }

        beginTest ("Strumento daw sconosciuto: non gestito, nessun invio");
        {
            Rig rig; install (rig);
            auto o = runTool (rig, "daw.track.explode", { { "track", 0 } });
            expect (! o.threw);
            expect (! o.handled, "nome sconosciuto non deve essere gestito come comando DAW");
            expect (rig.daw->calls.empty());
        }
    }

    //==========================================================================
    // Comandi UI esistenti: stessi campi di prima, ma senza eccezioni
    //==========================================================================
    void testUiDispatcherCompatibility()
    {
        beginTest ("UI: setVolume/setPan/mute/solo con i campi storici");
        {
            Rig rig; install (rig);
            expect (dispatchUi (rig, { { "command", "setVolume" }, { "trackId", 2 }, { "valueDb", 0.0 } }));
            expect (dispatchUi (rig, { { "command", "setPan" },    { "trackId", 2 }, { "value", 0.25 } }));
            expect (dispatchUi (rig, { { "command", "muteTrack" }, { "trackId", 2 }, { "muted", true } }));
            expect (dispatchUi (rig, { { "command", "soloTrack" }, { "trackId", 2 }, { "soloed", false } }));
            expect (dispatchUi (rig, { { "command", "gotoMarker" }, { "index", 1 } }));
            expectEquals ((int) rig.daw->calls.size(), 5);
            if (rig.daw->calls.size() == 5)
            {
                expectWithinAbsoluteError (rig.daw->calls[0].value, 1.0f, 1.0e-6f);
                expectWithinAbsoluteError (rig.daw->calls[1].value, 0.25f, 1.0e-6f);
                expect (rig.daw->calls[2].flag);
                expect (! rig.daw->calls[3].flag);
                expectEquals (juce::String (rig.daw->calls[4].fn), juce::String ("gotoMarker"));
                expectEquals (rig.daw->calls[4].track, 1);
            }
        }

        beginTest ("UI: payload malformati non lanciano e non inviano");
        {
            Rig rig; install (rig);
            expect (dispatchUi (rig, { { "command", 5 } }), "command non stringa");
            expect (dispatchUi (rig, { { "command", "setVolume" }, { "trackId", "x" }, { "valueDb", 0.0 } }), "trackId stringa");
            expect (dispatchUi (rig, { { "command", "muteTrack" }, { "trackId", 1 }, { "muted", "si" } }), "muted stringa");
            expect (dispatchUi (rig, { { "command", "gotoMarker" }, { "index", "Chorus" } }), "index stringa");
            expect (dispatchUi (rig, { { "command", "gotoMarker" } }), "index mancante");
            expect (dispatchUi (rig, nlohmann::json::array()), "payload array");
            expect (dispatchUi (rig, { { "command", "setVolume" }, { "trackId", 1 }, { "valueDb", 1.0e300 } }), "valueDb +1e300");
            expect (dispatchUi (rig, { { "command", "setVolume" }, { "trackId", 1 }, { "valueDb", -1.0e300 } }), "valueDb -1e300");
            expect (dispatchUi (rig, { { "command", "setVolume" }, { "trackId", 1 }, { "valueDb", 12.5 } }), "valueDb oltre +12");
            expect (dispatchUi (rig, { { "command", "setVolume" }, { "trackId", 1 }, { "valueDb", -97.0 } }), "valueDb sotto -96");
            expect (rig.daw->calls.empty(), "nessun invio, nessun salto al marker 0");
        }
    }
};

static OscBridgeTest oscBridgeTest;
