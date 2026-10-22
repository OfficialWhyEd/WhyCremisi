#include "AIProvider.h"
#include <nlohmann/json.hpp>
#include <thread>

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
//  Accumulatori delle tool call in streaming
// ═══════════════════════════════════════════════════════════════════

static nlohmann::json parseArgumentsOrRaw (const juce::String& grezzi)
{
    if (grezzi.trim().isEmpty()) return nlohmann::json::object();
    auto parsed = parseJsonSafe (grezzi);
    if (! parsed.is_null()) return parsed;
    // JSON incompleto o invalido: non si indovina e non si butta via.
    // Chi esegue legge qui e riporta l'errore, invece di mandare al modello
    // argomenti vuoti facendogli credere che sia andata bene.
    return nlohmann::json{{"__raw_arguments", grezzi.toStdString()}};
}

void ToolCallAccumulator::clear()
{
    calls.clear();
    argumentiGrezzi.clear();
    ordine.clear();
}

void ToolCallAccumulator::addChunk (int index, const juce::String& id, const juce::String& name,
                                    const juce::String& argumentsFragment)
{
    if (calls.count (index) == 0)
    {
        ordine.push_back (index);
        calls[index] = AIProvider::ToolCallResult{};
    }

    auto& c = calls[index];
    if (id.isNotEmpty())   c.id = id;
    // llama.cpp puo' mandare il nome che cresce (prima un prefisso, poi il
    // nome intero): vale l'ultimo, non la concatenazione.
    if (name.isNotEmpty()) c.name = name;
    if (argumentsFragment.isNotEmpty()) argumentiGrezzi[index] += argumentsFragment;
}

std::vector<AIProvider::ToolCallResult> ToolCallAccumulator::finish() const
{
    std::vector<AIProvider::ToolCallResult> out;
    for (int idx : ordine)
    {
        auto it = calls.find (idx);
        if (it == calls.end()) continue;

        auto c = it->second;
        const juce::String grezzi = argumentiGrezzi.count (idx) ? argumentiGrezzi.at (idx) : juce::String();
        c.arguments = parseArgumentsOrRaw (grezzi);
        out.push_back (std::move (c));
    }
    return out;
}

void AnthropicToolAccumulator::clear()
{
    calls.clear();
    buffer.clear();
    ordine.clear();
}

void AnthropicToolAccumulator::startBlock (int index, const juce::String& id, const juce::String& name)
{
    if (calls.count (index) == 0) ordine.push_back (index);
    auto& c = calls[index];
    c.id = id;
    c.name = name;
}

void AnthropicToolAccumulator::addJsonFragment (int index, const juce::String& fragment)
{
    if (calls.count (index) == 0) return;   // non e' un blocco tool_use
    buffer[index] += fragment;
}

void AnthropicToolAccumulator::stopBlock (int index)
{
    // Gli argomenti si interpretano solo qui: durante lo stream il JSON e'
    // per definizione incompleto.
    (void) index;
}

std::vector<AIProvider::ToolCallResult> AnthropicToolAccumulator::finish() const
{
    std::vector<AIProvider::ToolCallResult> out;
    for (int idx : ordine)
    {
        auto it = calls.find (idx);
        if (it == calls.end()) continue;

        auto c = it->second;
        const juce::String grezzi = buffer.count (idx) ? buffer.at (idx) : juce::String();
        c.arguments = parseArgumentsOrRaw (grezzi);
        out.push_back (std::move (c));
    }
    return out;
}

void GeminiToolAccumulator::clear()
{
    calls.clear();
    buffer.clear();
}

void GeminiToolAccumulator::addCall (const juce::String& name, const nlohmann::json& args)
{
    AIProvider::ToolCallResult c;
    c.name = name;

    if (args.is_string())
    {
        // Argomenti a pezzi: si accumulano e si interpretano alla fine.
        buffer[name] += juce::String (args.get<std::string>());
        c.arguments = nlohmann::json::object();
    }
    else if (args.is_object())
    {
        buffer.erase (name);
        c.arguments = args;
    }
    else
    {
        c.arguments = nlohmann::json::object();
    }

    calls.push_back (std::move (c));
}

void GeminiToolAccumulator::addArgumentsFragment (const juce::String& name, const juce::String& fragment)
{
    buffer[name] += fragment;
}

std::vector<AIProvider::ToolCallResult> GeminiToolAccumulator::finish() const
{
    std::vector<AIProvider::ToolCallResult> out;
    for (const auto& c : calls)
    {
        auto copia = c;
        auto it = buffer.find (c.name);
        if (it != buffer.end() && it->second.isNotEmpty())
            copia.arguments = parseArgumentsOrRaw (it->second);
        out.push_back (std::move (copia));
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════
//  OpenAI Provider (also base for OpenRouter, Groq, llama.cpp)
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

// Un messaggio senza "content" fa fallire il server prima ancora del
// modello: la chat template di llama.cpp risponde 400 con "All non-assistant
// messages must contain 'content'", e lo stesso vale per gli altri servizi
// OpenAI-compatibili. Qui si mette in forma quello che parte dal plugin.
static nlohmann::json sanitizeOpenAIMessages (nlohmann::json msgs)
{
    nlohmann::json out = nlohmann::json::array();
    for (auto& m : msgs)
    {
        if (! m.is_object()) continue;

        const auto role = m.value ("role", std::string ("user"));
        if (! m.contains ("content") || m["content"].is_null())
            m["content"] = "";

        const bool vuoto = m["content"].is_string()
                        && m["content"].get<std::string>().empty();
        const bool assistant = (role == "assistant");

        // Un assistant vuoto e' legittimo (puo' portare solo tool_calls);
        // un utente/system/tool vuoto no: non c'e' niente da leggere.
        if (vuoto && ! assistant && ! m.contains ("tool_calls")) continue;

        out.push_back (m);
    }
    return out;
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
            return sanitizeOpenAIMessages (j);
        }
    }
    nlohmann::json messages = nlohmann::json::array();
    if (systemPrompt.isNotEmpty())
        messages.push_back({{"role", "system"}, {"content", systemPrompt.toStdString()}});
    messages.push_back({{"role", "user"}, {"content", userMessage.toStdString()}});
    return sanitizeOpenAIMessages (messages);
}

