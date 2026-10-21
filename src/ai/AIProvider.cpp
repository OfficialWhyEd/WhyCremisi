#include "AIProvider.h"
#include <nlohmann/json.hpp>

// ═══════════════════════════════════════════════════════════════════
//  Shared HTTP Helper
// ═══════════════════════════════════════════════════════════════════

static juce::String makeHttp(const juce::String& urlStr, const juce::String& method,
                              const juce::String& jsonBody, int timeoutMs,
                              const juce::String& extraHeaders = {})
{
    juce::URL url(urlStr);
    juce::String headers = "Content-Type: application/json\r\n";
    if (extraHeaders.isNotEmpty()) headers += extraHeaders;

    std::unique_ptr<juce::InputStream> stream;
    if (method == "GET") {
        stream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(timeoutMs).withExtraHeaders(headers));
    } else if (method == "POST") {
        stream = url.withPOSTData(jsonBody.toRawUTF8()).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(timeoutMs).withExtraHeaders(headers));
    }

    juce::String response;
    char buffer[4096];
    while (stream && !stream->isExhausted()) {
        int bytesRead = stream->read(buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) { buffer[bytesRead] = '\0'; response += juce::String(buffer); }
        else break;
    }
    return response;
}

static nlohmann::json parseJsonSafe(const juce::String& raw)
{
    try { return nlohmann::json::parse(raw.toStdString()); }
    catch (...) { return nlohmann::json(); }
}

static juce::String extractError(const juce::String& raw)
{
    auto j = parseJsonSafe(raw);
    if (j.contains("error")) {
        if (j["error"].is_object() && j["error"].contains("message"))
            return juce::String(j["error"]["message"].get<std::string>());
        if (j["error"].is_string())
            return juce::String(j["error"].get<std::string>());
    }
    return raw.substring(0, 200);
}

// ═══════════════════════════════════════════════════════════════════
//  Apertura di uno stream con diagnosi
// ═══════════════════════════════════════════════════════════════════
//
//  Prima ogni provider apriva lo stream cosi':
//
//      auto stream = url.withPOSTData(...).createInputStream(options);
//      if (!stream) { onChunk("", true); return; }
//
//  con due difetti che rendevano il plugin muto. Il primo: il motivo del
//  fallimento non usciva mai da qui. Il secondo, peggiore: una risposta di
//  errore del servizio (401 chiave sbagliata, 429 quota finita, 404 modello
//  inesistente) *non* produce uno stream nullo — produce uno stream valido
//  che contiene il JSON dell'errore. Il ciclo di lettura cercava righe
//  "data: ", non ne trovava, finiva senza aver letto niente e chiudeva con
//  una risposta vuota e nessuna spiegazione.
//
//  Qui si legge il codice di stato prima di tutto e si traduce in una frase
//  che l'utente possa capire e su cui possa agire.

struct StreamOpenResult
{
    std::unique_ptr<juce::InputStream> stream;
    int statusCode = 0;
    juce::String error;   // vuoto se e' andata bene
};

static juce::String describeHttpFailure (int status, const juce::String& body,
                                         const juce::String& servizio)
{
    const juce::String dettaglio = body.isNotEmpty() ? extractError (body) : juce::String();

    juce::String testa;
    switch (status)
    {
        case 401:
        case 403:
            testa = "Credenziali rifiutate da " + servizio
                  + ". Controlla la chiave o rifai l'accesso.";
            break;
        case 404:
            testa = "Endpoint o modello non trovato su " + servizio
                  + ". Il modello selezionato potrebbe non esistere piu'.";
            break;
        case 429:
            testa = "Troppe richieste verso " + servizio
                  + ": quota o limite di frequenza esaurito. Riprova fra poco.";
            break;
        case 400:
            testa = "Richiesta rifiutata da " + servizio + ".";
            break;
        default:
            if (status >= 500)
                testa = servizio + " ha un problema al momento (errore " + juce::String (status) + ").";
            else
                testa = servizio + " ha risposto con l'errore " + juce::String (status) + ".";
            break;
    }

    return dettaglio.isNotEmpty() ? testa + " (" + dettaglio + ")" : testa;
}

static StreamOpenResult openStreamingPost (const juce::String& urlStr,
                                           const juce::String& jsonBody,
                                           const juce::String& headers,
                                           int timeoutMs,
                                           const juce::String& servizio)
{
    StreamOpenResult esito;

    juce::URL url (urlStr);
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs (timeoutMs > 0 ? timeoutMs : 30000)
        .withExtraHeaders (headers)
        .withNumRedirectsToFollow (5)
        .withStatusCode (&esito.statusCode);

    esito.stream = url.withPOSTData (jsonBody).createInputStream (options);

    if (esito.stream == nullptr)
    {
        esito.error = "Impossibile contattare " + servizio
                    + ". Verifica la connessione"
                    + (urlStr.contains ("localhost") || urlStr.contains ("127.0.0.1")
                        ? juce::String (" e che il servizio locale sia acceso su ") + urlStr
                        : juce::String ("."));
        return esito;
    }

    if (esito.statusCode >= 400)
    {
        // Il corpo dell'errore e' corto: leggerlo tutto costa nulla e dice
        // esattamente cosa non e' piaciuto al servizio.
        auto corpo = esito.stream->readEntireStreamAsString();
        esito.error = describeHttpFailure (esito.statusCode, corpo, servizio);
        esito.stream.reset();
        return esito;
    }

    return esito;
}

