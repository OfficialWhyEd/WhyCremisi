#include "McuProtocol.h"

namespace McuProtocol
{
    juce::MidiMessage fader (int strip, float value01)
    {
        const int v = juce::jlimit (0, 16383, juce::roundToInt (juce::jlimit (0.0f, 1.0f, value01) * 16383.0f));
        return juce::MidiMessage::pitchWheel (juce::jlimit (0, 8, strip) + 1, v);
    }

    juce::MidiMessage buttonDown (int note) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 127); }
    juce::MidiMessage buttonUp (int note)   { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 0); }

    juce::MidiMessage vpotTurn (int strip, int ticks)
    {
        const int n = juce::jlimit (1, 63, std::abs (ticks));
        const int value = n | (ticks < 0 ? 0x40 : 0);
        return juce::MidiMessage::controllerEvent (1, 0x10 + juce::jlimit (0, 7, strip), value);
    }

    bool parseFader (const juce::MidiMessage& m, int& strip, float& value01)
    {
        if (! m.isPitchWheel()) return false;
        strip = m.getChannel() - 1;
        value01 = (float) m.getPitchWheelValue() / 16383.0f;
        return strip >= 0 && strip <= 8;
    }

    bool parseLed (const juce::MidiMessage& m, int& note, bool& on)
    {
        if (! m.isNoteOn (true) && ! m.isNoteOff()) return false;
        note = m.getNoteNumber();
        on = m.isNoteOn() && m.getVelocity() > 0;
        return true;
    }

    bool parseLcd (const juce::MidiMessage& m, int& offset, juce::String& text)
    {
        if (! m.isSysEx()) return false;
        const auto* d = m.getSysExData();
        const int n = m.getSysExDataSize();
        // 00 00 66 <modello> 12 <offset> <testo...>
        if (n < 7 || d[0] != 0x00 || d[1] != 0x00 || d[2] != 0x66 || d[4] != 0x12) return false;
        offset = d[5];
        text = juce::String::fromUTF8 (reinterpret_cast<const char*> (d + 6), n - 6);
        return true;
    }

    juce::MidiMessage deviceQuery()
    {
        const juce::uint8 data[] = { 0x00, 0x00, 0x66, 0x14, 0x00 };
        return juce::MidiMessage::createSysExMessage (data, (int) sizeof (data));
    }
}
