#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

class AIProvider
{
public:
    virtual ~AIProvider() = default;

    struct Config {
        juce::String apiKey;
        juce::String model;
        juce::String baseUrl;
        int timeoutMs = 30000;
        int maxTokens = 2048;
        float temperature = 0.7f;
        juce::String toolsJson;          // JSON array of tool definitions (OpenAI/Anthropic format)
        juce::String contextMessages;     // JSON array of prior messages for multi-turn

        // Token OAuth di un abbonamento Claude (Pro / Max / Claude Code).
        // Se valorizzato ha la precedenza sulla apiKey: si autentica con
        // "Authorization: Bearer", non con "x-api-key", e richiede il beta
        // header oauth-2025-04-20. Serve a chi ha gia' un abbonamento e non
        // vuole procurarsi una chiave API separata.
        juce::String authToken;

        // Profondita' di ragionamento su Claude: low | medium | high | xhigh | max.
        // Vuoto = default del modello (high).
        juce::String effort;
    };

    struct ToolCallResult {
        juce::String id;
        juce::String name;
        nlohmann::json arguments;
        juce::String output;             // filled by tool executor
        bool hasOutput = false;
    };

    // Consumo di una singola richiesta. Serve a mostrare all'utente quanto
    // sta spendendo, e a capire se il prompt caching sta funzionando: se
    // cacheReadTokens resta a zero fra richieste che condividono lo stesso
    // preambolo, qualcosa lo sta invalidando.
    struct Usage {
        int inputTokens = 0;
        int outputTokens = 0;
        int cacheReadTokens = 0;
        int cacheWriteTokens = 0;

        int total() const { return inputTokens + outputTokens + cacheReadTokens + cacheWriteTokens; }
        bool isEmpty() const { return total() == 0; }
    };

    struct Result {
        juce::String text;
        bool success = false;
        juce::String error;
        std::vector<ToolCallResult> toolCalls;  // populated when model requests tool use
        Usage usage;
        juce::String stopReason;   // end_turn | tool_use | max_tokens | refusal | ...
        juce::String modelUsed;    // modello che ha effettivamente risposto
    };

    using StreamCallback = std::function<void(const juce::String& chunk, bool isDone)>;

    virtual Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) = 0;
    virtual void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) = 0;
    virtual void abort() = 0;
    virtual bool testConnection() = 0;
    virtual juce::StringArray getAvailableModels() = 0;
    virtual juce::String getName() const = 0;

    void configure(const Config& cfg) { config = cfg; }
    const Config& getConfig() const { return config; }
    virtual void setToolsJson(const juce::String& json) { config.toolsJson = json; }
    virtual void setContextMessages(const juce::String& json) { config.contextMessages = json; }

    // ── Esito dell'ultima chiamata in streaming ────────────────────
    //
    //  Lo streaming consegna il testo un pezzo alla volta e non puo'
    //  restituire un Result, quindi prima non aveva modo di dire *perche'*
    //  era fallito: chiave sbagliata, servizio spento o quota finita
    //  arrivavano tutti alla UI come una risposta vuota. Chi guardava
    //  vedeva il plugin non rispondere e basta.
    //
    //  Ora ogni fallimento lascia qui un motivo leggibile, che il bridge
    //  legge quando il callback segnala isDone.
    juce::String getLastError() const { return lastError; }
    int getLastStatusCode() const { return lastStatusCode; }
    bool wasAborted() const { return lastAborted; }
    /** L'ultima risposta si e' fermata per limite di token (finish_reason=length). */
    bool wasTruncated() const { return lastTruncated; }
    void clearLastError() { lastError.clear(); lastStatusCode = 0; lastAborted = false; lastTruncated = false; }

    // ── Tool call dell'ultimo streaming ────────────────────────────
    //
    //  Il testo che arriva a onChunk e' solo cio' che il modello ha
    //  scritto. Le chiamate agli strumenti viaggiano qui, come dati:
    //  un marcatore di testo non e' una tool call, e trattarlo come tale
    //  e' esattamente il difetto che questa struttura elimina.
    const std::vector<ToolCallResult>& getStreamToolCalls() const { return streamToolCalls; }
    void clearStreamToolCalls() { streamToolCalls.clear(); }

