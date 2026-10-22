#include "../ChainProvider.h"
#include <juce_core/juce_core.h>

namespace
{
    // Provider finto: risponde "ok" oppure fallisce con un errore, e conta le chiamate.
    struct Finto : AIProvider
    {
        Finto (juce::String n, bool ok, juce::String err = {}, int status = 0, bool mezzo = false)
            : nome (n), funziona (ok), errore (err), stato (status), mezzaFrase (mezzo) {}

        Result sendPrompt (const juce::String&, const juce::String&) override
        {
            ++chiamate;
            beginRequest();
            Result r;
            r.success = funziona;
            if (funziona) r.text = nome + " risponde";
            else          r.error = errore;
            return r;
        }
        void sendPromptStreaming (const juce::String&, const juce::String&, StreamCallback cb) override
        {
            ++chiamate;
            beginRequest();
            if (funziona || mezzaFrase)
                cb (nome + " risponde", false);
            if (! funziona) { lastError = errore; lastStatusCode = stato; }
            cb ({}, true);
        }
        void abort() override {}
        bool testConnection() override { return funziona; }
        juce::StringArray getAvailableModels() override { return { nome + "-modello" }; }
        juce::String getName() const override { return nome; }

        juce::String nome; bool funziona; juce::String errore; int stato; bool mezzaFrase;
        int chiamate = 0;
    };

    std::shared_ptr<Finto> f (juce::String n, bool ok, juce::String e = {}, int s = 0, bool m = false)
    { return std::make_shared<Finto> (n, ok, e, s, m); }
}

class ChainProviderTest : public juce::UnitTest
{
public:
    ChainProviderTest() : juce::UnitTest ("ChainProviderTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("Se il primo risponde, gli altri non vengono toccati");
        {
            auto a = f ("A", true); auto b = f ("B", true);
            ChainProvider c ({ a, b });
            auto r = c.sendPrompt ("s", "u");
            expect (r.success && r.text == "A risponde");
            expect (a->chiamate == 1 && b->chiamate == 0);
            expect (c.getLastAnswerer() == "A");
        }

        beginTest ("Quota finita sul primo: risponde il secondo");
        {
            auto a = f ("A", false, "429 quota", 429); auto b = f ("B", true);
            ChainProvider c ({ a, b });
            auto r = c.sendPrompt ("s", "u");
            expect (r.success && r.text == "B risponde");
            expect (c.getLastAnswerer() == "B");
        }

        beginTest ("Tutti falliscono: nessun successo finto, errori di tutti elencati");
        {
            auto a = f ("A", false, "chiave rifiutata"); auto b = f ("B", false, "servizio spento");
            ChainProvider c ({ a, b });
            auto r = c.sendPrompt ("s", "u");
            expect (! r.success);
            expect (r.error.contains ("A: chiave rifiutata") && r.error.contains ("B: servizio spento"), r.error);
            expect (c.getLastAnswerer().isEmpty());
        }

        beginTest ("Catena vuota: errore chiaro");
        {
            ChainProvider c ({});
            expect (! c.sendPrompt ("s", "u").success);
            std::vector<juce::String> out; bool fine = false;
            c.sendPromptStreaming ("s", "u", [&] (const juce::String& ch, bool d) { if (ch.isNotEmpty()) out.push_back (ch); if (d) fine = true; });
            expect (fine && out.empty() && c.getLastError().isNotEmpty());
        }

        beginTest ("Streaming: il fallimento prima del testo passa al successivo, una sola chiusura");
        {
            auto a = f ("A", false, "429", 429); auto b = f ("B", true);
            ChainProvider c ({ a, b });
            juce::String testo; int chiusure = 0;
            c.sendPromptStreaming ("s", "u", [&] (const juce::String& ch, bool d) { testo += ch; if (d) ++chiusure; });
            expect (testo == "B risponde", testo);
            expect (chiusure == 1, "chiusure " + juce::String (chiusure));
            expect (c.getLastError().isEmpty());
            expect (c.getLastAnswerer() == "B");
        }

        beginTest ("Streaming: se il testo e' gia' uscito non si cambia provider");
        {
            auto a = f ("A", false, "connessione persa", 0, true); auto b = f ("B", true);
            ChainProvider c ({ a, b });
            juce::String testo;
            c.sendPromptStreaming ("s", "u", [&] (const juce::String& ch, bool) { testo += ch; });
            expect (testo == "A risponde", testo);
            expect (b->chiamate == 0);
            expect (c.getLastError().contains ("connessione persa"));
        }

        beginTest ("Streaming: tutti falliscono, errore con tutte le strade");
        {
            auto a = f ("A", false, "uno", 500); auto b = f ("B", false, "due", 401);
            ChainProvider c ({ a, b });
            c.sendPromptStreaming ("s", "u", [] (const juce::String&, bool) {});
            expect (c.getLastError().contains ("A: uno") && c.getLastError().contains ("B: due"), c.getLastError());
            expect (c.getLastStatusCode() == 401);
        }

        beginTest ("Nome e modelli: dal primo, nome con tutta la catena");
        {
            ChainProvider c ({ f ("A", true), f ("B", true) });
            expect (c.getName() == "Catena (A > B)");
            expect (c.getAvailableModels()[0] == "A-modello");
            expect (c.testConnection());
        }
    }
};

static ChainProviderTest chainProviderTest;