// ═══════════════════════════════════════════════════════════════════
//  OpenAI Provider (also base for OpenRouter, Groq)
// ═══════════════════════════════════════════════════════════════════

juce::String OpenAIProvider::getApiUrl() const
{
    return "https://api.openai.com/v1/chat/completions";
}

juce::String OpenAIProvider::makeHttpRequest(const juce::String& url, const juce::String& method,
                                              const juce::String& jsonBody, int timeoutMs,
                                              const juce::String& extraHeaders)
{
    return makeHttp(url, method, jsonBody, timeoutMs, extraHeaders);
}

static nlohmann::json buildOpenAIMessages(const juce::String& systemPrompt, const juce::String& userMessage, const juce::String& contextMessages)
{
    if (contextMessages.isNotEmpty()) {
        auto j = parseJsonSafe(contextMessages);
        if (j.is_array() && !j.empty()) {
            // If there's a system prompt, prepend it
            if (systemPrompt.isNotEmpty()) {
                j.insert(j.begin(), {{"role", "system"}, {"content", systemPrompt.toStdString()}});
            }
            j.push_back({{"role", "user"}, {"content", userMessage.toStdString()}});
            return j;
        }
    }
    nlohmann::json messages = nlohmann::json::array();
    if (systemPrompt.isNotEmpty())
        messages.push_back({{"role", "system"}, {"content", systemPrompt.toStdString()}});
    messages.push_back({{"role", "user"}, {"content", userMessage.toStdString()}});
    return messages;
}

AIProvider::Result OpenAIProvider::sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage)
{
    Result result;
    if (config.apiKey.isEmpty()) { result.error = "API key not configured"; return result; }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["messages"] = buildOpenAIMessages(systemPrompt, userMessage, config.contextMessages);
    body["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["max_tokens"] = config.maxTokens;
    body["stream"] = false;
    addRouting (body);

    if (config.toolsJson.isNotEmpty()) {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    juce::String authHeader = "Authorization: Bearer " + config.apiKey + "\r\n" + getExtraHeaders();
    juce::String raw = makeHttpRequest(getApiUrl(), "POST", juce::String(body.dump()), config.timeoutMs, authHeader);
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);

    // Consumo, nel formato OpenAI-compatibile usato anche da OpenRouter e Groq.
    if (j.contains("usage") && j["usage"].is_object()) {
        const auto& u = j["usage"];
        auto readInt = [&u](const char* key) {
            return (u.contains(key) && u[key].is_number_integer()) ? u[key].get<int>() : 0;
        };
        result.usage.inputTokens  = readInt("prompt_tokens");
        result.usage.outputTokens = readInt("completion_tokens");
    }

    if (j.contains("model") && j["model"].is_string())
        result.modelUsed = juce::String(j["model"].get<std::string>());

    if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
        if (j["choices"][0].contains("finish_reason") && j["choices"][0]["finish_reason"].is_string())
            result.stopReason = juce::String(j["choices"][0]["finish_reason"].get<std::string>());
        auto& msg = j["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null())
            result.text = juce::String(msg["content"].get<std::string>());
        // Parse tool_calls
        if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
            for (auto& tc : msg["tool_calls"]) {
                ToolCallResult tcr;
                tcr.id = juce::String(tc["id"].get<std::string>());
                tcr.name = juce::String(tc["function"]["name"].get<std::string>());
                if (tc["function"].contains("arguments"))
                    tcr.arguments = nlohmann::json::parse(tc["function"]["arguments"].get<std::string>());
                result.toolCalls.push_back(tcr);
            }
        }
        result.success = true;
    } else {
        result.error = extractError(raw);
    }
    return result;
}

