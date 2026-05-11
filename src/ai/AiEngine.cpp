#include "AiEngine.h"
#include <nlohmann/json.hpp>

AiEngine::AiEngine() { configured = false; }

AiEngine::~AiEngine() {}

void AiEngine::configure(const Config& cfg) { config = cfg; configured = true; }

void AiEngine::updateConfig(std::function<void(Config&)> updater) { updater(config); configured = true; }

// ── Widget/Context ─────────────────────────────────────────────

void AiEngine::setWidgetList(const std::vector<WidgetInfo>& w)
{
    widgets = w;
}

void AiEngine::setContext(const juce::String& meterData, const juce::String& transportData)
{
    lastMeterData = meterData;
    lastTransportData = transportData;
}

void AiEngine::setPersonalityContext(const juce::String& context)
{
    personalityContext = context;
}

void AiEngine::setAgentWorkspaceContext(const juce::String& context)
{
    agentWorkspaceContext = context;
}

juce::String AiEngine::buildSystemPrompt() const
{
    juce::String prompt;
    prompt += "You are WhyCremisi, an AI mixing engineer assistant.\n";
    prompt += "You can control the mix by outputting structured JSON actions.\n";
    prompt += "Always respond in JSON format:\n";
    prompt += "{\n";
    prompt += "  \"text\": \"Your natural language explanation to the user\",\n";
    prompt += "  \"actions\": [\n";
    prompt += "    { \"widgetId\": \"<id>\", \"value\": <0.0-1.0>, \"description\": \"what this does\" }\n";
    prompt += "  ]\n";
    prompt += "}\n\n";
    prompt += "Available controls:\n";
    for (const auto& w : widgets)
    {
        prompt += "- " + w.widgetId + " (" + w.label + "): range " + juce::String(w.min) + "-" + juce::String(w.max);
        if (w.unit.isNotEmpty()) prompt += " " + w.unit;
        prompt += ", current: " + juce::String(w.currentValue);
        prompt += "\n";
    }
    if (lastMeterData.isNotEmpty())
        prompt += "\nPlugin chain:\n" + lastMeterData + "\n";
    if (lastTransportData.isNotEmpty())
        prompt += "\nTransport:\n" + lastTransportData + "\n";
    if (agentWorkspaceContext.isNotEmpty())
        prompt += "\n=== AGENT WORKSPACE ===\n" + agentWorkspaceContext + "\n";
    if (personalityContext.isNotEmpty())
        prompt += "\n=== PERSONALITY (session memory) ===\n" + personalityContext + "\n";
    prompt += "\nIMPORTANT: values must be in 0.0-1.0 normalized range.\n";
    prompt += "Only output valid JSON, no other text outside the JSON block.\n";
    return prompt;
}

// ── Structured Prompt ──────────────────────────────────────────

AiEngine::StructuredResponse AiEngine::sendPromptStructured(const juce::String& prompt)
{
    juce::String systemPrompt = buildSystemPrompt();
    juce::String fullPrompt = "Context:\n" + systemPrompt + "\n\nUser request:\n" + prompt;

    juce::String raw;
    if (!configured) {
        StructuredResponse r;
        r.text = "[AI] Not configured.";
        r.success = false;
        return r;
    }

    switch (config.provider) {
        case Provider::Ollama: raw = callOllama(fullPrompt); break;
        case Provider::Gemini: raw = callGemini(prompt, systemPrompt); break;
        case Provider::Anthropic: raw = callAnthropic(prompt, systemPrompt); break;
        case Provider::OpenAI: raw = callOpenAI(prompt); break;
        case Provider::OpenRouter: raw = callOpenRouter(prompt); break;
        case Provider::Groq: raw = callGroq(prompt); break;
    }

    auto response = parseStructuredResponse(raw);

    // Execute actions
    if (response.success && actionCallback)
    {
        for (const auto& action : response.actions)
            actionCallback(action);
    }

    return response;
}

void AiEngine::sendPromptAsyncStructured(const juce::String& prompt, StructuredCallback callback)
{
    auto result = sendPromptStructured(prompt);
    if (callback) callback(result);
}

