#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// Messaggi del protocollo Mackie Control (MCU), solo codifica e lettura. Nessuna porta MIDI qui:
// chi usa questa classe decide su quale porta mandarli. Il protocollo e' quello di Mackie/Logic,
// descritto in pubblico; qui sono i valori che le DAW capiscono.
namespace McuProtocol
{
    constexpr int stripCount = 8;     // otto canali per banco, il nono (indice 8) e' il master

    // Note dei pulsanti per canale (0..7).
    inline int noteRecArm (int strip) { return 0x00 + strip; }
    inline int noteSolo   (int strip) { return 0x08 + strip; }
    inline int noteMute   (int strip) { return 0x10 + strip; }
    inline int noteSelect (int strip) { return 0x18 + strip; }

    // Note del trasporto e dei banchi.
    constexpr int noteRewind = 0x5B, noteForward = 0x5C, noteStop = 0x5D, notePlay = 0x5E,
                  noteRecord = 0x5F, noteCycle = 0x56;
    constexpr int noteBankLeft = 0x2E, noteBankRight = 0x2F, noteChannelLeft = 0x30, noteChannelRight = 0x31;

    // Fader: pitch bend sul canale MIDI del canale della console, 14 bit. value01 da 0 a 1.
    juce::MidiMessage fader (int strip, float value01);

    // Pulsante premuto e rilasciato (nota con velocita' 127 e poi 0).
    juce::MidiMessage buttonDown (int note);
    juce::MidiMessage buttonUp (int note);

    // Manopola: giri relativi, positivi in senso orario. Da 1 a 63 scatti per messaggio.
    juce::MidiMessage vpotTurn (int strip, int ticks);

    // Lettura di cio' che la DAW rimanda.
    bool parseFader (const juce::MidiMessage& m, int& strip, float& value01);
    bool parseLed (const juce::MidiMessage& m, int& note, bool& on);   // la DAW accende o spegne un pulsante
    bool parseLcd (const juce::MidiMessage& m, int& offset, juce::String& text);   // testo del display

    // Domanda di presenza dell'unita' (la DAW risponde se ci vede come console).
    juce::MidiMessage deviceQuery();
}