void OpenAIProvider::sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    if (!onChunk) return;
    beginRequest();

    if (config.apiKey.isEmpty()) {
        lastError = "Nessuna chiave configurata per " + getName() + ".";
        onChunk("", true);
        return;
    }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["messages"] = buildOpenAIMessages(systemPrompt, userMessage, config.contextMessages);
    body["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["max_tokens"] = config.maxTokens;
    body["stream"] = true;
    addRouting (body);

    if (config.toolsJson.isNotEmpty()) {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    juce::String authHeader = "Content-Type: application/json\r\nAuthorization: Bearer "
                            + config.apiKey + "\r\n" + getExtraHeaders();

    auto apertura = openStreamingPost (getApiUrl(), body.dump(), authHeader,
                                       config.timeoutMs, getName());
    if (apertura.stream == nullptr) {
        lastError = apertura.error;
        lastStatusCode = apertura.statusCode;
        onChunk("", true);
        return;
    }
    auto stream = std::move (apertura.stream);

    bool ricevutoQualcosa = false;
    juce::String line;
    char ch;
    while (stream->isExhausted() == false && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            if (line.startsWith("data: ")) {
                juce::String data = line.substring(6).trim();
                if (data == "[DONE]") break;
                auto j = parseJsonSafe(data);
                if (j.contains("error")) {
                    lastError = extractError (data);
                    break;
                }
                if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
                    ricevutoQualcosa = true;
                    auto delta = j["choices"][0]["delta"];
                    if (delta.contains("content") && !delta["content"].is_null()) {
                        juce::String content = juce::String(delta["content"].get<std::string>());
                        onChunk(content, false);
                    }
                    // Streaming tool calls come as delta with tool_calls array
                    if (delta.contains("tool_calls") && delta["tool_calls"].is_array() && !delta["tool_calls"].empty()) {
                        for (auto& tc : delta["tool_calls"]) {
                            if (tc.contains("function") && tc["function"].contains("name") && !tc["function"]["name"].is_null()) {
                                juce::String toolName = juce::String(tc["function"]["name"].get<std::string>());
                                juce::String toolArgs;
                                if (tc["function"].contains("arguments") && !tc["function"]["arguments"].is_null())
                                    toolArgs = juce::String(tc["function"]["arguments"].get<std::string>());
                                onChunk("[TOOL_CALL:" + toolName + " " + toolArgs + "]", false);
                            }
                        }
                    }
                }
            }
            line.clear();
        } else {
            line += ch;
        }
    }

    lastAborted = abortRequested.load();
    if (lastAborted)
        lastError = "Richiesta annullata.";
    else if (!ricevutoQualcosa && lastError.isEmpty())
        lastError = getName() + " ha chiuso la connessione senza inviare nulla.";

    // Il segnale di fine va dato *sempre*, anche quando si annulla: prima
    // l'annullamento saltava questa riga e il bridge restava convinto che una
    // richiesta fosse ancora in corso, rifiutando tutte le successive con
    // "AI is already processing".
    onChunk("", true);
    abortRequested = false;
}

bool OpenAIProvider::testConnection()
{
    // Prima rispondeva true e basta: qualunque chiave, anche inventata,
    // risultava valida e il problema saltava fuori solo al primo prompt.
    if (config.apiKey.isEmpty()) { lastError = "Nessuna chiave inserita."; return false; }

    auto url = getApiUrl().upToLastOccurrenceOf ("/", false, false);   // .../v1/chat
    url = url.upToLastOccurrenceOf ("/", false, false) + "/models";    // .../v1/models

    juce::String raw = makeHttpRequest (url, "GET", {}, 10000,
                                        "Authorization: Bearer " + config.apiKey + "\r\n" + getExtraHeaders());
    if (raw.isEmpty()) { lastError = "Nessuna risposta da " + getName() + "."; return false; }

    auto j = parseJsonSafe (raw);
    if (j.contains ("data") && j["data"].is_array()) return true;

    lastError = extractError (raw);
    return false;
}

juce::StringArray OpenAIProvider::getAvailableModels()
{
    return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-3.5-turbo"};
}

// ═══════════════════════════════════════════════════════════════════
//  OpenRouter Provider
// ═══════════════════════════════════════════════════════════════════
//
//  Via d'accesso per chi non ha un abbonamento Claude e non vuole pagare a
//  consumo: una sola chiave, creabile gratis, da' accesso ai modelli di piu'
//  fornitori, e quelli con suffisso ":free" non si pagano. In cambio i
//  modelli gratuiti hanno limiti di frequenza piu' stretti e possono essere
//  lenti nelle ore di punta.

juce::String OpenRouterProvider::getExtraHeaders() const
{
    // OpenRouter usa questi due header per attribuire il traffico
    // all'applicazione che lo genera. Non sono obbligatori, ma senza il
    // plugin risulta traffico anonimo.
    return "HTTP-Referer: https://github.com/OfficialWhyEd/WhyCremisi\r\n"
           "X-Title: WhyCremisi\r\n";
}

juce::StringArray OpenRouterProvider::freeModels()
{
    // Verificati sull'elenco /api/v1/models del 21/10/2026: tutti con
    // supporto agli strumenti. Gli id DeepSeek ":free" di prima non
    // esistono piu'.
    return {
        "nvidia/nemotron-3-ultra-550b-a55b:free",
        "nvidia/nemotron-3-super-120b-a12b:free",
        "google/gemma-4-31b-it:free",
        "minimax/minimax-m3.1-flash:free",
        "openrouter/free",
    };
}

juce::StringArray OpenRouterProvider::fallbackChain()
{
    // OpenRouter accetta al massimo tre modelli in "models": il primo e'
    // quello scelto, gli altri due sono la rete di sicurezza gratuita.
    return { "nvidia/nemotron-3-ultra-550b-a55b:free", "openrouter/free" };
}

void OpenRouterProvider::addRouting (nlohmann::json& body) const
{
    nlohmann::json models = nlohmann::json::array();
    models.push_back (config.model.toStdString());
    for (const auto& m : fallbackChain())
        if (m != config.model)
            models.push_back (m.toStdString());
    body["models"] = models;
}

