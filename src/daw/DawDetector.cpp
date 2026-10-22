#include "DawDetector.h"
#include <juce_audio_processors/juce_audio_processors.h>

DawType DawDetector::detectFromOscAddress(const juce::String& address)
{
    if (address.startsWith("/live/"))
        return DawType::Ableton;

    if (address.startsWith("/track/") || address == "/play" || address == "/stop"
        || address == "/record" || address == "/tempo" || address == "/position"
        || address.startsWith("/marker/"))
        return DawType::Reaper;

    if (address.startsWith("/cue/") || address.startsWith("/loop/")
        || address.startsWith("/device/") || address.startsWith("/select/"))
        return DawType::Ableton;

    return DawType::Unknown;
}

// Riconoscimento dall'host: prima era uno stub che rispondeva sempre
// "Unknown", e la DAW restava sconosciuta finche' non arrivava un messaggio
// OSC. Conseguenza: in Ableton i tool esclusivi di Live non venivano offerti
// (perche' la DAW risultava sconosciuta) e l'agente rispondeva "non posso".
// JUCE sa chi sta ospitando il plugin: si usa quello.
DawType DawDetector::detectFromHost(const juce::String& host, int port)
{
    juce::ignoreUnused(host, port);

    const juce::PluginHostType h;

    if (h.isReaper())        return DawType::Reaper;
    if (h.isAbletonLive())   return DawType::Ableton;
    if (h.isCubase())        return DawType::Cubase;
    if (h.isLogic())         return DawType::Logic;
    if (h.isStudioOne())     return DawType::StudioOne;
    if (h.isBitwigStudio())   return DawType::Bitwig;
    if (h.isProTools())      return DawType::ProTools;
    if (h.isFruityLoops())    return DawType::FLStudio;

    return DawType::Generic;
}

juce::String DawDetector::getName(DawType type)
{
    switch (type)
    {
        case DawType::Reaper:    return "REAPER";
        case DawType::Ableton:   return "Ableton Live";
        case DawType::Logic:     return "Logic Pro";
        case DawType::Cubase:    return "Cubase";
        case DawType::FLStudio:  return "FL Studio";
        case DawType::Bitwig:    return "Bitwig Studio";
        case DawType::StudioOne: return "Studio One";
        case DawType::ProTools:  return "Pro Tools";
        default:                 return "Unknown DAW";
    }
}

juce::String DawDetector::getOscHelp(DawType type)
{
    switch (type)
    {
        case DawType::Reaper:
            return "Enable OSC in REAPER: Preferences > Control Surfaces > Add > OSC (Open Sound Control). "
                   "Set Local listen port to 8000 (WhyCremisi sends here), device IP 127.0.0.1 and device port 9000 (WhyCremisi listens here). The installer does this for you.";
        case DawType::Ableton:
            return "Install AbletonOSC by ideoforms: https://github.com/ideoforms/AbletonOSC. "
                   "It runs as a Max for Live device and handles /live/... OSC endpoints.";
        default:
            return "Consult your DAW's documentation for OSC configuration.";
    }
}
