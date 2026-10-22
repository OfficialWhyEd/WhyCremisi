#include "DawCapabilities.h"

namespace DawCaps
{
    // ── Gruppo A: non toccano la DAW ────────────────────────────────
    //  Stessi su ogni host. Se un giorno uno di questi avesse bisogno della
    //  DAW, va spostato: qui dentro non deve entrare niente di specifico.
    static const juce::StringArray& universali()
    {
        static const juce::StringArray a {
            "dsp.compressor.set", "dsp.limiter.set", "dsp.bypass", "dsp.getState",
            "mix.analyze", "ui.showPanels",
            "samples.scan", "samples.search",
            "memory.remember", "memory.search", "memory.forget",
            "skill.search", "skill.load", "mcp.list", "mcp.call"
        };
        return a;
    }

    // ── Gruppo B: stessa intenzione su ogni DAW, tradotta dall'adattatore ──
    //  UNITA' E INDICI DEL CONTRATTO: traccia 0-based, volume in dB.
    //  La conversione (Reaper +1 e posizione del fader, Live curva del fader) sta
    //  nell'adattatore, non qui e non nel tool.
    static const juce::StringArray& comuni()
    {
        static const juce::StringArray a {
            "daw.transport.play", "daw.transport.stop", "daw.transport.record",
            "daw.transport.setTempo",
            "daw.track.create", "daw.track.setVolume", "daw.track.setPan",
            "daw.track.mute", "daw.track.solo"
        };
        return a;
    }

    // ── Gruppo C: esistono solo in Live ────────────────────────────
    static const juce::StringArray& soloAbleton()
    {
        static const juce::StringArray a {
            "ableton.setup", "music.compose", "beat.create",
            // Fanno parte del gruppo Live perche' leggono da AbletonOSC.
            // Su REAPER sarebbe possibile (il feedback volume/pan/mute arriva),
            // ma non e' implementato: meglio non prometterlo.
            "mix.levels", "mix.snapshot.save", "mix.snapshot.recall",
            "device.list", "device.params", "device.setParam", "device.dictionary"
        };
        return a;
    }

    // ── Sospesi: il tool esiste nel registro ma l'executor lo rifiuta
    //    sempre ("Questo comando non e' piu' usato"). Non vanno offerti:
    //    un tool che risponde sempre "no" e' solo un turno sprecato. ──
    static const juce::StringArray& sospesi()
    {
        static const juce::StringArray a { "daw.plugin.setParam", "daw.plugin.bypass" };
        return a;
    }

    // ── Marker: Live li chiama cue point e li ha; REAPER li espone solo
    //    in lettura via OSC, quindi non si possono creare ne' saltare ──
    static const juce::StringArray& markerSoloAbleton()
    {
        static const juce::StringArray a { "daw.marker.goto", "daw.marker.set" };
        return a;
    }

    juce::String name (DawType daw) { return DawDetector::getName (daw); }

    bool supports (const juce::String& toolName, DawType daw)
    {
        if (sospesi().contains (toolName))     return false;
        if (universali().contains (toolName)) return true;
        if (comuni().contains (toolName))     return true;
        if (soloAbleton().contains (toolName)) return daw == DawType::Ableton;
        if (markerSoloAbleton().contains (toolName)) return daw == DawType::Ableton;
        return false;
    }

    juce::String reasonFor (const juce::String& toolName, DawType daw)
    {
        if (supports (toolName, daw)) return {};

        const auto nomeDaw = name (daw);

        if (sospesi().contains (toolName))
            return "\"" + toolName + "\" e' sospeso (in manutenzione): non usarlo, "
                   "per i plugin usa device.* su Live oppure dsp.* per la catena interna.";

        if (soloAbleton().contains (toolName))
            return "\"" + toolName + "\" esiste solo in Ableton Live: su " + nomeDaw
                 + " non e' disponibile. Usa i tool daw.* e dsp.* che ci sono.";

        if (markerSoloAbleton().contains (toolName))
            return "\"" + toolName + "\" non e' disponibile su " + nomeDaw
                 + ": la sua OSC espone i marker solo in lettura.";

        return "\"" + toolName + "\" non e' disponibile su " + nomeDaw + ".";
    }

    juce::StringArray toolsFor (DawType daw)
    {
        juce::StringArray out;
        out.addArray (universali());
        out.addArray (comuni());
        if (daw == DawType::Ableton)
        {
            out.addArray (soloAbleton());
            out.addArray (markerSoloAbleton());
        }
        return out;
    }

    juce::StringArray allTools()
    {
        juce::StringArray out;
        out.addArray (universali());
        out.addArray (comuni());
        out.addArray (soloAbleton());
        out.addArray (markerSoloAbleton());
        out.addArray (sospesi());
        return out;
    }

    juce::StringArray unavailableFor (DawType daw)
    {
        juce::StringArray out;
        for (const auto& t : allTools())
            if (! supports (t, daw)) out.addIfNotAlreadyThere (t);
        return out;
    }

    juce::String promptBlock (DawType daw)
    {
        return promptBlock (daw, toolsFor (daw));
    }

    // Il blocco che il modello legge. Deve dire TRE cose, e tutte e tre
    // servono: cosa puo' chiamare ORA, cosa esiste altrove (altrimenti
    // risponde "non posso" anche a una domanda su un'altra DAW), cosa e'
    // sospeso. La lista dei tool universali non si ripete: e' gia' nello
    // schema che il provider gli manda.
    juce::String promptBlock (DawType daw, const juce::StringArray& chiamabili)
    {
        juce::String testo;

        if (daw == DawType::Unknown || daw == DawType::Generic)
            testo = "DAW collegata: non ancora identificata. Puoi usare i tool daw.* generici; "
                    "se un comando non produce effetto, dillo invece di ripeterlo.";
        else
            testo = "DAW aperta: " + name (daw) + ". Traccia 0-based, volumi in dB; "
                    "gli indirizzi sono gia' tradotti dall'adattatore.";

        if (! chiamabili.isEmpty())
            testo << " Strumenti che puoi chiamare ORA: " << chiamabili.joinIntoString (", ") << ".";

        // Cosa esiste ma non qui: raggruppato per non fare un muro di nomi.
        juce::StringArray soloLive, sospesiQui;
        for (const auto& t : unavailableFor (daw))
        {
            if (sospesi().contains (t))            sospesiQui.add (t);
            else                                   soloLive.add (t);
        }

        if (! soloLive.isEmpty())
            testo << " Esistono in WhyCremisi ma NON su questa DAW (altrove si': per esempio su "
                  << "Ableton Live): " << soloLive.joinIntoString (", ")
                  << ". Qui non puoi chiamarli ne' dire di averli usati; se ti chiedono se "
                     "sapresti farlo altrove, rispondi secondo questo elenco.";

        if (! sospesiQui.isEmpty())
            testo << " Sospesi (in manutenzione, non usarli): "
                  << sospesiQui.joinIntoString (", ") << ".";

        testo << " Non inventare altri strumenti: se non e' in questo elenco, non esiste.";
        return testo;
    }
}