juce::StringArray OpenRouterProvider::getAvailableModels()
{
    // DeepSeek V4.1 Flash per primo (a pagamento ma quasi gratis), poi i
    // modelli senza costo. Chi non ha credito parte comunque: la catena
    // di riserva passa ai gratuiti.
    juce::StringArray all { defaultModel() };
    all.addArray (freeModels());
    return all;
}

bool OpenRouterProvider::testConnection()
{
    if (config.apiKey.isEmpty()) { lastError = "Nessuna chiave inserita."; return false; }

    // Interroghiamo l'elenco modelli invece di generare: e' una GET, non
    // consuma nulla e valida comunque la chiave.
    juce::String raw = makeHttpRequest ("https://openrouter.ai/api/v1/models", "GET", {},
                                        10000,
                                        "Authorization: Bearer " + config.apiKey + "\r\n" + getExtraHeaders());
    if (raw.isEmpty()) { lastError = "Nessuna risposta da OpenRouter."; return false; }

    auto j = parseJsonSafe (raw);
    if (j.contains ("data") && j["data"].is_array()) return true;

    lastError = extractError (raw);
    return false;
}

// ═══════════════════════════════════════════════════════════════════
//  Anthropic Provider
// ═══════════════════════════════════════════════════════════════════

juce::String AnthropicProvider::makeHttpRequest(const juce::String& url, const juce::String& method,
                                                 const juce::String& jsonBody, int timeoutMs,
                                                 const juce::String& extraHeaders)
{
    return makeHttp(url, method, jsonBody, timeoutMs, extraHeaders);
}

juce::String AnthropicProvider::detectSubscriptionToken()
{
    // Meccanismo ufficiale: ANTHROPIC_AUTH_TOKEN e' la variabile che l'SDK e
    // la CLI Anthropic leggono per un token di abbonamento, ed e' quella che
    // "ant auth print-credentials --env" esporta.
    //
    // Di proposito NON andiamo a leggere i file di credenziali di Claude Code
    // o della CLI: sono segreti di un altro programma, con una struttura che
    // puo' cambiare, e un plugin audio non ha motivo di frugarci dentro.
    auto fromEnv = juce::SystemStats::getEnvironmentVariable ("ANTHROPIC_AUTH_TOKEN", {});
    if (fromEnv.trim().isNotEmpty())
        return fromEnv.trim();

    // Token gia' emesso e conservato da noi in un collegamento precedente.
    // E' roba nostra, non di un altro programma: qui possiamo guardare.
    auto salvato = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("WhyCremisi").getChildFile ("accesso-claude.txt");
    if (salvato.existsAsFile())
        return salvato.loadFileAsString().trim();

    return {};
}

juce::File AnthropicProvider::findClaudeCodeExecutable()
{
    // Cerchiamo il programma, non i suoi segreti: sapere che Claude Code
    // c'e' basta per offrire il collegamento con un clic.
   #if JUCE_WINDOWS
    const char* nomi[] = { "claude.exe", "claude.cmd", "claude.bat" };
   #else
    const char* nomi[] = { "claude" };
   #endif

    // Posti dove finisce di solito l'installazione, prima del PATH: e' piu'
    // veloce e non dipende da come e' configurata la shell dell'utente.
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    juce::Array<juce::File> cartelle {
        home.getChildFile (".local").getChildFile ("bin"),
        home.getChildFile (".claude").getChildFile ("local"),
        home.getChildFile ("AppData").getChildFile ("Local").getChildFile ("Programs")
    };

    for (const auto& dir : cartelle)
        for (auto* n : nomi)
            if (auto f = dir.getChildFile (n); f.existsAsFile())
                return f;

    // Poi il PATH, voce per voce.
    const auto path = juce::SystemStats::getEnvironmentVariable ("PATH", {});
   #if JUCE_WINDOWS
    auto voci = juce::StringArray::fromTokens (path, ";", "\"");
   #else
    auto voci = juce::StringArray::fromTokens (path, ":", "\"");
   #endif

    for (const auto& voce : voci)
    {
        if (voce.trim().isEmpty()) continue;
        for (auto* n : nomi)
            if (auto f = juce::File (voce.trim()).getChildFile (n); f.existsAsFile())
                return f;
    }

    return {};
}

