#include "ReaperDawHandler.h"

// ── Adattatore REAPER ──────────────────────────────────────────────
//
//  Contratto (uguale per tutte le DAW): traccia 0-based, volume in dB.
//  Qui si traduce nel dialetto di REAPER, preso dal file
//  ~/.config/REAPER/OSC/Default.ReaperOSC installato:
//
//    TEMPO     f/tempo/raw        (n/tempo è normalizzato: non e' in BPM)
//    TRACK_VOLUME f/track/@/volume/db   (n/track/@/volume è normalizzato)
//    TRACK_PAN n/track/@/pan
//    TRACK_MUTE/SOLO b/track/@/mute|solo   (0/1, non toggle)
//    FX_PARAM_VALUE n/track/@/fx/@/fxparam/@/value
//    FX_BYPASS b/track/@/fx/@/bypass
//
//  Gli indici di track e di fx in REAPER partono da 1: il +1 si fa qui.
//
//  Marker: in questa configurazione esistono solo in lettura
//  (s/marker/@/name, f/marker/@/time). Crearli o saltarci non e'
//  possibile via OSC: il tool corrispondente non viene offerto (vedi
//  DawCapabilities) e i metodi restano vuoti per non mandare comandi
//  che nessuno ascolta.

static constexpr int reaperIndex (int zeroBased) { return zeroBased + 1; }

void ReaperDawHandler::play()
{
    if (sendCallback) sendCallback("/play", 1.0f);
}

void ReaperDawHandler::stop()
{
    if (sendCallback) sendCallback("/stop", 1.0f);
}

void ReaperDawHandler::record()
{
    // t/record è un trigger: commuta. Non esiste un "record on/off".
    // Lo stato va verificato dal feedback, non da questo comando.
    if (sendCallback) sendCallback("/record", 1.0f);
}

void ReaperDawHandler::pause()
{
    if (sendCallback) sendCallback("/pause", 1.0f);
}

void ReaperDawHandler::setTempo(float bpm)
{
    // /tempo è un valore normalizzato (rotary); i BPM vanno su /tempo/raw.
    if (sendCallback) sendCallback("/tempo/raw", bpm);
}

void ReaperDawHandler::setVolumeDb(int trackId, float db)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(trackId)) + "/volume/db", db);
}

void ReaperDawHandler::setPan(int trackId, float value)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(trackId)) + "/pan", value);
}

void ReaperDawHandler::muteTrack(int trackId, bool muted)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(trackId)) + "/mute", muted ? 1.0f : 0.0f);
}

void ReaperDawHandler::soloTrack(int trackId, bool soloed)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(trackId)) + "/solo", soloed ? 1.0f : 0.0f);
}

// ── Marker: non disponibili via OSC in questa configurazione ────────
void ReaperDawHandler::gotoMarker(int)  {}
void ReaperDawHandler::setMarker()      {}
void ReaperDawHandler::prevMarker()     {}
void ReaperDawHandler::nextMarker()     {}

void ReaperDawHandler::selectTrack(int index)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(index)) + "/select", 1.0f);
}

void ReaperDawHandler::setFxParam(int trackId, int fxId, int paramId, float value)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(trackId))
                     + "/fx/" + juce::String(reaperIndex(fxId))
                     + "/fxparam/" + juce::String(reaperIndex(paramId))
                     + "/value", value);
}

void ReaperDawHandler::setFxBypass(int trackId, int fxId, bool bypass)
{
    if (sendCallback)
        sendCallback("/track/" + juce::String(reaperIndex(trackId))
                     + "/fx/" + juce::String(reaperIndex(fxId))
                     + "/bypass", bypass ? 1.0f : 0.0f);
}
