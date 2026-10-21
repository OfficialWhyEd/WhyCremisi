/*
  ==============================================================================
    AiEngineTest.cpp

    Copre i difetti che rendevano il plugin muto, in modo che non tornino.

    Il piu' grave riguardava il nome del provider. Il salvataggio scriveva
    "Ollama" con la maiuscola, il ripristino confrontava con "ollama"
    minuscolo, nessun confronto andava a segno e a ogni riavvio il provider
    tornava a quello predefinito — con addosso la chiave di un fornitore
    diverso. Le richieste partivano verso un servizio spento, lo streaming
    non riportava alcun errore, e chi usava il plugin vedeva soltanto una
    risposta che non arrivava mai.
  ==============================================================================
*/

#include "../AiEngine.h"
#include "../AIProvider.h"
#include <juce_core/juce_core.h>

class AiEngineTest : public juce::UnitTest
{
public:
    AiEngineTest() : juce::UnitTest ("AiEngineTest", "whycremisi") {}

    void runTest() override
    {
        using Provider = AiEngine::Provider;

        beginTest ("Il nome del provider sopravvive al giro di andata e ritorno");
        {
            const Provider tutti[] = { Provider::Ollama, Provider::Gemini,
                                       Provider::Anthropic, Provider::OpenAI,
                                       Provider::OpenRouter, Provider::Groq,
                                       Provider::ClaudeCode };

            for (auto p : tutti)
            {
                const auto id = AiEngine::providerToId (p);
                expect (id.isNotEmpty(), "Ogni provider deve avere un nome");
                expect (id == id.toLowerCase(), "Il nome canonico e' minuscolo: " + id);
                expect (AiEngine::providerFromId (id) == p,
                        "Salvato e riletto deve dare lo stesso provider: " + id);
            }
        }

        beginTest ("Si rileggono anche i nomi salvati dalle versioni precedenti");
        {
            // Sono le forme che si trovano davvero nei file gia' sul disco.
            expect (AiEngine::providerFromId ("Ollama")           == Provider::Ollama);
            expect (AiEngine::providerFromId ("Anthropic Claude") == Provider::Anthropic);
            expect (AiEngine::providerFromId ("Google Gemini")    == Provider::Gemini);
            expect (AiEngine::providerFromId ("OpenRouter")       == Provider::OpenRouter);
            expect (AiEngine::providerFromId ("  gemini  ")       == Provider::Gemini,
                    "Gli spazi attorno non devono far perdere la scelta");
        }

        beginTest ("I box scelti dal modello nel JSON: solo id validi, senza doppioni");
        {
            AiEngine engine;
            auto r = engine.parseStructuredResponse (
                R"({"text":"Ecco","actions":[],"panels":["EQ","spectral","eq","finto"," loudness "]})");
            expect (r.success);
            expect (r.text == "Ecco");
            expect (r.panels.size() == 3, "Attesi 3 box: " + r.panels.joinIntoString (","));
            expect (r.panels[0] == "eq" && r.panels[1] == "spectral" && r.panels[2] == "loudness");

            // Senza il campo (chat normale) non esce nessun box.
            auto c = engine.parseStructuredResponse (R"({"text":"Ciao"})");
            expect (c.panels.isEmpty());

            // Un campo di tipo sbagliato non deve rompere la risposta.
            auto s = engine.parseStructuredResponse (R"({"text":"x","panels":"eq"})");
            expect (s.success && s.panels.isEmpty());
        }

        beginTest ("Claude Code non viene scambiato per Anthropic");
        {
            // Anthropic usa la chiave API, Claude Code il login del programma
            // ufficiale: confonderli manderebbe l'utente su un percorso senza chiave.
            expect (AiEngine::providerFromId ("claudecode")             == Provider::ClaudeCode);
            expect (AiEngine::providerFromId ("Claude (il tuo account)") == Provider::ClaudeCode);
            expect (AiEngine::providerFromId ("anthropic")              == Provider::Anthropic);
            expect (AiEngine::providerFromId ("Anthropic Claude")       == Provider::Anthropic);
        }

        beginTest ("Claude Code legge la risposta del programma ufficiale");
        {
            auto ok = ClaudeCodeProvider::parseCliOutput (
                R"({"type":"result","is_error":false,"result":"Ciao!","stop_reason":"end_turn",)"
                R"("usage":{"input_tokens":12,"output_tokens":3,"cache_read_input_tokens":5},)"
                R"("modelUsage":{"claude-sonnet-5-5":{}}})");
            expect (ok.success);
            expect (ok.text == "Ciao!");
            expect (ok.stopReason == "end_turn");
            expect (ok.usage.inputTokens == 12 && ok.usage.outputTokens == 3 && ok.usage.cacheReadTokens == 5);
            expect (ok.modelUsed == "claude-sonnet-5-5");

            auto err = ClaudeCodeProvider::parseCliOutput (R"({"is_error":true,"result":"Not logged in"})");
            expect (! err.success);
            expect (err.error == "Not logged in");

            expect (! ClaudeCodeProvider::parseCliOutput ("non e' json").success);
            expect (! ClaudeCodeProvider::parseCliOutput ({}).success);
        }

        beginTest ("Claude Code parte senza strumenti ne' sessione, con argomenti sicuri");
        {
            auto a = ClaudeCodeProvider::buildArguments ("sonnet", "high", "C:/t/sys.txt");
            expect (a.contains ("-p"));
            expect (a.contains ("--no-session-persistence"));
            expect (a.contains ("--tools") && a[a.indexOf ("--tools") + 1].isEmpty(),
                    "Nessuno strumento: deve rispondere solo con testo");
            expect (a[a.indexOf ("--model") + 1] == "sonnet");
            expect (a[a.indexOf ("--system-prompt-file") + 1] == "C:/t/sys.txt");

            // Un modello con caratteri di shell non deve arrivare alla riga di comando.
            auto b = ClaudeCodeProvider::buildArguments ("sonnet\" & calc & \"", {}, {});
            expect (b[b.indexOf ("--model") + 1] == "sonnetcalc",
                    "Il modello va ripulito: " + b[b.indexOf ("--model") + 1]);
            expect (! b.contains ("--effort"));
        }

        beginTest ("Claude Code compone il turno con la cronologia");
        {
            auto p = ClaudeCodeProvider::composePrompt (
                R"([{"role":"user","content":"ciao"},{"role":"assistant","content":"eccomi"}])", "alza il basso");
            expect (p.contains ("Utente: ciao"));
            expect (p.contains ("Assistente: eccomi"));
            expect (p.trimEnd().endsWith ("Utente: alza il basso"));
            expect (ClaudeCodeProvider::composePrompt ("rotto", "x").trim() == "Utente: x");
        }

        beginTest ("OpenRouter PKCE: sfida, codice e indirizzo");
        {
            const auto a = OpenRouterAuth::creaSfida();
            const auto b = OpenRouterAuth::creaSfida();
            expect (a.verifier.length() >= 43 && a.verifier != b.verifier);
            expect (a.challenge.length() == 43);   // SHA-256 in base64url senza padding
            expect (! a.challenge.containsAnyOf ("+/="));
            const auto h = juce::SHA256 (a.verifier.toRawUTF8(), (size_t) a.verifier.getNumBytesAsUTF8()).toHexString();
            expect (h.length() == 64);
            expect (OpenRouterAuth::codiceDaRichiesta ("GET /?code=abc123 HTTP/1.1") == "abc123");
            expect (OpenRouterAuth::codiceDaRichiesta ("GET /favicon.ico HTTP/1.1").isEmpty());
            const auto u = OpenRouterAuth::urlAutorizzazione (4321, "XYZ");
            expect (u.startsWith ("https://openrouter.ai/auth?callback_url="));
            expect (u.contains ("localhost%3A4321") && u.contains ("code_challenge=XYZ") && u.contains ("S256"));
        }

        beginTest ("Codex legge gli eventi JSONL e riconosce gli errori");
        {
            const juce::String ok =
                "{\"type\":\"thread.started\",\"thread_id\":\"x\"}\n"
                "{\"type\":\"item.completed\",\"item\":{\"id\":\"i\",\"type\":\"agent_message\",\"text\":\"ciao\"}}\n"
                "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":10,\"cached_input_tokens\":4,\"output_tokens\":2}}\n";
            auto r = CodexProvider::parseCliOutput (ok);
            expect (r.success);
            expect (r.text == "ciao");
            expect (r.usage.inputTokens == 10 && r.usage.outputTokens == 2 && r.usage.cacheReadTokens == 4);

            auto e = CodexProvider::parseCliOutput ("{\"type\":\"turn.failed\",\"error\":{\"message\":\"non autorizzato\"}}\n");
            expect (! e.success && e.error.contains ("non autorizzato"));
            expect (! CodexProvider::parseCliOutput ({}).success);

            expect (AiEngine::providerFromId ("codex") == Provider::Codex);
            expect (AiEngine::providerFromId ("ChatGPT") == Provider::Codex);
            expect (AiEngine::providerToId (Provider::Codex) == "codex");

            auto a = CodexProvider::buildArguments ("gpt-5\" & calc & \"", "high");
            expect (a.joinIntoString (" ").contains ("exec --json"));
            expect (! a.joinIntoString (" ").contains ("&"));
            expect (a.contains ("read-only"));
        }

        beginTest ("OpenRouter non viene scambiato per OpenAI");
        {
            // Entrambi cominciano per "open": con un confronto per prefisso
            // scritto nell'ordine sbagliato, chi sceglieva OpenRouter finiva
            // su OpenAI e la sua chiave veniva rifiutata.
            expect (AiEngine::providerFromId ("openrouter") == Provider::OpenRouter);
            expect (AiEngine::providerFromId ("openai")     == Provider::OpenAI);
        }

        beginTest ("OpenRouter parte da DeepSeek V4.1 Flash con riserva gratuita");
        {
            AIProvider::Config cfg;
            cfg.apiKey = "sk-or-prova";
            cfg.model  = OpenRouterProvider::defaultModel();
            OpenRouterProvider p (cfg);
            auto modelli = p.getAvailableModels();
            expect (modelli[0] == "deepseek/deepseek-v4.1-flash", "Il primo modello deve essere DeepSeek V4.1 Flash");
            expect (modelli.size() > 1, "Dopo DeepSeek ci sono i gratuiti");
            for (const auto& m : OpenRouterProvider::freeModels())
                expect (m.endsWith (":free") || m == "openrouter/free", "Gratuito atteso: " + m);
            // OpenRouter accetta al massimo tre modelli nel campo "models"
            expect (OpenRouterProvider::fallbackChain().size() <= 2);
            expect (! OpenRouterProvider::fallbackChain().contains (OpenRouterProvider::defaultModel()));
        }

        beginTest ("Un nome sconosciuto non cambia la scelta gia' fatta");
        {
            expect (AiEngine::providerFromId ("qualcosa-di-ignoto", Provider::Anthropic)
                        == Provider::Anthropic,
                    "Meglio tenere la scelta corrente che ripiegare in silenzio");
            expect (AiEngine::providerFromId ("", Provider::Gemini) == Provider::Gemini);
        }

        beginTest ("Senza credenziali lo streaming dice perche', invece di tacere");
        {
            // Era il secondo difetto: ogni fallimento chiudeva il flusso con
            // un pezzo vuoto e nessuna spiegazione. La UI riceveva una
            // risposta bianca e non c'era modo di capire cosa correggere.
            AIProvider::Config cfg;   // nessuna chiave, nessun token
            AnthropicProvider claude (cfg);

            bool conclusa = false;
            juce::String testo;
            claude.sendPromptStreaming ("sistema", "ciao",
                [&] (const juce::String& pezzo, bool fine)
                {
                    testo += pezzo;
                    if (fine) conclusa = true;
                });

            expect (conclusa, "Il segnale di fine deve arrivare sempre, altrimenti "
                              "il bridge resta convinto che una richiesta sia in corso");
            expect (testo.isEmpty(), "Senza credenziali non puo' esserci testo");
            expect (claude.getLastError().isNotEmpty(),
                    "Il motivo del fallimento deve essere leggibile");
        }

        beginTest ("Lo stesso vale per gli altri fornitori");
        {
            AIProvider::Config cfg;
            OpenAIProvider gpt (cfg);
            GeminiProvider gemini (cfg);

            for (AIProvider* p : { static_cast<AIProvider*> (&gpt),
                                   static_cast<AIProvider*> (&gemini) })
            {
                bool conclusa = false;
                p->sendPromptStreaming ("sistema", "ciao",
                    [&] (const juce::String&, bool fine) { if (fine) conclusa = true; });

                expect (conclusa, "Fine mancante da " + p->getName());
                expect (p->getLastError().isNotEmpty(), "Motivo mancante da " + p->getName());
            }
        }

        beginTest ("La verifica della connessione non dice di si' senza controllare");
        {
            // OpenAIProvider::testConnection() restituiva true e basta:
            // qualunque chiave, anche inventata, risultava valida, e il
            // problema saltava fuori solo al primo prompt.
            AIProvider::Config cfg;
            OpenAIProvider gpt (cfg);
            expect (! gpt.testConnection(), "Senza chiave non puo' risultare connesso");
            expect (gpt.getLastError().isNotEmpty(), "E deve dire perche'");
        }

        beginTest ("Il modello predefinito di Claude e' quello di punta");
        {
            expect (AnthropicProvider::defaultModel() == "claude-opus-5",
                    "Opus 5 e' il modello di riferimento del progetto");
        }
    }
};

static AiEngineTest aiEngineTest;