AIProvider::Result OpenAIProvider::sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage)
{
    Result result;
    if (requiresApiKey() && config.apiKey.isEmpty()) { result.error = "API key not configured"; return result; }

    nlohmann::json body;
    // Senza modello il server usa quello che ha già caricato: e' il caso di
    // llama.cpp, dove il nome del modello e' un dettaglio del server, non
    // una scelta da fare nel plugin.
    if (config.model.isNotEmpty())
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

    juce::String authHeader;
    if (config.apiKey.isNotEmpty())
        authHeader << "Authorization: Bearer " << config.apiKey << "\r\n";
    authHeader << getExtraHeaders();
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
    clearStreamToolCalls();
    ToolCallAccumulator accumulatore;

    if (requiresApiKey() && config.apiKey.isEmpty()) {
        lastError = "Nessuna chiave configurata per " + getName() + ".";
        onChunk("", true);
        return;
    }

    nlohmann::json body;
    if (config.model.isNotEmpty())
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

    juce::String authHeader = "Content-Type: application/json\r\n";
    if (config.apiKey.isNotEmpty())
        authHeader << "Authorization: Bearer " << config.apiKey << "\r\n";
    authHeader << getExtraHeaders();

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
                    // Tool call native: sono dati, non testo. Si accumulano
                    // per index (gli argomenti arrivano a frammenti) e si
                    // consegnano a fine stream. Il marcatore che si vede in
                    // chat lo costruisce il bridge, non il provider.
                    if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
                        for (auto& tc : delta["tool_calls"]) {
                            const int idx = (tc.contains("index") && tc["index"].is_number_integer())
                                          ? tc["index"].get<int>() : 0;
                            juce::String id;
                            if (tc.contains("id") && tc["id"].is_string())
                                id = juce::String(tc["id"].get<std::string>());
                            juce::String nome, argomenti;
                            if (tc.contains("function") && tc["function"].is_object()) {
                                if (tc["function"].contains("name") && tc["function"]["name"].is_string())
                                    nome = juce::String(tc["function"]["name"].get<std::string>());
                                if (tc["function"].contains("arguments")) {
                                    const auto& a = tc["function"]["arguments"];
                                    if (a.is_string())      argomenti = juce::String(a.get<std::string>());
                                    else if (!a.is_null())  argomenti = juce::String(a.dump());
                                }
                            }
                            accumulatore.addChunk(idx, id, nome, argomenti);
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

    // Le chiamate complete esistono solo ora che lo stream e' finito: il
    // bridge le legge nel callback di fine, quindi vanno pubblicate prima.
    streamToolCalls = accumulatore.finish();

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
    if (requiresApiKey() && config.apiKey.isEmpty()) { lastError = "Nessuna chiave inserita."; return false; }

    auto url = getApiUrl().upToLastOccurrenceOf ("/", false, false);   // .../v1/chat
    url = url.upToLastOccurrenceOf ("/", false, false) + "/models";    // .../v1/models

    juce::String auth;
    if (config.apiKey.isNotEmpty())
        auth << "Authorization: Bearer " << config.apiKey << "\r\n";
    auth << getExtraHeaders();

    juce::String raw = makeHttpRequest (url, "GET", {}, 10000, auth);
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
//  llama.cpp Provider (llama-server in locale, API di OpenAI)
// ═══════════════════════════════════════════════════════════════════

// Indirizzo normalizzato: toglie lo slash finale, e accetta sia la radice
// (http://127.0.0.1:8080) sia un indirizzo gia' completo incollato per sbaglio.
static juce::String llamaBase (const juce::String& configurato)
{
    auto base = configurato.isNotEmpty() ? configurato.trim() : LlamaCppProvider::defaultBaseUrl();
    while (base.endsWithChar ('/')) base = base.dropLastCharacters (1);
    if (base.endsWith ("/v1/chat/completions")) base = base.dropLastCharacters (19);
    else if (base.endsWith ("/v1/models"))      base = base.dropLastCharacters (10);
    if (base.endsWith ("/v1"))                  base = base.dropLastCharacters (3);
    return base;
}

juce::String LlamaCppProvider::getApiUrl() const
{
    return llamaBase (config.baseUrl) + "/v1/chat/completions";
}

bool LlamaCppProvider::testConnection()
{
    const auto base = llamaBase (config.baseUrl);

    juce::String raw = makeHttpRequest (base + "/v1/models", "GET", {}, 5000, {});
    if (raw.isEmpty())
    {
        lastError = "llama-server non risponde su " + base
                  + ". Avvialo (llama-server -m modello.gguf --port "
                  + LlamaCppProvider::defaultBaseUrl().fromLastOccurrenceOf (":", false, false)
                  + ") e riprova.";
        return false;
    }

    auto j = parseJsonSafe (raw);
    if (j.contains ("data") && j["data"].is_array()) return true;

    lastError = extractError (raw);
    if (lastError.isEmpty()) lastError = "Risposta non riconosciuta da " + base + "/v1/models";
    return false;
}

juce::StringArray LlamaCppProvider::getAvailableModels()
{
    // I modelli sono i file .gguf che llama-server ha caricato: l'id lo
    // decide lui, quindi l'elenco va chiesto, non indovinato.
    juce::StringArray models;

    const auto base = llamaBase (config.baseUrl);
    juce::String raw = makeHttpRequest (base + "/v1/models", "GET", {}, 5000, {});
    auto j = parseJsonSafe (raw);
    if (j.contains ("data") && j["data"].is_array())
        for (const auto& m : j["data"])
            if (m.contains ("id") && m["id"].is_string())
                models.add (juce::String (m["id"].get<std::string>()));

    return models;
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
    // Solo il meccanismo ufficiale: ANTHROPIC_AUTH_TOKEN e' la variabile che
    // l'SDK e la CLI Anthropic leggono, messa dall'utente stesso. Non
    // frughiamo nei file di credenziali di altri programmi e non
    // conserviamo token: per entrare con l'account Claude c'e'
    // ClaudeCodeProvider, che lascia il login al programma ufficiale.
    return juce::SystemStats::getEnvironmentVariable ("ANTHROPIC_AUTH_TOKEN", {}).trim();
}

// ═══════════════════════════════════════════════════════════════════
//  Claude Code Provider (account Claude, login fatto dal programma ufficiale)
// ═══════════════════════════════════════════════════════════════════

juce::File ClaudeCodeProvider::findExecutable()
{
    // Cerchiamo il programma, non i suoi segreti.
   #if JUCE_WINDOWS
    const char* nomi[] = { "claude.exe", "claude.cmd" };
   #else
    const char* nomi[] = { "claude" };
   #endif

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

    // Installazione con npm: il programma vero sta dentro il pacchetto.
    if (auto f = home.getChildFile ("AppData").getChildFile ("Roaming").getChildFile ("npm")
                     .getChildFile ("node_modules").getChildFile ("@anthropic-ai")
                     .getChildFile ("claude-code").getChildFile ("bin").getChildFile ("claude.exe");
        f.existsAsFile())
        return f;

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

// Un argomento nel comando di shell: tra virgolette, senza virgolette dentro.
static juce::String shellArg (const juce::String& s)
{
    return "\"" + s.replace ("\"", "") + "\"";
}

// Modello e livello di effort finiscono in una riga di shell: ammessi solo
// i caratteri che servono davvero.
static juce::String soloSicuri (const juce::String& s)
{
    return s.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-");
}

juce::String ClaudeCodeProvider::buildShellCommand (const juce::File& exe,
                                                    const juce::StringArray& args,
                                                    const juce::File& promptFile,
                                                    const juce::File& workDir)
{
    juce::String cmd = shellArg (exe.getFullPathName());
    for (const auto& a : args)
        cmd += " " + shellArg (a);

    if (promptFile != juce::File())
        cmd += " < " + shellArg (promptFile.getFullPathName());

    if (workDir != juce::File())
       #if JUCE_WINDOWS
        cmd = "cd /d " + shellArg (workDir.getFullPathName()) + " && " + cmd;
       #else
        cmd = "cd " + shellArg (workDir.getFullPathName()) + " && " + cmd;
       #endif

   #if JUCE_WINDOWS
    // Con /s cmd toglie solo le virgolette esterne.
    return "cmd.exe /d /s /c \"" + cmd + "\"";
   #else
    return "/bin/sh -c " + shellArg (cmd);
   #endif
}

ClaudeCodeProvider::LoginState ClaudeCodeProvider::queryLoginState()
{
    LoginState st;
    const auto exe = findExecutable();
    if (! exe.existsAsFile())
        return st;
    st.installed = true;

    juce::ChildProcess proc;
    const auto cmd = buildShellCommand (exe, { "auth", "status", "--json" }, {}, {});
    if (! proc.start (cmd, juce::ChildProcess::wantStdOut))
        return st;

    const auto out = proc.readAllProcessOutput();
    proc.waitForProcessToFinish (15000);

    const auto j = nlohmann::json::parse (out.toStdString(), nullptr, false);
    if (j.is_object())
    {
        st.loggedIn = j.value ("loggedIn", false);
        st.method = juce::String (j.value ("authMethod", std::string()));
    }
    return st;
}

bool ClaudeCodeProvider::openLoginTerminal()
{
    const auto exe = findExecutable();
    if (! exe.existsAsFile())
        return false;

    const auto cmd = shellArg (exe.getFullPathName()) + " auth login";

    juce::ChildProcess p;
   #if JUCE_WINDOWS
    return p.start ("cmd.exe /d /c start \"Accedi a Claude\" cmd.exe /k \"" + cmd + "\"");
   #elif JUCE_MAC
    return p.start (juce::StringArray { "osascript", "-e",
        "tell application \"Terminal\" to do script \"" + cmd.replace ("\"", "\\\"") + "\"",
        "-e", "tell application \"Terminal\" to activate" });
   #else
    return p.start (juce::StringArray { "x-terminal-emulator", "-e", "sh", "-c", cmd + "; exec sh" });
   #endif
}

juce::StringArray ClaudeCodeProvider::buildArguments (const juce::String& model,
                                                      const juce::String& effort,
                                                      const juce::String& systemPromptFile)
{
    juce::StringArray a;
    a.add ("-p");
    a.add ("--output-format"); a.add ("json");
    // Solo testo: niente strumenti, niente sessione salvata, niente
    // impostazioni o comandi dell'utente. Le azioni sulla DAW passano dalle
    // azioni JSON che il system prompt chiede gia'.
    a.add ("--tools"); a.add ("");
    a.add ("--no-session-persistence");
    a.add ("--disable-slash-commands");
    a.add ("--setting-sources"); a.add ("");
    if (soloSicuri (model).isNotEmpty())
    {
        a.add ("--model"); a.add (soloSicuri (model));
    }
    if (soloSicuri (effort).isNotEmpty())
    {
        a.add ("--effort"); a.add (soloSicuri (effort));
    }
    if (systemPromptFile.isNotEmpty())
    {
        a.add ("--system-prompt-file"); a.add (systemPromptFile);
    }
    return a;
}

juce::String ClaudeCodeProvider::composePrompt (const juce::String& contextMessagesJson,
                                                const juce::String& userMessage)
{
    juce::String out;
    if (contextMessagesJson.isNotEmpty())
    {
        const auto prior = nlohmann::json::parse (contextMessagesJson.toStdString(), nullptr, false);
        if (prior.is_array())
            for (const auto& m : prior)
            {
                if (! m.is_object() || ! m.contains ("content") || ! m["content"].is_string())
                    continue;
                const auto role = juce::String (m.value ("role", std::string()));
                const auto content = juce::String (m["content"].get<std::string>());
                if (content.isEmpty()) continue;
                if (role == "user")           out += "Utente: " + content + "\n\n";
                else if (role == "assistant") out += "Assistente: " + content + "\n\n";
            }
    }

    // Prompt troppo lungo: si tiene la parte piu' recente.
    constexpr int massimo = 60000;
    if (out.length() > massimo)
        out = out.substring (out.length() - massimo);

    return out + "Utente: " + userMessage + "\n";
}

AIProvider::Result ClaudeCodeProvider::parseCliOutput (const juce::String& raw)
{
    Result r;
    const auto j = nlohmann::json::parse (raw.toStdString(), nullptr, false);
    if (! j.is_object())
    {
        r.error = "Risposta di Claude non leggibile.";
        return r;
    }

    const auto testo = juce::String (j.value ("result", std::string()));
    if (j.value ("is_error", false))
    {
        r.error = testo.isNotEmpty() ? testo : "Claude ha restituito un errore.";
        return r;
    }

    r.success = true;
    r.text = testo;
    r.stopReason = juce::String (j.value ("stop_reason", std::string()));

    if (j.contains ("usage") && j["usage"].is_object())
    {
        const auto& u = j["usage"];
        r.usage.inputTokens      = u.value ("input_tokens", 0);
        r.usage.outputTokens     = u.value ("output_tokens", 0);
        r.usage.cacheReadTokens  = u.value ("cache_read_input_tokens", 0);
        r.usage.cacheWriteTokens = u.value ("cache_creation_input_tokens", 0);
    }
    if (j.contains ("modelUsage") && j["modelUsage"].is_object() && ! j["modelUsage"].empty())
        r.modelUsed = juce::String (j["modelUsage"].begin().key());

    return r;
}

void ClaudeCodeProvider::abort()
{
    abortRequested = true;
    std::lock_guard<std::mutex> lock (procMutex);
    if (running != nullptr)
        running->kill();
}

AIProvider::Result ClaudeCodeProvider::sendPrompt (const juce::String& systemPrompt, const juce::String& userMessage)
{
    beginRequest();
    Result r;

    const auto exe = findExecutable();
    if (! exe.existsAsFile())
    {
        r.error = "Claude non risulta installato su questa macchina. Installa Claude Code e riprova.";
        lastError = r.error;
        return r;
    }

    // File di lavoro in una cartella temporanea vuota: cosi' non si
    // caricano CLAUDE.md o memorie di altri progetti.
    const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WhyCremisi-claude");
    base.createDirectory();
    const auto id = juce::Uuid().toString();
    const auto sysFile = base.getChildFile ("sys-" + id + ".txt");
    const auto promptFile = base.getChildFile ("prompt-" + id + ".txt");
    sysFile.replaceWithText (systemPrompt);
    promptFile.replaceWithText (composePrompt (config.contextMessages, userMessage));

    const auto model = config.model.isNotEmpty() ? config.model : defaultModel();
    const auto cmd = buildShellCommand (exe, buildArguments (model, config.effort, sysFile.getFullPathName()),
                                        promptFile, base);

    juce::ChildProcess proc;
    if (! proc.start (cmd, juce::ChildProcess::wantStdOut))
    {
        r.error = "Non sono riuscito ad avviare Claude.";
        lastError = r.error;
        sysFile.deleteFile(); promptFile.deleteFile();
        return r;
    }

    {
        std::lock_guard<std::mutex> lock (procMutex);
        running = &proc;
    }

    // Se qualcosa si pianta il processo va fermato: ci pensa questo guardiano.
    const int limite = juce::jmax (config.timeoutMs, 180000);
    std::atomic<bool> finito { false };
    std::thread guardiano ([&]
    {
        for (int ms = 0; ms < limite && ! finito; ms += 100)
            juce::Thread::sleep (100);
        if (! finito)
        {
            std::lock_guard<std::mutex> lock (procMutex);
            if (running != nullptr) running->kill();
        }
    });

    const auto out = proc.readAllProcessOutput();
    proc.waitForProcessToFinish (5000);
    finito = true;
    guardiano.join();

    {
        std::lock_guard<std::mutex> lock (procMutex);
        running = nullptr;
    }
    sysFile.deleteFile();
    promptFile.deleteFile();

    if (abortRequested)
    {
        lastAborted = true;
        r.error = "Interrotto.";
        return r;
    }

    r = parseCliOutput (out);
    if (! r.success)
    {
        if (r.error.isEmpty() || out.trim().isEmpty())
            r.error = "Claude non ha risposto. Se non hai ancora fatto l'accesso, usa \"Accedi con Claude\".";
        lastError = r.error;
    }
    return r;
}

void ClaudeCodeProvider::sendPromptStreaming (const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    // Il programma ufficiale restituisce la risposta intera: la si consegna
    // in un colpo solo.
    const auto r = sendPrompt (systemPrompt, userMessage);
    if (onChunk)
    {
        if (r.success && r.text.isNotEmpty())
            onChunk (r.text, false);
        onChunk ({}, true);
    }
}

bool ClaudeCodeProvider::testConnection()
{
    beginRequest();
    const auto st = queryLoginState();
    if (! st.installed)
    {
        lastError = "Claude non risulta installato su questa macchina.";
        return false;
    }
    if (! st.loggedIn)
    {
        lastError = "Accesso a Claude non ancora fatto.";
        return false;
    }
    return true;
}

juce::StringArray ClaudeCodeProvider::knownModels()
{
    return { "claude-sonnet-5-5", "claude-opus-5-5", "claude-haiku-5-5", "claude-fable-5-1" };
}

juce::StringArray ClaudeCodeProvider::getAvailableModels()
{
    return knownModels();
}

bool ClaudeCodeProvider::isKnownModel (const juce::String& m)
{
    return knownModels().contains (m) || juce::StringArray { "sonnet", "opus", "haiku" }.contains (m);
}


// ═══════════════════════════════════════════════════════════════════
//  Codex Provider (account ChatGPT, login fatto dal programma ufficiale)
// ═══════════════════════════════════════════════════════════════════

juce::File CodexProvider::findExecutable()
{
   #if JUCE_WINDOWS
    const char* nomi[] = { "codex.exe", "codex.cmd" };
   #else
    const char* nomi[] = { "codex" };
   #endif

    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    juce::Array<juce::File> cartelle {
        home.getChildFile ("AppData").getChildFile ("Local").getChildFile ("Programs")
            .getChildFile ("OpenAI").getChildFile ("Codex").getChildFile ("bin"),
        home.getChildFile ("AppData").getChildFile ("Roaming").getChildFile ("npm"),
        home.getChildFile (".local").getChildFile ("bin")
    };
    for (const auto& dir : cartelle)
        for (auto* n : nomi)
            if (auto f = dir.getChildFile (n); f.existsAsFile())
                return f;

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

CodexProvider::LoginState CodexProvider::queryLoginState()
{
    LoginState st;
    const auto exe = findExecutable();
    if (! exe.existsAsFile())
        return st;
    st.installed = true;

    juce::ChildProcess proc;
    const auto cmd = ClaudeCodeProvider::buildShellCommand (exe, { "login", "status" }, {}, {});
    // A seconda della versione l'esito esce su stdout o su stderr.
    if (! proc.start (cmd, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return st;
    const auto out = proc.readAllProcessOutput();
    proc.waitForProcessToFinish (15000);

    if (out.containsIgnoreCase ("not logged in"))
        return st;
    if (out.containsIgnoreCase ("logged in using chatgpt"))
    {
        st.loggedIn = true;
        st.method = "ChatGPT";
    }
    else if (out.containsIgnoreCase ("logged in"))
    {
        st.loggedIn = true;
        st.method = "API key";
    }
    return st;
}

bool CodexProvider::openLoginTerminal()
{
    const auto exe = findExecutable();
    if (! exe.existsAsFile())
        return false;

    const auto cmd = shellArg (exe.getFullPathName()) + " login";

    juce::ChildProcess p;
   #if JUCE_WINDOWS
    return p.start ("cmd.exe /d /c start \"Accedi a ChatGPT\" cmd.exe /k \"" + cmd + "\"");
   #elif JUCE_MAC
    return p.start (juce::StringArray { "osascript", "-e",
        "tell application \"Terminal\" to do script \"" + cmd.replace ("\"", "\\\"") + "\"",
        "-e", "tell application \"Terminal\" to activate" });
   #else
    return p.start (juce::StringArray { "x-terminal-emulator", "-e", "sh", "-c", cmd + "; exec sh" });
   #endif
}

juce::StringArray CodexProvider::buildArguments (const juce::String& model, const juce::String& effort)
{
    juce::StringArray a;
    a.add ("exec");
    a.add ("--json");
    a.add ("--skip-git-repo-check");
    a.add ("--ephemeral");
    a.add ("--ignore-user-config");
    a.add ("--ignore-rules");
    a.add ("-s"); a.add ("read-only");
    // "default" = il modello predefinito dell'abbonamento: nessun -m.
    if (soloSicuri (model).isNotEmpty() && model != "default")
    {
        a.add ("-m"); a.add (soloSicuri (model));
    }
    const auto e = soloSicuri (effort);
    if (e == "low" || e == "medium" || e == "high" || e == "xhigh")
    {
        a.add ("-c"); a.add ("model_reasoning_effort=" + e);
    }
    a.add ("-");   // il prompt arriva da stdin
    return a;
}

AIProvider::Result CodexProvider::parseCliOutput (const juce::String& raw)
{
    Result r;
    juce::String testo;
    juce::String errore;
    bool completato = false;

    for (const auto& riga : juce::StringArray::fromLines (raw))
    {
        const auto t = riga.trim();
        if (! t.startsWithChar ('{')) continue;
        const auto j = nlohmann::json::parse (t.toStdString(), nullptr, false);
        if (! j.is_object()) continue;
        const auto tipo = j.value ("type", std::string());

        if (tipo == "item.completed" && j.contains ("item") && j["item"].is_object()
            && j["item"].value ("type", std::string()) == "agent_message")
        {
            const auto msg = juce::String (j["item"].value ("text", std::string()));
            if (testo.isNotEmpty()) testo += "\n\n";
            testo += msg;
        }
        else if (tipo == "turn.completed")
        {
            completato = true;
            if (j.contains ("usage") && j["usage"].is_object())
            {
                const auto& u = j["usage"];
                r.usage.inputTokens     = u.value ("input_tokens", 0);
                r.usage.outputTokens    = u.value ("output_tokens", 0);
                r.usage.cacheReadTokens = u.value ("cached_input_tokens", 0);
            }
        }
        else if (tipo == "turn.failed" || tipo == "error")
        {
            auto m = juce::String (j.value ("message", std::string()));
            if (m.isEmpty() && j.contains ("error") && j["error"].is_object())
                m = juce::String (j["error"].value ("message", std::string()));
            if (m.isNotEmpty()) errore = m;
        }
    }

    if (completato && testo.isNotEmpty())
    {
        r.success = true;
        r.text = testo;
        return r;
    }
    r.error = errore.isNotEmpty() ? errore : "Risposta di ChatGPT non leggibile.";
    return r;
}

void CodexProvider::abort()
{
    abortRequested = true;
    std::lock_guard<std::mutex> lock (procMutex);
    if (running != nullptr)
        running->kill();
}

AIProvider::Result CodexProvider::sendPrompt (const juce::String& systemPrompt, const juce::String& userMessage)
{
    beginRequest();
    Result r;

    const auto exe = findExecutable();
    if (! exe.existsAsFile())
    {
        r.error = "ChatGPT (Codex) non risulta installato su questa macchina. Installa Codex e riprova.";
        lastError = r.error;
        return r;
    }

    const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WhyCremisi-codex");
    base.createDirectory();
    const auto promptFile = base.getChildFile ("prompt-" + juce::Uuid().toString() + ".txt");
    // Codex non ha un parametro per il system prompt: va in testa al testo.
    promptFile.replaceWithText ("ISTRUZIONI DI SISTEMA (seguile alla lettera, rispondi solo a testo, "
                                "non usare strumenti ne' comandi):\n" + systemPrompt
                                + "\n\n--- CONVERSAZIONE ---\n"
                                + ClaudeCodeProvider::composePrompt (config.contextMessages, userMessage));

    const auto cmd = ClaudeCodeProvider::buildShellCommand (exe, buildArguments (config.model, config.effort),
                                                            promptFile, base);

    juce::ChildProcess proc;
    if (! proc.start (cmd, juce::ChildProcess::wantStdOut))
    {
        r.error = "Non sono riuscito ad avviare Codex.";
        lastError = r.error;
        promptFile.deleteFile();
        return r;
    }
    {
        std::lock_guard<std::mutex> lock (procMutex);
        running = &proc;
    }

    const int limite = juce::jmax (config.timeoutMs, 180000);
    std::atomic<bool> finito { false };
    std::thread guardiano ([&]
    {
        for (int ms = 0; ms < limite && ! finito; ms += 100)
            juce::Thread::sleep (100);
        if (! finito)
        {
            std::lock_guard<std::mutex> lock (procMutex);
            if (running != nullptr) running->kill();
        }
    });

    const auto out = proc.readAllProcessOutput();
    proc.waitForProcessToFinish (5000);
    finito = true;
    guardiano.join();
    {
        std::lock_guard<std::mutex> lock (procMutex);
        running = nullptr;
    }
    promptFile.deleteFile();

    if (abortRequested)
    {
        lastAborted = true;
        r.error = "Interrotto.";
        return r;
    }

    r = parseCliOutput (out);
    if (! r.success)
    {
        if (out.trim().isEmpty())
            r.error = "ChatGPT non ha risposto. Se non hai ancora fatto l'accesso, usa \"Accedi con ChatGPT\".";
        lastError = r.error;
    }
    return r;
}

void CodexProvider::sendPromptStreaming (const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    const auto r = sendPrompt (systemPrompt, userMessage);
    if (onChunk)
    {
        if (r.success && r.text.isNotEmpty())
            onChunk (r.text, false);
        onChunk ({}, true);
    }
}

bool CodexProvider::testConnection()
{
    beginRequest();
    const auto st = queryLoginState();
    if (! st.installed)
    {
        lastError = "ChatGPT (Codex) non risulta installato su questa macchina.";
        return false;
    }
    if (! st.loggedIn)
    {
        lastError = "Accesso a ChatGPT non ancora fatto.";
        return false;
    }
    return true;
}

juce::StringArray CodexProvider::getAvailableModels()
{
    // I modelli dipendono dall'abbonamento: "default" e' sempre valido. Gli altri
    // si leggono dall'elenco che Codex stesso tiene aggiornato per l'account.
    juce::StringArray out { "default" };
    const auto cache = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                           .getChildFile (".codex").getChildFile ("models_cache.json");
    const auto j = juce::JSON::parse (cache);
    if (auto* arr = j["models"].getArray())
        for (auto& m : *arr)
            if (m["visibility"].toString() == "list" && m["slug"].toString().isNotEmpty())
                out.addIfNotAlreadyThere (m["slug"].toString());
    return out;
}

juce::String AnthropicProvider::displayNameFor (const juce::String& modelId)
{
    if (modelId == "claude-opus-5-5")    return "Opus 5.5";
    if (modelId == "claude-sonnet-5-5")  return "Sonnet 5.5";
    if (modelId == "claude-haiku-5-5") return "Haiku 5.5";
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

// Lo storico interno e' in forma neutra (quella di OpenAI). Claude vuole
// un'altra forma: assistant con tool_calls diventa blocchi tool_use, e i
// risultati diventano blocchi tool_result DENTRO un messaggio utente.
// Copiare il JSON com'era produceva messaggi invalidi.
nlohmann::json anthropicMessagesFromNeutral (const juce::String& contextJson)
{
    nlohmann::json out = nlohmann::json::array();
    auto prior = parseJsonSafe (contextJson);
    if (! prior.is_array()) return out;

    auto appendToolResult = [&out] (nlohmann::json block)
    {
        if (! out.empty() && out.back().value ("role", std::string()) == "user"
            && out.back()["content"].is_array())
        {
            out.back()["content"].push_back (block);
            return;
        }
        out.push_back ({ { "role", "user" }, { "content", nlohmann::json::array ({ block }) } });
    };

    for (auto& m : prior)
    {
        if (! m.is_object()) continue;
        const juce::String role = juce::String (m.value ("role", std::string ("user")));

        if (role == "system") continue;

        if (role == "tool")
        {
            const juce::String id   = juce::String (m.value ("tool_call_id", std::string()));
            const juce::String text = juce::String (m.value ("content", std::string()));
            if (text.isEmpty()) continue;

            nlohmann::json block;
            block["type"] = "tool_result";
            block["tool_use_id"] = id.toStdString();
            block["content"] = text.toStdString();
            if (text.startsWith ("ERRORE")) block["is_error"] = true;
            appendToolResult (block);
            continue;
        }

        if (role == "assistant" && m.contains ("tool_calls") && m["tool_calls"].is_array())
        {
            nlohmann::json blocks = nlohmann::json::array();
            if (m.contains ("content") && m["content"].is_string()
                && ! m["content"].get<std::string>().empty())
            {
                nlohmann::json t;
                t["type"] = "text";
                t["text"] = m["content"];
                blocks.push_back (t);
            }

            for (auto& tc : m["tool_calls"])
            {
                if (! tc.is_object() || ! tc.contains ("function")) continue;
                const auto& f = tc["function"];

                nlohmann::json input = nlohmann::json::object();
                if (f.contains ("arguments"))
                {
                    if (f["arguments"].is_object()) input = f["arguments"];
                    else if (f["arguments"].is_string())
                    {
                        auto p = parseJsonSafe (juce::String (f["arguments"].get<std::string>()));
                        if (p.is_object()) input = p;
                    }
                }

                nlohmann::json b;
                b["type"] = "tool_use";
                b["id"] = (tc.contains ("id") && tc["id"].is_string())
                         ? tc["id"] : nlohmann::json (std::string());
                b["name"] = f.value ("name", std::string());
                b["input"] = input;
                blocks.push_back (b);
            }

            nlohmann::json msg;
            msg["role"] = "assistant";
            msg["content"] = blocks;
            out.push_back (msg);
            continue;
        }

        if (m.contains ("content") && m["content"].is_string()
            && m["content"].get<std::string>().empty())
            continue;

        out.push_back (m);
    }

    return out;
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
    nlohmann::json messages = anthropicMessagesFromNeutral (config.contextMessages);
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
    clearStreamToolCalls();
    AnthropicToolAccumulator acc;

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
                    if (type == "content_block_start" && j.contains("content_block")
                        && j["content_block"].contains("type") && j["content_block"]["type"].is_string()
                        && j["content_block"]["type"].get<std::string>() == "tool_use") {
                        const int idx = (j.contains("index") && j["index"].is_number_integer()) ? j["index"].get<int>() : 0;
                        const auto& cb = j["content_block"];
                        const juce::String id = (cb.contains("id") && cb["id"].is_string())
                                              ? juce::String(cb["id"].get<std::string>()) : juce::String();
                        const juce::String nome = (cb.contains("name") && cb["name"].is_string())
                                                ? juce::String(cb["name"].get<std::string>()) : juce::String();
                        // L'input qui e' sempre {}: gli argomenti veri arrivano dopo.
                        acc.startBlock(idx, id, nome);
                    }
                    // Gli argomenti di un tool_use arrivano a frammenti, in
                    // eventi input_json_delta, e si interpretano solo alla fine.
                    if (type == "content_block_delta" && j.contains("delta")
                        && j["delta"].contains("type") && j["delta"]["type"].is_string()
                        && j["delta"]["type"].get<std::string>() == "input_json_delta") {
                        const int idx = (j.contains("index") && j["index"].is_number_integer()) ? j["index"].get<int>() : 0;
                        if (j["delta"].contains("partial_json") && j["delta"]["partial_json"].is_string())
                            acc.addJsonFragment(idx, juce::String(j["delta"]["partial_json"].get<std::string>()));
                    }
                    if (type == "content_block_stop") {
                        const int idx = (j.contains("index") && j["index"].is_number_integer()) ? j["index"].get<int>() : 0;
                        acc.stopBlock(idx);
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

    streamToolCalls = acc.finish();

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
    ping["model"] = "claude-haiku-5-5";
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
    return { "claude-opus-5-5", "claude-sonnet-5-5", "claude-haiku-5-5" };
}

// ═══════════════════════════════════════════════════════════════════
//  Ollama Provider (endpoint compatibile OpenAI)
// ═══════════════════════════════════════════════════════════════════
//
//  Ollama espone /v1/chat/completions con lo stesso formato di OpenAI:
//  passando di li' le tool call sono native e il codice e' uno solo per
//  Ollama e llama.cpp. La vecchia API /api/generate non conosceva gli
//  strumenti, quindi il modello li improvvisava come testo.

juce::String OllamaProvider::getApiUrl() const
{
    auto base = config.baseUrl.isNotEmpty() ? config.baseUrl.trim()
                                            : juce::String ("http://localhost:11434");
    while (base.endsWithChar ('/')) base = base.dropLastCharacters (1);
    if (base.endsWith ("/v1/chat/completions")) return base;
    if (base.endsWith ("/v1"))                  return base + "/chat/completions";
    return base + "/v1/chat/completions";
}

bool OllamaProvider::testConnection()
{
    const auto base = getApiUrl().upToLastOccurrenceOf ("/chat/completions", false, false);
    juce::String raw = makeHttpRequest (base + "/models", "GET", {}, 5000, {});
    if (raw.contains ("data") || raw.contains ("models")) return true;

    lastError = "Ollama non risponde su " + config.baseUrl
              + ". Avvialo (ollama serve) e riprova.";
    return false;
}

juce::StringArray OllamaProvider::getAvailableModels()
{
    juce::StringArray models;
    const auto base = getApiUrl().upToLastOccurrenceOf ("/chat/completions", false, false);
    auto j = parseJsonSafe (makeHttpRequest (base + "/models", "GET", {}, 5000, {}));
    if (j.contains ("data") && j["data"].is_array())
        for (const auto& m : j["data"])
            if (m.contains ("id") && m["id"].is_string())
                models.add (juce::String (m["id"].get<std::string>()));

    if (models.isEmpty()) models = {"llama3.2", "llama3.1", "mistral", "phi3", "gemma2"};
    return models;
}

// ═══════════════════════════════════════════════════════════════════
//  Gemini Provider
// ═══════════════════════════════════════════════════════════════════

// Gemini non parla il formato OpenAI: i tool diventano functionDeclarations
// e lo storico diventa functionCall / functionResponse dentro i parts.
juce::String GeminiProvider::makeHttpRequest(const juce::String& url, const juce::String& method,
                                               const juce::String& jsonBody, int timeoutMs,
                                               const juce::String& extraHeaders)
{
    return makeHttp(url, method, jsonBody, timeoutMs, extraHeaders);
}

static nlohmann::json geminiToolsFromOpenAI (const juce::String& toolsJson)
{
    auto arr = parseJsonSafe (toolsJson);
    if (! arr.is_array() || arr.empty()) return nlohmann::json();

    nlohmann::json decls = nlohmann::json::array();
    for (auto& t : arr)
    {
        if (! t.is_object() || ! t.contains ("function")) continue;
        const auto& f = t["function"];
        if (! f.is_object() || ! f.contains ("name")) continue;

        nlohmann::json d;
        d["name"] = f["name"];
        if (f.contains ("description")) d["description"] = f["description"];
        if (f.contains ("parameters"))  d["parameters"]  = f["parameters"];
        decls.push_back (d);
    }

    if (decls.empty()) return nlohmann::json();
    return nlohmann::json::array ({ { { "functionDeclarations", decls } } });
}

nlohmann::json geminiContentsFromNeutral (const juce::String& contextJson, const juce::String& userMessage)
{
    nlohmann::json contents = nlohmann::json::array();

    auto prior = parseJsonSafe (contextJson);
    if (prior.is_array())
    {
        for (auto& m : prior)
        {
            if (! m.is_object()) continue;
            const juce::String role = juce::String (m.value ("role", std::string ("user")));

            if (role == "system") continue;   // sta in system_instruction

            if (role == "assistant")
            {
                nlohmann::json parts = nlohmann::json::array();
                if (m.contains ("content") && m["content"].is_string()
                    && ! m["content"].get<std::string>().empty())
                    parts.push_back ({ { "text", m["content"] } });

                if (m.contains ("tool_calls") && m["tool_calls"].is_array())
                    for (auto& tc : m["tool_calls"])
                    {
                        if (! tc.is_object() || ! tc.contains ("function")) continue;
                        auto args = nlohmann::json::object();
                        const auto& f = tc["function"];
                        if (f.contains ("arguments"))
                        {
                            if (f["arguments"].is_object()) args = f["arguments"];
                            else if (f["arguments"].is_string())
                            {
                                auto p = parseJsonSafe (juce::String (f["arguments"].get<std::string>()));
                                if (p.is_object()) args = p;
                            }
                        }
                        parts.push_back ({ { "functionCall", { { "name", f.value ("name", std::string()) }, { "args", args } } } });
                    }

                if (! parts.empty())
                    contents.push_back ({ { "role", "model" }, { "parts", parts } });
            }
            else if (role == "tool")
            {
                // functionResponse.response deve essere un oggetto.
                nlohmann::json response = nlohmann::json::object();
                response["result"] = m.value ("content", std::string());
                contents.push_back ({ { "role", "user" },
                                      { "parts", nlohmann::json::array ({ { { "functionResponse",
                                          { { "name", m.value ("name", std::string()) }, { "response", response } } } } }) } });
            }
            else
            {
                contents.push_back ({ { "role", "user" },
                                      { "parts", nlohmann::json::array ({ { { "text", m.value ("content", std::string()) } } }) } });
            }
        }
    }

    contents.push_back ({ { "role", "user" }, { "parts", nlohmann::json::array ({ { { "text", userMessage.toStdString() } } }) } });
    return contents;
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
    body["contents"] = geminiContentsFromNeutral (config.contextMessages, userMessage);
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["generationConfig"]["maxOutputTokens"] = config.maxTokens;
    if (searchEnabled)
        body["tools"] = nlohmann::json::array({{{"googleSearch", nlohmann::json::object()}}});
    else {
        auto t = geminiToolsFromOpenAI (config.toolsJson);
        if (! t.is_null()) body["tools"] = t;
    }

    juce::String raw = makeHttpRequest(url, "POST", juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);
    try {
        // Un turno puo' contenere testo, functionCall, o entrambi: si leggono
        // tutti i parts invece del solo primo.
        juce::String testo;
        const auto parts = j.at("candidates").at(0).at("content").at("parts");
        for (const auto& p : parts)
        {
            if (p.contains("text") && p["text"].is_string())
                testo += juce::String(p["text"].get<std::string>());
            if (p.contains("functionCall") && p["functionCall"].is_object())
            {
                const auto& fc = p["functionCall"];
                AIProvider::ToolCallResult tc;
                if (fc.contains("name") && fc["name"].is_string())
                    tc.name = juce::String(fc["name"].get<std::string>());
                if (fc.contains("args")) tc.arguments = fc["args"];
                if (tc.name.isNotEmpty()) result.toolCalls.push_back(tc);
            }
        }
        result.text = testo;
        result.success = true;
        streamToolCalls = result.toolCalls;   // stesso contratto dello streaming
    } catch (...) { result.error = extractError(raw); }
    return result;
}

void GeminiProvider::sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    if (!onChunk) return;
    beginRequest();
    clearStreamToolCalls();
    GeminiToolAccumulator acc;

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
    body["contents"] = geminiContentsFromNeutral (config.contextMessages, userMessage);
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["generationConfig"]["maxOutputTokens"] = config.maxTokens;
    if (searchEnabled)
        body["tools"] = nlohmann::json::array({{{"googleSearch", nlohmann::json::object()}}});
    else {
        auto t = geminiToolsFromOpenAI (config.toolsJson);
        if (! t.is_null()) body["tools"] = t;
    }

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
                        if (!parts.is_null() && parts.is_array()) {
                            for (const auto& p : parts) {
                                if (p.contains("text") && p["text"].is_string()) {
                                    ricevutoQualcosa = true;
                                    onChunk(juce::String(p["text"].get<std::string>()), false);
                                }
                                if (p.contains("functionCall") && p["functionCall"].is_object()) {
                                    ricevutoQualcosa = true;
                                    const auto& fc = p["functionCall"];
                                    const juce::String nome = (fc.contains("name") && fc["name"].is_string())
                                                            ? juce::String(fc["name"].get<std::string>()) : juce::String();
                                    if (nome.isNotEmpty()) {
                                        const auto args = fc.contains("args") ? fc["args"] : nlohmann::json::object();
                                        acc.addCall(nome, args);
                                    }
                                }
                            }
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

    streamToolCalls = acc.finish();

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

//==============================================================================
//  OpenRouterAuth (PKCE)
//==============================================================================
static juce::String base64Url (const void* dati, size_t n)
{
    auto s = juce::Base64::toBase64 (dati, n);
    return s.replace ("+", "-").replace ("/", "_").replace ("=", "");
}

OpenRouterAuth::Sfida OpenRouterAuth::creaSfida()
{
    juce::MemoryBlock casuale (48);
    juce::Random r;
    r.setSeedRandomly();
    for (size_t i = 0; i < casuale.getSize(); ++i)
        casuale[i] = (char) r.nextInt (256);

    Sfida s;
    s.verifier = base64Url (casuale.getData(), casuale.getSize());
    const auto hash = juce::SHA256 (s.verifier.toRawUTF8(), (size_t) s.verifier.getNumBytesAsUTF8());
    const auto raw = hash.getRawData();
    s.challenge = base64Url (raw.getData(), raw.getSize());
    return s;
}

juce::String OpenRouterAuth::codiceDaRichiesta (const juce::String& primaRiga)
{
    const int q = primaRiga.indexOf ("code=");
    if (q < 0) return {};
    auto resto = primaRiga.substring (q + 5);
    const int fine = resto.indexOfAnyOf (" &\r\n");
    return (fine >= 0 ? resto.substring (0, fine) : resto).trim();
}

juce::String OpenRouterAuth::urlAutorizzazione (int porta, const juce::String& challenge)
{
    return "https://openrouter.ai/auth?callback_url="
         + juce::URL::addEscapeChars ("http://localhost:" + juce::String (porta) + "/", true)
         + "&code_challenge=" + challenge + "&code_challenge_method=S256";
}

juce::String OpenRouterAuth::accedi (int timeoutSecondi, juce::String& errore, std::function<void (const juce::String&)> apriUrl,
                                  const std::atomic<bool>* stop)
{
    juce::StreamingSocket ascolto;
    if (! ascolto.createListener (0, "127.0.0.1"))
    {
        errore = "Non riesco ad aprire la porta locale per il rientro.";
        return {};
    }
    const auto sfida = creaSfida();
    apriUrl (urlAutorizzazione (ascolto.getBoundPort(), sfida.challenge));

    std::unique_ptr<juce::StreamingSocket> client;
    const auto fine = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutSecondi * 1000u;
    juce::String codice;
    while (juce::Time::getMillisecondCounter() < fine && codice.isEmpty() && ! (stop && stop->load()))
    {
        if (ascolto.waitUntilReady (true, 500) != 1) continue;
        client.reset (ascolto.waitForNextConnection());
        if (client == nullptr) continue;
        char buf[2048] = {};
        client->waitUntilReady (true, 2000);
        const int n = client->read (buf, sizeof (buf) - 1, false);
        if (n > 0)
            codice = codiceDaRichiesta (juce::String::fromUTF8 (buf, n).upToFirstOccurrenceOf ("\r\n", false, false));
        const juce::String pagina = codice.isNotEmpty()
            ? "<html><body style='font-family:sans-serif;background:#0b0b0b;color:#eee;text-align:center;padding-top:20vh'><h2>Fatto. Puoi tornare a WhyCremisi.</h2></body></html>"
            : "<html><body>Niente codice ricevuto.</body></html>";
        const juce::String risposta = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                                    + juce::String ((int) pagina.getNumBytesAsUTF8()) + "\r\n\r\n" + pagina;
        client->write (risposta.toRawUTF8(), (int) risposta.getNumBytesAsUTF8());
        client.reset();
    }
    if (codice.isEmpty())
    {
        errore = "L'accesso non è stato completato in tempo.";
        return {};
    }

    nlohmann::json corpo = { { "code", codice.toStdString() },
                             { "code_verifier", sfida.verifier.toStdString() },
                             { "code_challenge_method", "S256" } };
    const auto risposta = makeHttp ("https://openrouter.ai/api/v1/auth/keys", "POST", corpo.dump(), 15000);
    const auto j = parseJsonSafe (risposta);
    if (j.is_object() && j.contains ("key") && j["key"].is_string())
        return juce::String (j["key"].get<std::string>().data());
    errore = "OpenRouter non ha restituito la chiave.";
    return {};
}