protected:
    std::vector<ToolCallResult> streamToolCalls;

protected:
    Config config;
    std::atomic<bool> abortRequested{false};

    juce::String lastError;
    bool lastTruncated = false;
    int lastStatusCode = 0;
    bool lastAborted = false;

    // Da chiamare all'inizio di ogni richiesta: azzera l'esito precedente e
    // il flag di annullamento, che altrimenti farebbe interrompere subito la
    // richiesta successiva.
    void beginRequest() { clearLastError(); abortRequested = false; }
};

// ── Accumulatori delle tool call in streaming ──────────────────────
//
//  Vivono fuori dal trasporto HTTP apposta: ricevono i pezzi come
//  arrivano e restituiscono le chiamate complete. Cosi' si possono
//  provare con chunk finti, senza rete e senza server.
//
//  Regola comune a tutti: gli argomenti arrivano a frammenti e si
//  interpretano solo alla fine. Se il JSON non e' valido non si indovina
//  e non si perde niente: si conserva il testo grezzo sotto
//  "__raw_arguments" e chi esegue riporta l'errore.
struct ToolCallAccumulator
{
    void clear();
    void addChunk (int index, const juce::String& id, const juce::String& name,
                   const juce::String& argumentsFragment);
    std::vector<AIProvider::ToolCallResult> finish() const;

private:
    std::map<int, AIProvider::ToolCallResult> calls;
    std::map<int, juce::String> argumentiGrezzi;
    std::vector<int> ordine;
};

// Anthropic manda un blocco "tool_use" (con id e nome) e poi una serie di
// input_json_delta che compongono gli argomenti; la fine la dichiara
// content_block_stop.
struct AnthropicToolAccumulator
{
    void clear();
    void startBlock (int index, const juce::String& id, const juce::String& name);
    void addJsonFragment (int index, const juce::String& fragment);
    void stopBlock (int index);
    std::vector<AIProvider::ToolCallResult> finish() const;
    bool isToolBlock (int index) const { return calls.count (index) > 0; }

private:
    std::map<int, AIProvider::ToolCallResult> calls;
    std::map<int, juce::String> buffer;
    std::vector<int> ordine;
};

// Gemini manda le functionCall dentro i parts, con gli argomenti che
// possono arrivare a pezzi. Le chiamate non hanno id: lo assegna il motore.
struct GeminiToolAccumulator
{
    void clear();
    void addCall (const juce::String& name, const nlohmann::json& args);
    void addArgumentsFragment (const juce::String& name, const juce::String& fragment);
    std::vector<AIProvider::ToolCallResult> finish() const;

private:
    std::vector<AIProvider::ToolCallResult> calls;
    std::map<juce::String, juce::String> buffer;
};

class OpenAIProvider : public AIProvider
{
public:
    OpenAIProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override { abortRequested = true; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "OpenAI"; }

    // Se il servizio pretende una chiave API. Solo llama.cpp, che gira in
    // locale e non autentica nessuno, risponde false: senza questo flag il
    // percorso OpenAI rifiuterebbe la richiesta prima di partire.
    virtual bool requiresApiKey() const { return true; }

protected:
    virtual juce::String getApiUrl() const;
    // Header aggiuntivi oltre a Authorization, per i servizi compatibili
    // OpenAI che ne richiedono di propri.
    virtual juce::String getExtraHeaders() const { return {}; }
    // Punto in cui un servizio compatibile aggiunge campi suoi al corpo
    // della richiesta (per OpenRouter: la catena di modelli di riserva).
    virtual void addRouting (nlohmann::json&) const {}
    juce::String makeHttpRequest(const juce::String& url, const juce::String& method,
                                  const juce::String& jsonBody, int timeoutMs,
                                  const juce::String& extraHeaders = {});
};

