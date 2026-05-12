#include "DawDetector.h"

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

DawType DawDetector::detectFromHost(const juce::String& host, int port)
{
    juce::ignoreUnused(host, port);
    return DawType::Unknown;
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
                   "Set Local Listen Port to 9000 and Pattern Config to 'ReaperOSC'.";
        case DawType::Ableton:
            return "Install AbletonOSC by ideoforms: https://github.com/ideoforms/AbletonOSC. "
                   "It runs as a Max for Live device and handles /live/... OSC endpoints.";
        default:
            return "Consult your DAW's documentation for OSC configuration.";
    }
}