AnthropicProvider::LinkResult AnthropicProvider::linkAccountViaClaudeCode()
{
    LinkResult r;

    const auto exe = findClaudeCodeExecutable();
    if (! exe.existsAsFile())
    {
        r.message = "Claude Code non risulta installato su questa macchina.";
        return r;
    }

    // "setup-token" e' il comando che Claude Code espone apposta per emettere
    // un token destinato a programmi terzi. Passiamo di qui, e non dal file
    // delle credenziali, per due motivi: quel file appartiene a un altro
    // programma e la sua forma puo' cambiare senza preavviso, e soprattutto
    // il token che contiene e' emesso per Claude Code, non per noi. Qui
    // invece l'utente approva nel browser e il token nasce autorizzato.
    juce::ChildProcess proc;
    if (! proc.start (exe.getFullPathName().quoted() + " setup-token",
                      juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        r.message = "Non sono riuscito ad avviare Claude Code.";
        return r;
    }

    // L'utente deve approvare nel browser: il tempo lo detta lui, non noi.
    const auto uscita = proc.readAllProcessOutput();
    proc.waitForProcessToFinish (300000);

    // Il token si riconosce dal prefisso; l'uscita contiene anche istruzioni
    // e righe di cortesia che qui non servono.
    for (const auto& riga : juce::StringArray::fromLines (uscita))
    {
        const auto t = riga.trim();
        const auto inizio = t.indexOf ("sk-ant-");
        if (inizio < 0) continue;

        auto token = t.substring (inizio);
        // Taglia la punteggiatura o il testo attaccato subito dopo il token.
        int fine = 0;
        while (fine < token.length()
               && (juce::CharacterFunctions::isLetterOrDigit (token[fine])
                   || token[fine] == '-' || token[fine] == '_'))
            ++fine;
        token = token.substring (0, fine);

        if (token.length() > 20)
        {
            r.ok = true;
            r.token = token;
            r.message = "Account Claude collegato.";
            return r;
        }
    }

    r.message = uscita.containsIgnoreCase ("login")
              ? "Claude Code chiede prima l'accesso: aprilo una volta e riprova."
              : "Claude Code non ha restituito un token.";
    return r;
}

juce::String AnthropicProvider::displayNameFor (const juce::String& modelId)
{
    if (modelId == "claude-opus-5")    return "Opus 5";
    if (modelId == "claude-sonnet-5")  return "Sonnet 5";
    if (modelId == "claude-haiku-4-5") return "Haiku 4.5";
    return modelId;
}

juce::String AnthropicProvider::buildHeaders() const
{
    juce::String headers = "anthropic-version: 2023-06-01\r\n";

    if (config.authToken.isNotEmpty())
    {
        // Abbonamento Claude: il token va su Authorization, non su x-api-key,
        // e senza il beta header /v1/messages rifiuta la richiesta.
        headers += "Authorization: Bearer " + config.authToken + "\r\n";
        headers += "anthropic-beta: oauth-2025-04-20\r\n";
    }
    else
    {
        headers += "x-api-key: " + config.apiKey + "\r\n";
    }

    return headers;
}

nlohmann::json AnthropicProvider::buildRequestBody (const juce::String& systemPrompt,
                                                    const juce::String& userMessage) const
{
    nlohmann::json body;
    body["model"] = (config.model.isNotEmpty() ? config.model : defaultModel()).toStdString();
    body["max_tokens"] = (config.maxTokens > 0 ? config.maxTokens : 1024);

    if (systemPrompt.isNotEmpty())
        body["system"] = systemPrompt.toStdString();

    // La conversazione precedente, se c'e', va prima del messaggio corrente:
    // e' quello che rende la cache del prompt riutilizzabile fra un turno e
    // l'altro invece di ripagare l'intero preambolo ogni volta.
    nlohmann::json messages = nlohmann::json::array();
    if (config.contextMessages.isNotEmpty())
    {
        auto prior = parseJsonSafe(config.contextMessages);
        if (prior.is_array())
            for (auto& m : prior)
                messages.push_back(m);
    }
    messages.push_back({{"role", "user"}, {"content", userMessage.toStdString()}});
    body["messages"] = messages;

    // Niente temperature/top_p/top_k: sui modelli Claude attuali sono stati
    // rimossi e la richiesta verrebbe rifiutata con un 400.
    body["thinking"] = {{"type", "adaptive"}};

    if (config.effort.isNotEmpty())
        body["output_config"] = {{"effort", config.effort.toStdString()}};

    if (config.toolsJson.isNotEmpty())
    {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    return body;
}

AIProvider::Result AnthropicProvider::sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage)
{
    Result result;
    if (config.apiKey.isEmpty() && config.authToken.isEmpty())
    {
        result.error = "Nessuna credenziale Claude: serve una chiave API oppure il login con un abbonamento.";
        return result;
    }

    auto body = buildRequestBody (systemPrompt, userMessage);

    juce::String raw = makeHttpRequest("https://api.anthropic.com/v1/messages", "POST",
                                        juce::String(body.dump()), config.timeoutMs, buildHeaders());
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);

    if (j.contains("usage"))
    {
        const auto& u = j["usage"];
        auto readInt = [&u](const char* key) {
            return (u.contains(key) && u[key].is_number_integer()) ? u[key].get<int>() : 0;
        };
        result.usage.inputTokens     = readInt("input_tokens");
        result.usage.outputTokens    = readInt("output_tokens");
        result.usage.cacheReadTokens  = readInt("cache_read_input_tokens");
        result.usage.cacheWriteTokens = readInt("cache_creation_input_tokens");
    }

    if (j.contains("model") && j["model"].is_string())
        result.modelUsed = juce::String(j["model"].get<std::string>());

    if (j.contains("stop_reason") && j["stop_reason"].is_string())
        result.stopReason = juce::String(j["stop_reason"].get<std::string>());

    // Un rifiuto arriva come risposta valida, non come errore HTTP, e con
    // content vuoto: senza questo controllo sembrerebbe una risposta muta.
    if (result.stopReason == "refusal")
    {
        result.error = "La richiesta e' stata rifiutata dai filtri di sicurezza del modello.";
        return result;
    }

    if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
        for (auto& block : j["content"]) {
            juce::String type = juce::String(block["type"].get<std::string>());
            if (type == "text") {
                result.text += juce::String(block["text"].get<std::string>());
            } else if (type == "tool_use") {
                ToolCallResult tcr;
                tcr.id = juce::String(block["id"].get<std::string>());
                tcr.name = juce::String(block["name"].get<std::string>());
                if (block.contains("input"))
                    tcr.arguments = block["input"];
                result.toolCalls.push_back(tcr);
            }
            // I blocchi "thinking" arrivano vuoti se non si chiede
            // display: summarized, quindi non c'e' niente da mostrare.
        }
        result.success = true;
    } else {
        result.error = extractError(raw);
    }
    return result;
}