// ── llama.cpp (llama-server) in locale ──────────────────────────────
//
//  llama-server espone l'API di OpenAI (/v1/chat/completions, /v1/models),
//  quindi si riusa tutta la logica di OpenAIProvider: cambiano solo
//  l'indirizzo e il fatto che non serve nessuna chiave.
//
//  Differenza da Ollama: Ollama ha un'API sua (/api/generate, /api/tags),
//  llama.cpp quella di OpenAI. Per questo i due non sono lo stesso
//  fornitore e non basta cambiare la porta.
class LlamaCppProvider : public OpenAIProvider
{
public:
    LlamaCppProvider(const Config& cfg) : OpenAIProvider(cfg) {}
    juce::String getName() const override { return "llama.cpp (locale)"; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;

    // llama-server parte sulla 8080, non sulla 11434 di Ollama.
    static juce::String defaultBaseUrl() { return "http://127.0.0.1:8080"; }

    bool requiresApiKey() const override { return false; }

protected:
    juce::String getApiUrl() const override;
    juce::String getExtraHeaders() const override { return {}; }
};

class AnthropicProvider : public AIProvider
{
public:
    AnthropicProvider(const Config& cfg) { configure(cfg); }    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override { abortRequested = true; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "Anthropic Claude"; }

    // Modello di riferimento del progetto.
    static juce::String defaultModel() { return "claude-opus-5-5"; }

    // Etichetta leggibile per la UI: "Opus 5" invece di "claude-opus-5-5".
    static juce::String displayNameFor (const juce::String& modelId);

    // Token passato dall'utente stesso con la variabile ANTHROPIC_AUTH_TOKEN
    // (il meccanismo ufficiale dell'SDK). Non cerchiamo e non conserviamo
    // niente altro: per usare l'abbonamento c'e' ClaudeCodeProvider.
    static juce::String detectSubscriptionToken();

    // true quando l'autenticazione avviene con un abbonamento Claude invece
    // che con una chiave API.
    bool usesSubscription() const { return config.authToken.isNotEmpty(); }

private:
    juce::String makeHttpRequest(const juce::String& url, const juce::String& method,
                                  const juce::String& jsonBody, int timeoutMs,
                                  const juce::String& extraHeaders = {});

    // Header di autenticazione + versione API, uguali per tutte le chiamate.
    juce::String buildHeaders() const;

    // Corpo della richiesta condiviso fra chiamata singola e streaming.
    nlohmann::json buildRequestBody (const juce::String& systemPrompt,
                                     const juce::String& userMessage) const;
};

// ── Account Claude tramite il programma ufficiale ───────────────────
//
//  Chi ha un abbonamento Claude entra con il login del programma ufficiale
//  "claude": WhyCremisi lo lancia per ogni richiesta e legge la risposta.
//  Il login lo fa l'utente nel browser, dentro "claude"; il plugin non vede,
//  non copia e non salva nessun token o file di credenziali.
class ClaudeCodeProvider : public AIProvider
{
public:
    ClaudeCodeProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override;
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "Claude (il tuo account)"; }

    static juce::String defaultModel() { return "claude-sonnet-5-5"; }
    static juce::StringArray knownModels();
    static bool isKnownModel (const juce::String& m);

    // Percorso dell'eseguibile "claude", se installato. Vuoto altrimenti.
    static juce::File findExecutable();

    struct LoginState
    {
        bool installed = false;
        bool loggedIn = false;
        juce::String method;    // es. "claude.ai"
    };

    // Chiede a "claude auth status" se l'utente ha gia' fatto il login.
    static LoginState queryLoginState();

    // Apre una finestra di terminale con "claude auth login": l'utente
    // completa l'accesso nel browser. Ritorna false se non parte.
    static bool openLoginTerminal();

    // Estrae il testo da {"result": ...} di "--output-format json".
    // Separato dal resto per poterlo provare senza lanciare niente.
    static Result parseCliOutput (const juce::String& raw);

    // Argomenti fissi: niente strumenti, niente sessione salvata, niente
    // impostazioni dell'utente. Solo il modello che risponde al testo.
    static juce::StringArray buildArguments (const juce::String& model,
                                             const juce::String& effort,
                                             const juce::String& systemPromptFile);

    // Comando completo per la shell: cartella di lavoro, eseguibile,
    // argomenti e prompt letto da file (virgolette e a capo non passano
    // bene dagli argomenti).
    static juce::String buildShellCommand (const juce::File& exe,
                                           const juce::StringArray& args,
                                           const juce::File& promptFile,
                                           const juce::File& workDir);

