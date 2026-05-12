#pragma once

#include <juce_core/juce_core.h>

enum class DawType
{
    Unknown,
    Reaper,
    Ableton,
    Logic,
    Cubase,
    FLStudio,
    Bitwig,
    StudioOne,
    ProTools,
    Generic
};

class DawDetector
{
public:
    /** Detect DAW type from an incoming OSC address pattern */
    static DawType detectFromOscAddress(const juce::String& address);

    /** Detect DAW type from a host/port pattern (if known) */
    static DawType detectFromHost(const juce::String& host, int port);

    /** Get human-readable name for a DAW type */
    static juce::String getName(DawType type);

    /** Get a suggestion for the DAW's OSC configuration */
    static juce::String getOscHelp(DawType type);
};