void AnthropicProvider::sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    if (!onChunk) return;
    beginRequest();

    if (config.apiKey.isEmpty() && config.authToken.isEmpty()) {
        lastError = "Nessun accesso configurato per Claude: collega l'abbonamento o inserisci una chiave.";
        onChunk("", true);
        return;
    }

    auto body = buildRequestBody (systemPrompt, userMessage);
    body["stream"] = true;

    auto apertura = openStreamingPost ("https://api.anthropic.com/v1/messages",
                                       body.dump(), buildHeaders(),
                                       config.timeoutMs, "Claude");
    if (apertura.stream == nullptr) {
        lastError = apertura.error;
        lastStatusCode = apertura.statusCode;
        onChunk("", true);
        return;
    }
    auto stream = std::move (apertura.stream);

    bool ricevutoQualcosa = false;
    juce::String line;
    char ch;
    while (!stream->isExhausted() && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            if (line.startsWith("data: ")) {
                auto j = parseJsonSafe(line.substring(6));
                if (j.contains("type")) {
                    juce::String type = juce::String(j["type"].get<std::string>());
                    // Claude puo' interrompere a meta' stream con un evento di
                    // errore: senza questo ramo la risposta si troncava in
                    // silenzio e sembrava che il modello non avesse finito.
                    if (type == "error") {
                        lastError = j.contains ("error") ? extractError (juce::String (j.dump()))
                                                         : juce::String ("Claude ha interrotto la risposta.");
                        break;
                    }
                    if (type == "content_block_delta" && j.contains("delta") && j["delta"].contains("text")) {
                        ricevutoQualcosa = true;
                        onChunk(juce::String(j["delta"]["text"].get<std::string>()), false);
                    }
                    if (type == "content_block_start" && j.contains("content_block") && j["content_block"].contains("type")) {
                        juce::String cbType = juce::String(j["content_block"]["type"].get<std::string>());
                        if (cbType == "tool_use") {
                            juce::String toolName = juce::String(j["content_block"]["name"].get<std::string>());
                            juce::String toolInput = j["content_block"].contains("input") ? juce::String(j["content_block"]["input"].dump()) : "";
                            onChunk("[TOOL_CALL:" + toolName + " " + toolInput + "]", false);
                        }
                    }
                    if (type == "message_stop") break;
                }
            }
            line.clear();
        } else {
            line += ch;
        }
    }

    lastAborted = abortRequested.load();
    if (lastAborted)
        lastError = "Richiesta annullata.";
    else if (!ricevutoQualcosa && lastError.isEmpty())
        lastError = "Claude ha chiuso la connessione senza inviare nulla.";

    onChunk("", true);
    abortRequested = false;
}

bool AnthropicProvider::testConnection()
{
    if (config.apiKey.isEmpty() && config.authToken.isEmpty())
    {
        lastError = "Nessun accesso configurato per Claude.";
        return false;
    }

    // Ping sul modello piu' economico. max_tokens 1 basta a validare la
    // credenziale: se e' sbagliata si torna indietro con un errore di auth
    // prima ancora di generare qualcosa.
    nlohmann::json ping;
    ping["model"] = "claude-haiku-4-5";
    ping["max_tokens"] = 1;
    ping["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", "ping"}}});

    juce::String raw = makeHttpRequest("https://api.anthropic.com/v1/messages", "POST",
                                        juce::String(ping.dump()), 10000, buildHeaders());
    if (raw.isEmpty())
    {
        lastError = "Nessuna risposta da Claude: controlla la connessione.";
        return false;
    }

    auto j = parseJsonSafe(raw);
    // Una risposta valida ha type "message"; un fallimento ha type "error".
    const bool ok = j.contains("type") && j["type"].is_string()
                 && juce::String(j["type"].get<std::string>()) == "message";
    if (!ok)
        lastError = extractError (raw);
    return ok;
}

