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

juce::StringArray ClaudeCodeProvider::getAvailableModels()
{
    return { "sonnet", "opus", "haiku" };
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
    // I modelli dipendono dall'abbonamento: l'elenco serve solo come suggerimento.
    return { "default", "gpt-5-codex", "gpt-5" };
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
