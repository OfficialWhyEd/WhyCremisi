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
                                       Provider::OpenRouter, Provider::Groq };

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