    // Testo da dare a "claude" per un turno: cronologia + messaggio.
    static juce::String composePrompt (const juce::String& contextMessagesJson,
                                       const juce::String& userMessage);

private:
    std::mutex procMutex;
    juce::ChildProcess* running = nullptr;
};

// ── Account ChatGPT tramite il programma ufficiale ──────────────────
//
//  Chi ha un abbonamento ChatGPT entra con il login del programma ufficiale
//  "codex" (OpenAI): WhyCremisi lo lancia per ogni richiesta e legge la
//  risposta. Il login lo fa l'utente nel browser; il plugin non vede,
//  non copia e non salva nessun token.
class CodexProvider : public AIProvider
{
public:
    CodexProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override;
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "ChatGPT (il tuo account)"; }

    // "default" = il modello predefinito dell'abbonamento.
    static juce::String defaultModel() { return "default"; }

    static juce::File findExecutable();

    struct LoginState
    {
        bool installed = false;
        bool loggedIn = false;
        juce::String method;    // "ChatGPT" oppure "API key"
    };

    static LoginState queryLoginState();
    static bool openLoginTerminal();

    // Legge l'output --json (una riga JSON per evento).
    static Result parseCliOutput (const juce::String& raw);

    // Solo testo: sandbox in sola lettura, nessuna sessione salvata,
    // nessuna configurazione dell'utente.
    static juce::StringArray buildArguments (const juce::String& model, const juce::String& effort);

private:
    std::mutex procMutex;
    juce::ChildProcess* running = nullptr;
};

class OllamaProvider : public OpenAIProvider
{
public:
    OllamaProvider(const Config& cfg) : OpenAIProvider(cfg) {}
    juce::String getName() const override { return "Ollama"; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;

    // Ollama in locale non chiede credenziali.
    bool requiresApiKey() const override { return false; }

protected:
    // /v1/chat/completions: l'endpoint compatibile OpenAI. La vecchia API
    // /api/generate non conosceva gli strumenti.
    juce::String getApiUrl() const override;
};

// ── Storico canonico: traduzione per dialetto ──────────────────────
//
//  Lo storico interno e' in forma neutra (OpenAI). Queste due funzioni lo
//  traducono nei dialetti che non lo accettano cosi' com'e'. Sono libere e
//  pubbliche apposta: si provano con JSON finti, senza rete.
nlohmann::json anthropicMessagesFromNeutral (const juce::String& contextJson);
nlohmann::json geminiContentsFromNeutral (const juce::String& contextJson,
                                          const juce::String& userMessage);

class GeminiProvider : public AIProvider
{
public:
    GeminiProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override { abortRequested = true; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "Google Gemini"; }

    // Modello leggero consigliato da Google per i progetti nuovi (pagina
    // dei modelli, 22/10/2026). gemini-2.0-flash e' spento.
    // "gemini-flash-latest" e' l'alias di Google che punta sempre al Flash
    // piu' recente: si aggiorna da solo quando esce un modello nuovo.
    static juce::String defaultModel() { return "gemini-flash-latest"; }

    // Riserve in ordine: i piu' recenti e capienti per primi, poi i leggeri
    // (piu' chiamate al minuto). Quando uno esaurisce la quota si passa al
    // successivo. Elenco della pagina dei modelli, 22/10/2026.
    static juce::StringArray fallbackModels()
    {
        return { "gemini-flash-latest", "gemini-3.8-flash", "gemini-3.6-flash",
                 "gemini-3.5-flash-lite", "gemini-3.1-flash-lite", "gemini-2.5-flash" };
    }

    // true per i modelli spenti da Google (serie 1.x e 2.0): chi li aveva
    // salvati riceverebbe un errore senza capirne il motivo.
    static bool isRetiredModel (const juce::String& modelId);