AiEngine::StructuredResponse AiEngine::parseStructuredResponse(const juce::String& raw) const
{
    StructuredResponse response;
    response.success = false;

    if (raw.isEmpty()) return response;

    try {
        auto j = nlohmann::json::parse(raw.toStdString());

        if (j.contains("text"))
            response.text = juce::String(j["text"].get<std::string>());

        if (j.contains("actions") && j["actions"].is_array())
        {
            for (const auto& a : j["actions"])
            {
                AiAction action;
                if (a.contains("widgetId"))
                    action.widgetId = juce::String(a["widgetId"].get<std::string>());
                if (a.contains("value"))
                    action.value = a["value"].get<float>();
                if (a.contains("description"))
                    action.description = juce::String(a["description"].get<std::string>());

                // Look up previous value for undo
                for (const auto& w : widgets)
                {
                    if (w.widgetId == action.widgetId)
                    {
                        action.previousValue = w.currentValue;
                        break;
                    }
                }

                response.actions.push_back(action);
            }
        }
        response.success = true;
    }
    catch (const std::exception& e)
    {
        // If JSON parse fails, treat entire response as text
        response.text = raw;
        response.success = true;
    }

    return response;
}

// ── Sync/Async passthrough ─────────────────────────────────────

juce::String AiEngine::sendPrompt(const juce::String& prompt)
{
    auto r = sendPromptStructured(prompt);
    return r.text;
}

void AiEngine::sendPromptAsync(const juce::String& prompt, ResponseCallback callback)
{
    auto r = sendPromptStructured(prompt);
    if (callback) callback(r.text, r.success);
}

// ── getAvailableModels, getProviderName ───────────────────────

juce::StringArray AiEngine::getAvailableModels()
{
    if (!configured) return {"Not configured"};
    switch (config.provider) {
        case Provider::Ollama: return {"llama3.2", "llama3.1", "mistral", "codellama", "phi3", "gemma2"};
        case Provider::Gemini: return {"gemini-1.5-flash", "gemini-1.5-pro"};
        case Provider::Anthropic: return {"claude-3-5-sonnet", "claude-3-haiku", "claude-3-opus"};
        case Provider::OpenAI: return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo"};
        case Provider::OpenRouter: return {"openai/gpt-4o", "anthropic/claude-3.5-sonnet", "google/gemini-1.5-pro"};
        case Provider::Groq: return {"llama-3.1-70b-versatile", "llama-3.1-8b-instant", "mixtral-8x7b"};
        default: return {"Unknown"};
    }
}

juce::String AiEngine::getProviderName() const
{
    switch (config.provider) {
        case Provider::Ollama: return "Ollama";
        case Provider::Gemini: return "Google Gemini";
        case Provider::Anthropic: return "Anthropic Claude";
        case Provider::OpenAI: return "OpenAI";
        case Provider::OpenRouter: return "OpenRouter";
        case Provider::Groq: return "Groq";
        default: return "Unknown";
    }
}

bool AiEngine::testConnection()
{
    if (!configured) return false;
    if (config.provider == Provider::Ollama) {
        juce::String url = config.baseUrl + "/api/tags";
        juce::String response = makeHttpRequest(url, "GET", "", 5000);
        return !response.isEmpty() && response.contains("models");
    }
    return config.apiKey.isNotEmpty();
}

// ── HTTP Helper ────────────────────────────────────────────────

juce::String AiEngine::makeHttpRequest(const juce::String& urlStr,
                                        const juce::String& method,
                                        const juce::String& jsonBody,
                                        int timeoutMs,
                                        const juce::String& extraHeaders)
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

    if (!stream) { lastError = "Failed to create connection"; return {}; }
    return response;
}

static juce::String parseError(const juce::String& raw)
{
    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        if (j.contains("error") && j["error"].is_object() && j["error"].contains("message"))
            return juce::String(j["error"]["message"].get<std::string>());
        if (j.contains("error") && j["error"].is_string())
            return juce::String(j["error"].get<std::string>());
    } catch (...) {}
    return raw.substring(0, 200);
}

static nlohmann::json buildOpenAIBody(const juce::String& model, const juce::String& prompt,
                                       float temperature, int maxTokens,
                                       const juce::String& systemPrompt = {})
{
    nlohmann::json body;
    body["model"] = model.toStdString();
    body["messages"] = nlohmann::json::array();
    if (systemPrompt.isNotEmpty())
        body["messages"].push_back({{"role", "system"}, {"content", systemPrompt.toStdString()}});
    body["messages"].push_back({{"role", "user"}, {"content", prompt.toStdString()}});
    body["temperature"] = temperature;
    if (maxTokens > 0) body["max_tokens"] = maxTokens;
    return body;
}

// ── Ollama ─────────────────────────────────────────────────────

