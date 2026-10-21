#include "AbletonDawHandler.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════
//  Indirizzi OSC di Ableton Live
// ═══════════════════════════════════════════════════════════════════
//
//  Il riferimento e' AbletonOSC (github.com/ideoforms/AbletonOSC), il
//  Remote Script piu' usato oggi. La differenza che conta rispetto al
//  vecchio LiveOSC, che questo file usava prima, e' come si indica la
//  traccia:
//
//      LiveOSC      /live/track/2/set/volume        [valore]
//      AbletonOSC   /live/track/set/volume          [2, valore]
//
//  Con gli indirizzi vecchi Ableton non risponde: nessun errore, i
//  comandi semplicemente non arrivano a destinazione.
//
//  L'indice della traccia va mandato come intero, non come float.
//
//  Perche' funzioni serve AbletonOSC installato fra i Remote Scripts di
//  Live e attivato in Preferenze - Link/Tempo/MIDI come Control Surface.
//  Live ascolta sulla porta 11000 e risponde sulla 11001.

// Volume in Live: valore normalizzato 0..1 (0.85 = 0 dB, 1.0 = +6 dB).
// Pan in Live: -1 tutto a sinistra, 0 centro, +1 tutto a destra.
// Valori fuori scala vengono rifiutati silenziosamente, quindi si limitano qui.

float AbletonDawHandler::liveVolumeFromGain (float gain)
{
    if (! (gain > 0.0f))
        return 0.0f;

    const float db = juce::Decibels::gainToDecibels (gain);
    if (db >= 0.0f)
        return 0.85f + 0.15f * juce::jmin (db, 6.0f) / 6.0f;

    // Sotto 0 dB: v = 0.85 * 10^(db/40), cioe' 0.85 * radice del guadagno.
    return 0.85f * std::pow (10.0f, db / 40.0f);
}

float AbletonDawHandler::livePanFromPan (float pan)
{
    return juce::jlimit (-1.0f, 1.0f, pan);
}

void AbletonDawHandler::createMidiClip (int trackId, int slotIndex, float lengthBeats)
{
    sendMulti ("/live/clip_slot/create_clip",
               { (float) trackId, (float) slotIndex, juce::jmax (0.25f, lengthBeats) },
               { true, true, false });
}

void AbletonDawHandler::addMidiNote (int trackId, int slotIndex, int pitch, float startBeat,
                                     float lengthBeats, int velocity)
{
    sendMulti ("/live/clip/add/notes",
               { (float) trackId, (float) slotIndex, (float) juce::jlimit (0, 127, pitch),
                 juce::jmax (0.0f, startBeat), juce::jmax (0.03125f, lengthBeats),
                 (float) juce::jlimit (1, 127, velocity), 0.0f },
               { true, true, true, false, false, true, true });
}

void AbletonDawHandler::sendMulti (const juce::String& address,
                                   const std::vector<float>& values,
                                   const std::vector<bool>& intMask)
{
    if (sendMultiCallback)
        sendMultiCallback (address, values, intMask);
    else if (sendCallback && ! values.empty())
        // Ripiego: senza il canale multi-argomento si manda almeno il
        // primo valore, cosi' i comandi di trasporto continuano a funzionare.
        sendCallback (address, values[0]);
}

// ── Trasporto ──────────────────────────────────────────────────────

void AbletonDawHandler::play()
{
    if (sendCallback) sendCallback("/live/song/start_playing", 1.0f);
}

void AbletonDawHandler::stop()
{
    if (sendCallback) sendCallback("/live/song/stop_playing", 1.0f);
}

void AbletonDawHandler::record()
{
    // In Live il record del sessione e' una proprieta' da impostare,
    // non un comando: /live/song/set/record_mode con 1 o 0.
    sendMulti ("/live/song/set/record_mode", { 1.0f }, { true });
}

void AbletonDawHandler::pause()
{
    // Live non ha una pausa vera: continue_playing riprende da dove si
    // era fermato, che e' il comportamento piu' vicino.
    if (sendCallback) sendCallback("/live/song/continue_playing", 1.0f);
}

void AbletonDawHandler::setTempo(float bpm)
{
    sendMulti ("/live/song/set/tempo", { bpm }, { false });
}

// ── Mixer ──────────────────────────────────────────────────────────

void AbletonDawHandler::setVolume(int trackId, float value)
{
    // `value` e' il guadagno lineare deciso dal dispatcher.
    sendMulti ("/live/track/set/volume", { (float) trackId, liveVolumeFromGain (value) }, { true, false });
}

void AbletonDawHandler::setPan(int trackId, float value)
{
    sendMulti ("/live/track/set/panning", { (float) trackId, livePanFromPan (value) }, { true, false });
}

void AbletonDawHandler::muteTrack(int trackId, bool muted)
{
    sendMulti ("/live/track/set/mute", { (float) trackId, muted ? 1.0f : 0.0f }, { true, true });
}

void AbletonDawHandler::soloTrack(int trackId, bool soloed)
{
    sendMulti ("/live/track/set/solo", { (float) trackId, soloed ? 1.0f : 0.0f }, { true, true });
}

// ── Locator ────────────────────────────────────────────────────────
//
//  Live chiama "cue point" quelli che altrove sono i marker.

void AbletonDawHandler::gotoMarker(int index)
{
    sendMulti ("/live/song/cue_point/jump", { (float) index }, { true });
}

void AbletonDawHandler::setMarker()
{
    if (sendCallback) sendCallback("/live/song/set_or_delete_cue", 1.0f);
}

void AbletonDawHandler::prevMarker()
{
    if (sendCallback) sendCallback("/live/song/jump_to_prev_cue", 1.0f);
}

void AbletonDawHandler::nextMarker()
{
    if (sendCallback) sendCallback("/live/song/jump_to_next_cue", 1.0f);
}

// ── Tracce e device ────────────────────────────────────────────────

void AbletonDawHandler::selectTrack(int index)
{
    sendMulti ("/live/view/set/selected_track", { (float) index }, { true });
}

void AbletonDawHandler::setFxParam(int trackId, int fxId, int paramId, float value)
{
    // In Live gli effetti sono "device" e i loro controlli "parameter":
    // l'indirizzo prende traccia, device e parametro come argomenti.
    sendMulti ("/live/device/set/parameter/value",
               { (float) trackId, (float) fxId, (float) paramId, value },
               { true, true, true, false });
}