    // true per i modelli che non rispondono a una chat di testo (voce,
    // immagini, live, trascrizione, embedding, robotica, musica).
    static bool isNonChatModel (const juce::String& modelId);

private:
    juce::String makeHttpRequest(const juce::String& url, const juce::String& method,
                                  const juce::String& jsonBody, int timeoutMs,
                                  const juce::String& extraHeaders = {});
    void setSearchEnabled(bool enabled) { searchEnabled = enabled; }
    bool searchEnabled = false;
};

// OpenRouter fa da passacarte verso i modelli di molti fornitori con una
// chiave sola, e offre alcuni modelli gratuiti. E' la via d'accesso per chi
// non ha un abbonamento Claude ne' vuole pagare a consumo: la chiave si crea
// gratis e i modelli con suffisso ":free" non si pagano.
class OpenRouterProvider : public OpenAIProvider
{
public:
    using OpenAIProvider::OpenAIProvider;
    juce::String getName() const override { return "OpenRouter"; }
    juce::StringArray getAvailableModels() override;
    bool testConnection() override;

    // Primo modello proposto: DeepSeek V4.1 Flash, costa frazioni di
    // centesimo per migliaia di richieste e regge le chiamate di strumenti.
    static juce::String defaultModel() { return "deepseek/deepseek-v4.1-flash"; }

    // Catena di riserva: se il modello scelto e' sovraccarico, senza
    // credito o limitato, OpenRouter passa da solo al successivo.
    static juce::StringArray fallbackChain();

    // Modelli utilizzabili senza spendere nulla (con chiamate di strumenti).
    static juce::StringArray freeModels();
    static bool isFreeModel (const juce::String& modelId) { return modelId.endsWith (":free"); }

private:
    juce::String getApiUrl() const override { return "https://openrouter.ai/api/v1/chat/completions"; }
    juce::String getExtraHeaders() const override;
    void addRouting (nlohmann::json& body) const override;
};

// OpenCode (modelli gratuiti, tra cui DeepSeek V4.1 Flash). Il server di
// OpenCode accetta i modelli gratuiti solo da dentro il suo programma
// (risposta 403 "FreeTierError" a chiunque altro), quindi WhyCremisi lancia
// il programma ufficiale "opencode run", come fa con Codex. Nessun account,
// nessuna chiave: basta avere OpenCode installato. Pagina: opencode.ai/docs/zen
class OpenCodeProvider : public AIProvider
{
public:
    OpenCodeProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override;
    bool testConnection() override;
    juce::StringArray getAvailableModels() override { return freeModels(); }
    juce::String getName() const override { return "OpenCode"; }

    static juce::String defaultModel() { return "deepseek-v4.1-flash-free"; }

    // Modelli gratuiti (elenco della pagina Zen del 22/10/2026). Quelli
    // "per uso di prova" non vanno usati con dati privati.
    static juce::StringArray freeModels();

    // true se l'id e' uno dei modelli di OpenCode (per correggere un id
    // rimasto da un altro fornitore).
    static bool isKnownModel (const juce::String& modelId);

    static juce::File findExecutable();

    // Legge l'output --format json (una riga JSON per evento).
    static Result parseCliOutput (const juce::String& raw);

    // Solo testo, nessuna sessione da riprendere.
    static juce::StringArray buildArguments (const juce::String& model);

private:
    std::mutex procMutex;
    juce::ChildProcess* running = nullptr;
};

class GroqProvider : public OpenAIProvider
{
public:
    using OpenAIProvider::OpenAIProvider;
    juce::String getName() const override { return "Groq"; }
    juce::StringArray getAvailableModels() override;
private:
    juce::String getApiUrl() const override { return "https://api.groq.com/openai/v1/chat/completions"; }
};

//==============================================================================
// Accesso a OpenRouter con un clic (OAuth PKCE): si apre la pagina ufficiale,
// l'utente accede, OpenRouter rimanda a una porta locale con un codice che si
// scambia con una chiave. Nessun segreto da copiare a mano.
struct OpenRouterAuth
{
    struct Sfida { juce::String verifier, challenge; };
    static Sfida creaSfida();                                   // verifier casuale + SHA-256 in base64url
    static juce::String codiceDaRichiesta (const juce::String& primaRiga);   // "GET /?code=XYZ HTTP/1.1" -> "XYZ"
    static juce::String urlAutorizzazione (int porta, const juce::String& challenge);
    // Blocca fino al rientro (o al tempo massimo). Ritorna la chiave, o stringa vuota con l'errore in 'errore'.
    static juce::String accedi (int timeoutSecondi, juce::String& errore, std::function<void (const juce::String&)> apriUrl,
                                const std::atomic<bool>* stop = nullptr);
};