juce::StringArray AnthropicProvider::getAvailableModels()
{
    // I modelli attuali. Gli id non portano suffisso di data: aggiungerlo
    // produce un 404. Opus 5 e' il default del progetto, Sonnet 5 il
    // compromesso qualita'/costo, Haiku 4.5 quello rapido ed economico.
    return { "claude-opus-5", "claude-sonnet-5", "claude-haiku-4-5" };
}

// ═══════════════════════════════════════════════════════════════════
//  Ollama Provider
// ═══════════════════════════════════════════════════════════════════

AIProvider::Result OllamaProvider::sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage)
{
    Result result;
    juce::String fullPrompt = systemPrompt.isNotEmpty()
        ? systemPrompt + "\n\n" + userMessage
        : userMessage;

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["prompt"] = fullPrompt.toStdString();
    body["stream"] = false;
    body["options"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["options"]["num_predict"] = config.maxTokens;

    juce::String raw = makeHttp(config.baseUrl + "/api/generate", "POST",
                                 juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);
    if (j.contains("response")) {
        result.text = juce::String(j["response"].get<std::string>());
        result.success = true;
    } else if (j.contains("error")) {
        result.error = juce::String(j["error"].get<std::string>());
    }
    return result;
}

void OllamaProvider::sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    if (!onChunk) return;
    beginRequest();

    juce::String fullPrompt = systemPrompt.isNotEmpty()
        ? systemPrompt + "\n\n" + userMessage
        : userMessage;

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["prompt"] = fullPrompt.toStdString();
    body["stream"] = true;
    body["options"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["options"]["num_predict"] = config.maxTokens;

    auto apertura = openStreamingPost (config.baseUrl + "/api/generate", body.dump(),
                                       "Content-Type: application/json\r\n",
                                       config.timeoutMs, "Ollama");
    if (apertura.stream == nullptr) {
        lastError = apertura.error;
        lastStatusCode = apertura.statusCode;
        onChunk("", true);
        return;
    }
    auto stream = std::move (apertura.stream);

    bool ricevutoQualcosa = false;
    juce::String line;
    char ch;
    while (!stream->isExhausted() && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            auto j = parseJsonSafe(line);
            if (j.contains("error")) {
                lastError = "Ollama: " + extractError (line);
                break;
            }
            if (j.contains("response")) {
                ricevutoQualcosa = true;
                onChunk(juce::String(j["response"].get<std::string>()), false);
            }
            if (j.contains("done") && j["done"].is_boolean() && j["done"].get<bool>())
                break;
            line.clear();
        } else {
            line += ch;
        }
    }

    lastAborted = abortRequested.load();
    if (lastAborted)
        lastError = "Richiesta annullata.";
    else if (!ricevutoQualcosa && lastError.isEmpty())
        lastError = "Ollama non ha prodotto testo: il modello \"" + config.model
                  + "\" potrebbe non essere scaricato (ollama pull " + config.model + ").";

    onChunk("", true);
    abortRequested = false;
}

bool OllamaProvider::testConnection()
{
    juce::String raw = makeHttp(config.baseUrl + "/api/tags", "GET", "", 5000);
    if (raw.contains("models")) return true;

    lastError = "Ollama non risponde su " + config.baseUrl
              + ". Avvialo, oppure scegli un altro fornitore.";
    return false;
}

juce::StringArray OllamaProvider::getAvailableModels()
{
    juce::StringArray models;
    juce::String raw = makeHttp(config.baseUrl + "/api/tags", "GET", "", 5000);
    auto j = parseJsonSafe(raw);
    if (j.contains("models") && j["models"].is_array()) {
        for (const auto& m : j["models"]) {
            if (m.contains("name"))
                models.add(juce::String(m["name"].get<std::string>()));
        }
    }
    if (models.isEmpty()) models = {"llama3.2", "llama3.1", "mistral", "codellama", "phi3", "gemma2"};
    return models;
}

// ═══════════════════════════════════════════════════════════════════
//  Gemini Provider
// ═══════════════════════════════════════════════════════════════════

juce::String GeminiProvider::makeHttpRequest(const juce::String& url, const juce::String& method,
                                               const juce::String& jsonBody, int timeoutMs,
                                               const juce::String& extraHeaders)
{
    return makeHttp(url, method, jsonBody, timeoutMs, extraHeaders);
}

AIProvider::Result GeminiProvider::sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage)
{
    Result result;
    if (config.apiKey.isEmpty()) { result.error = "API key not configured"; return result; }

    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                       + config.model + ":generateContent?key=" + config.apiKey;

    nlohmann::json body;
    if (systemPrompt.isNotEmpty())
        body["system_instruction"]["parts"] = nlohmann::json::array({{{"text", systemPrompt.toStdString()}}});
    body["contents"] = nlohmann::json::array({{{"parts", nlohmann::json::array({{{"text", userMessage.toStdString()}}})}}});
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["generationConfig"]["maxOutputTokens"] = config.maxTokens;
    if (searchEnabled)
        body["tools"] = nlohmann::json::array({{{"googleSearch", nlohmann::json::object()}}});

    juce::String raw = makeHttpRequest(url, "POST", juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);
    try {
        result.text = juce::String(j.at("candidates").at(0).at("content").at("parts").at(0).at("text").get<std::string>());
        result.success = true;
    } catch (...) { result.error = extractError(raw); }
    return result;
}

