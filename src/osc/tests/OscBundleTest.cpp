/*
  ==============================================================================
  OscBundleTest.cpp
  PercheCremisi — parsing dei bundle OSC

  REAPER manda il feedback dentro un bundle ("#bundle" + timetag + elementi).
  Prima il parser scriveva "Bundle received (not parsed)" e buttava via tutto:
  nessuno stato arrivava, la DAW non veniva riconosciuta, nessuna conferma era
  possibile. Qui si prova che i messaggi dentro un bundle arrivino davvero.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../OscHandler.h"
#include <cstring>

class OscBundleTest : public juce::UnitTest
{
public:
    OscBundleTest() : juce::UnitTest ("OscBundle", "whycremisi") {}

    void runTest() override
    {
        testBundleConDueMessaggi();
        testBundleAnnidato();
        testBundleTroncatoNonCrasha();
        testMessaggioSemplice();
    }

private:
    struct Ricevuto { juce::String addr; float val; };

    static void scriviStringa (juce::MemoryBlock& b, const juce::String& s)
    {
        const auto utf8 = s.toRawUTF8();
        const int len = (int) std::strlen (utf8) + 1;
        b.append (utf8, (size_t) len);
        while (b.getSize() % 4 != 0) b.append ("\0", 1);
    }

    static void scriviFloat (juce::MemoryBlock& b, float v)
    {
        uint32_t bits;
        std::memcpy (&bits, &v, 4);
        const uint8_t be[4] = { (uint8_t)(bits >> 24), (uint8_t)(bits >> 16),
                                (uint8_t)(bits >> 8),  (uint8_t)bits };
        b.append (be, 4);
    }

    static juce::MemoryBlock messaggio (const juce::String& addr, float valore)
    {
        juce::MemoryBlock m;
        scriviStringa (m, addr);
        scriviStringa (m, ",f");
        scriviFloat (m, valore);
        return m;
    }

    static juce::MemoryBlock bundleCon (const juce::MemoryBlock& a, const juce::MemoryBlock& b)
    {
        juce::MemoryBlock out;
        out.append ("#bundle\0", 8);
        const uint8_t timetag[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };   // "immediato"
        out.append (timetag, 8);

        auto aggiungi = [&out] (const juce::MemoryBlock& m)
        {
            const uint32_t len = (uint32_t) m.getSize();
            const uint8_t lenBe[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16),
                                       (uint8_t)(len >> 8),  (uint8_t)len };
            out.append (lenBe, 4);
            out.append (m.getData(), m.getSize());
        };

        aggiungi (a);
        aggiungi (b);
        return out;
    }

    static std::vector<Ricevuto> parse (OscHandler& h, const juce::MemoryBlock& pkt)
    {
        std::vector<Ricevuto> out;
        h.setMessageCallback ([&out] (const juce::String& addr, const juce::var& v)
        {
            out.push_back ({ addr, (float) (double) v });
        });
        h.handleOscPacket ((const char*) pkt.getData(), (int) pkt.getSize());
        return out;
    }

    void testBundleConDueMessaggi()
    {
        beginTest ("Bundle con due messaggi: arrivano entrambi");

        OscHandler h (0);
        auto pkt = bundleCon (messaggio ("/track/1/mute", 1.0f),
                              messaggio ("/tempo/raw", 100.0f));
        auto r = parse (h, pkt);

        expectEquals ((int) r.size(), 2, "due messaggi dentro un bundle");
        if (r.size() == 2)
        {
            expectEquals (r[0].addr, juce::String ("/track/1/mute"));
            expectWithinAbsoluteError (r[0].val, 1.0f, 0.001f);
            expectEquals (r[1].addr, juce::String ("/tempo/raw"));
            expectWithinAbsoluteError (r[1].val, 100.0f, 0.001f);
        }
    }

    void testBundleAnnidato()
    {
        beginTest ("Bundle annidato: si scende di un livello");

        OscHandler h (0);
        auto interno = bundleCon (messaggio ("/track/2/volume/db", -6.0f),
                                  messaggio ("/track/2/pan", -0.5f));
        auto esterno = bundleCon (messaggio ("/track/1/select", 1.0f), interno);

        auto r = parse (h, esterno);
        expectEquals ((int) r.size(), 3);
        if (r.size() == 3)
        {
            expectEquals (r[0].addr, juce::String ("/track/1/select"));
            expectEquals (r[1].addr, juce::String ("/track/2/volume/db"));
            expectWithinAbsoluteError (r[1].val, -6.0f, 0.001f);
        }
    }

    void testBundleTroncatoNonCrasha()
    {
        beginTest ("Bundle troncato: si ferma, non legge fuori dal buffer");

        OscHandler h (0);
        auto pkt = bundleCon (messaggio ("/play", 1.0f), messaggio ("/stop", 1.0f));
        // Taglia a meta' del secondo elemento.
        const int tagliato = (int) pkt.getSize() - 3;
        auto r = parse (h, juce::MemoryBlock (pkt.getData(), (size_t) tagliato));

        expect (r.size() <= 2, "niente letture oltre il buffer");
        if (! r.empty())
            expectEquals (r[0].addr, juce::String ("/play"));
    }

    void testMessaggioSemplice()
    {
        beginTest ("Messaggio singolo (non bundle): invariato");

        OscHandler h (0);
        auto r = parse (h, messaggio ("/record", 1.0f));
        expectEquals ((int) r.size(), 1);
        if (! r.empty())
            expectEquals (r[0].addr, juce::String ("/record"));
    }
};

static OscBundleTest oscBundleTest;