juce::String AiEngine::callOllama(const juce::String& fullPrompt)
{
    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["prompt"] = fullPrompt.toStdString();
    body["stream"] = false;
    body["options"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["options"]["num_predict"] = config.maxTokens;

    juce::String raw = makeHttpRequest(config.baseUrl + "/api/generate", "POST",
                                        juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty()) {
        if (lastError.isEmpty()) lastError = "Empty response from Ollama";
        return "[ERROR] " + lastError;
    }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        if (j.contains("response")) return juce::String(j["response"].get<std::string>());
        if (j.contains("error")) { lastError = "Ollama: " + juce::String(j["error"].get<std::string>()); return "[ERROR] " + lastError; }
    } catch (const std::exception& e) { lastError = juce::String("JSON parse: ") + e.what(); }
    return "[ERROR] " + lastError;
}

// ── Gemini ─────────────────────────────────────────────────────

juce::String AiEngine::callGemini(const juce::String& prompt, const juce::String& systemPrompt)
{
    if (config.apiKey.isEmpty()) { lastError = "Gemini API key not configured"; return "[ERROR] " + lastError; }

    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                       + config.model + ":generateContent?key=" + config.apiKey;
    nlohmann::json body;
    if (systemPrompt.isNotEmpty())
        body["system_instruction"]["parts"] = nlohmann::json::array({{{"text", systemPrompt.toStdString()}}});
    body["contents"] = nlohmann::json::array({{{"parts", nlohmann::json::array({{{"text", prompt.toStdString()}}})}}});
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["generationConfig"]["maxOutputTokens"] = config.maxTokens;

    juce::String raw = makeHttpRequest(url, "POST", juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty()) { if (lastError.isEmpty()) lastError = "Empty response from Gemini"; return "[ERROR] " + lastError; }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        return juce::String(j.at("candidates").at(0).at("content").at("parts").at(0).at("text").get<std::string>());
    } catch (const std::exception& e) { lastError = juce::String("Gemini parse: ") + e.what(); }
    return "[ERROR] " + lastError;
}

// ── Anthropic ──────────────────────────────────────────────────

juce::String AiEngine::callAnthropic(const juce::String& prompt, const juce::String& systemPrompt)
{
    if (config.apiKey.isEmpty()) { lastError = "Anthropic API key not configured"; return "[ERROR] " + lastError; }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["max_tokens"] = (config.maxTokens > 0 ? config.maxTokens : 1024);
    if (systemPrompt.isNotEmpty())
        body["system"] = systemPrompt.toStdString();
    body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt.toStdString()}}});
    juce::String authHeaders = "x-api-key: " + config.apiKey + "\r\n" "anthropic-version: 2023-06-01\r\n";

    juce::String raw = makeHttpRequest("https://api.anthropic.com/v1/messages", "POST",
                                        juce::String(body.dump()), config.timeoutMs, authHeaders);
    if (raw.isEmpty()) { if (lastError.isEmpty()) lastError = "Empty response from Anthropic"; return "[ERROR] " + lastError; }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        return juce::String(j.at("content").at(0).at("text").get<std::string>());
    } catch (const std::exception& e) { lastError = juce::String("Anthropic parse: ") + e.what(); }
    return "[ERROR] " + lastError;
}

// ── OpenAI-Compatible ──────────────────────────────────────────

juce::String AiEngine::callOpenAICompatible(const juce::String& url, const juce::String& prompt,
                                             const juce::String& systemPrompt)
{
    if (config.apiKey.isEmpty()) { lastError = "API key not configured"; return "[ERROR] " + lastError; }

    auto body = buildOpenAIBody(config.model, prompt, config.temperature, config.maxTokens, systemPrompt);
    juce::String authHeader = "Authorization: Bearer " + config.apiKey + "\r\n";
    juce::String raw = makeHttpRequest(url, "POST", juce::String(body.dump()), config.timeoutMs, authHeader);
    if (raw.isEmpty()) { if (lastError.isEmpty()) lastError = "Empty response"; return "[ERROR] " + lastError; }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        return juce::String(j.at("choices").at(0).at("message").at("content").get<std::string>());
    } catch (const std::exception& e) { lastError = juce::String("Parse: ") + e.what() + " | " + parseError(raw); }
    return "[ERROR] " + lastError;
}

juce::String AiEngine::callOpenAI(const juce::String& prompt)
{
    juce::String sys = buildSystemPrompt();
    return callOpenAICompatible("https://api.openai.com/v1/chat/completions", prompt, sys);
}

juce::String AiEngine::callOpenRouter(const juce::String& prompt)
{
    juce::String sys = buildSystemPrompt();
    return callOpenAICompatible("https://openrouter.ai/api/v1/chat/completions", prompt, sys);
}

juce::String AiEngine::callGroq(const juce::String& prompt)
{
    juce::String sys = buildSystemPrompt();
    return callOpenAICompatible("https://api.groq.com/openai/v1/chat/completions", prompt, sys);
}