void GeminiProvider::sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    if (!onChunk) return;
    beginRequest();

    if (config.apiKey.isEmpty()) {
        lastError = "Nessuna chiave configurata per Gemini.";
        onChunk("", true);
        return;
    }

    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                       + config.model + ":streamGenerateContent?alt=sse&key=" + config.apiKey;

    nlohmann::json body;
    if (systemPrompt.isNotEmpty())
        body["system_instruction"]["parts"] = nlohmann::json::array({{{"text", systemPrompt.toStdString()}}});
    body["contents"] = nlohmann::json::array({{{"parts", nlohmann::json::array({{{"text", userMessage.toStdString()}}})}}});
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["generationConfig"]["maxOutputTokens"] = config.maxTokens;

    auto apertura = openStreamingPost (url, body.dump(),
                                       "Content-Type: application/json\r\n",
                                       config.timeoutMs, "Gemini");
    if (apertura.stream == nullptr) {
        lastError = apertura.error;
        lastStatusCode = apertura.statusCode;
        onChunk("", true);
        return;
    }
    auto stream = std::move (apertura.stream);

    bool ricevutoQualcosa = false;
    juce::String line;
    char ch;
    while (!stream->isExhausted() && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            if (line.startsWith("data: ")) {
                auto j = parseJsonSafe(line.substring(6));
                if (j.contains ("error")) {
                    lastError = "Gemini: " + extractError (line.substring (6));
                    break;
                }
                try {
                    auto candidates = j["candidates"];
                    if (!candidates.is_null() && candidates.is_array() && !candidates.empty()) {
                        auto parts = candidates[0]["content"]["parts"];
                        if (!parts.is_null() && parts.is_array() && !parts.empty()) {
                            ricevutoQualcosa = true;
                            onChunk(juce::String(parts[0]["text"].get<std::string>()), false);
                        }
                    }
                } catch (...) {}
            }
            line.clear();
        } else {
            line += ch;
        }
    }

    lastAborted = abortRequested.load();
    if (lastAborted)
        lastError = "Richiesta annullata.";
    else if (!ricevutoQualcosa && lastError.isEmpty())
        lastError = "Gemini non ha prodotto testo: il modello \"" + config.model
                  + "\" potrebbe non essere piu' disponibile.";

    onChunk("", true);
    abortRequested = false;
}

bool GeminiProvider::testConnection()
{
    if (config.apiKey.isEmpty()) { lastError = "Nessuna chiave inserita."; return false; }
    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models?key=" + config.apiKey;
    juce::String raw = makeHttpRequest(url, "GET", "", 10000);
    if (raw.contains("models")) return true;

    lastError = raw.isEmpty() ? juce::String ("Nessuna risposta da Google.")
                              : extractError (raw);
    return false;
}

juce::StringArray GeminiProvider::getAvailableModels()
{
    // Chiede a Google quali modelli esistono davvero, invece di tenere una
    // lista scritta a mano. Le liste fisse invecchiano in silenzio: qui
    // c'erano gemini-1.5-flash e gemini-1.5-pro, ritirati, e chi li avesse
    // scelti avrebbe ricevuto un errore senza capirne il motivo.
    if (config.apiKey.isNotEmpty())
    {
        const juce::String url = "https://generativelanguage.googleapis.com/v1beta/models?key="
                               + config.apiKey;
        const juce::String raw = makeHttpRequest (url, "GET", "", 10000);
        const auto j = parseJsonSafe (raw);

        if (j.contains ("models") && j["models"].is_array())
        {
            juce::StringArray trovati;
            for (const auto& m : j["models"])
            {
                if (! m.contains ("name") || ! m["name"].is_string()) continue;

                // Tiene solo quelli che sanno generare testo: l'elenco
                // comprende anche modelli per gli embedding, che qui non
                // servono e confonderebbero soltanto.
                bool generaTesto = false;
                if (m.contains ("supportedGenerationMethods") && m["supportedGenerationMethods"].is_array())
                    for (const auto& metodo : m["supportedGenerationMethods"])
                        if (metodo.is_string() && metodo.get<std::string>() == "generateContent")
                            generaTesto = true;
                if (! generaTesto) continue;

                // I nomi arrivano come "models/gemini-...": si tiene la parte utile.
                juce::String nome (m["name"].get<std::string>());
                trovati.add (nome.fromLastOccurrenceOf ("/", false, true));
            }

            if (! trovati.isEmpty())
                return trovati;
        }
    }

    // Senza chiave non si puo' chiedere niente: si mostra un punto di
    // partenza ragionevole, che verra' sostituito appena la chiave c'e'.
    return { "gemini-2.0-flash" };
}
