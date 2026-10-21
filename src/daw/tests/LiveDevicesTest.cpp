#include "../LiveDevices.h"
#include <juce_core/juce_core.h>

class LiveDevicesTest : public juce::UnitTest
{
public:
    LiveDevicesTest() : juce::UnitTest ("LiveDevicesTest", "whycremisi") {}

    static OscHandler::OscArg num (float v) { return { false, v, {} }; }
    static OscHandler::OscArg str (const char* t) { return { true, 0.0f, t }; }

    void runTest() override
    {
        beginTest ("Nomi e numeri si separano togliendo gli indici in testa");
        {
            std::vector<OscHandler::OscArg> nomi { num (2), str ("EQ Eight"), str ("Serum") };
            auto n = LiveDevices::nomiDaRisposta (nomi, 1);
            expectEquals (n.size(), 2);
            expectEquals (n[1], juce::String ("Serum"));

            std::vector<OscHandler::OscArg> val { num (2), num (1), num (0.5f), num (0.25f) };
            auto v = LiveDevices::numeriDaRisposta (val, 2);
            expectEquals ((int) v.size(), 2);
            expectWithinAbsoluteError (v[1], 0.25, 1e-6);
        }

        beginTest ("uniscimi usa la lista piu' corta, mai parametri sfasati");
        {
            juce::StringArray n; n.add ("A"); n.add ("B"); n.add ("C");
            auto p = LiveDevices::uniscimi (n, { 1, 2, 3 }, { 0, 0 }, { 1, 1, 1 });
            expectEquals ((int) p.size(), 2);
            expectEquals (p[1].name, juce::String ("B"));
            expectEquals (p[1].index, 1);
        }

        beginTest ("trovaPerNome: esatto, parziale, numero, ambiguo, assente");
        {
            juce::StringArray d; d.add ("EQ Eight"); d.add ("Compressor"); d.add ("Glue Compressor");
            juce::String e;
            expectEquals (LiveDevices::trovaPerNome (d, "compressor", e), 1);      // esatto vince sul parziale
            expectEquals (LiveDevices::trovaPerNome (d, "eq", e), 0);              // parziale unico
            expectEquals (LiveDevices::trovaPerNome (d, "2", e), 2);               // numero
            expectEquals (LiveDevices::trovaPerNome (d, "9", e), -1);
            expect (e.isNotEmpty());
            expectEquals (LiveDevices::trovaPerNome (d, "glu", e), 2);
            expectEquals (LiveDevices::trovaPerNome (d, "comp", e), -1);           // Compressor e Glue Compressor
            expect (e.contains ("ambiguo"), e);
            expectEquals (LiveDevices::trovaPerNome (d, "reverb", e), -1);
            expect (e.contains ("EQ Eight"), "l'errore elenca cosa c'e'");
            expectEquals (LiveDevices::trovaPerNome (d, "  ", e), -1);
        }

        beginTest ("Normalizzato e reale sono inversi e restano nell'escursione");
        {
            LiveParam p { 1, "Cutoff", 10.0, 20.0, 120.0 };
            expectWithinAbsoluteError (LiveDevices::daNormalizzato (p, 0.5), 70.0, 1e-9);
            expectWithinAbsoluteError (LiveDevices::daNormalizzato (p, 2.0), 120.0, 1e-9);
            expectWithinAbsoluteError (LiveDevices::aNormalizzato (p, 70.0), 0.5, 1e-9);
            LiveParam fermo { 0, "x", 1.0, 5.0, 5.0 };
            expectWithinAbsoluteError (LiveDevices::aNormalizzato (fermo, 5.0), 0.0, 1e-9);
        }

        beginTest ("uguali: tolleranza dell'1% dell'escursione");
        {
            LiveParam p { 0, "x", 0.0, 0.0, 127.0 };
            expect (LiveDevices::uguali (p, 64.0, 64.9));
            expect (! LiveDevices::uguali (p, 64.0, 66.0));
        }
    }
};

static LiveDevicesTest liveDevicesTest;
