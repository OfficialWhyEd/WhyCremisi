/*
  ==============================================================================
  AiEngine.cpp
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  AI Engine Implementation
  
  Phase 2 PROGRESS: Ollama local HTTP implemented
  Uses JUCE URL and WebInputStream for HTTP requests
  ==============================================================================
*/

#include "AiEngine.h"
#include <nlohmann/json.hpp>

AiEngine::AiEngine()
{
    configured = false;
}

AiEngine::~AiEngine()
{
}

void AiEngine::configure(const Config& cfg)
{
    config = cfg;
    configured = true;
}

void AiEngine::updateConfig(std::function<void(Config&)> updater)
{
    updater(config);
    configured = true;
}

juce::String AiEngine::sendPrompt(const juce::String& prompt)
{
    if (!configured)
    {
        return "[AI] Not configured. Call configure() first.";
    }
    
    switch (config.provider)
    {
        case Provider::Ollama:
            return callOllama(prompt);
            
        case Provider::Gemini:
            return callGemini(prompt);
            
        case Provider::Anthropic:
            return callAnthropic(prompt);
            
        case Provider::OpenAI:
            return callOpenAI(prompt);
            
        case Provider::OpenRouter:
            return callOpenRouter(prompt);
            
        case Provider::Groq:
            return callGroq(prompt);
            
        default:
            return "Unknown provider";
    }
}

void AiEngine::sendPromptAsync(const juce::String& prompt, ResponseCallback callback)
{
    // For now, synchronous call wrapped in async
    // In future, use ThreadPool or async HTTP
    juce::String response = sendPrompt(prompt);
    bool success = !response.startsWith("[AI]") && !response.startsWith("Error");
    callback(response, success);
}

juce::StringArray AiEngine::getAvailableModels()
{
    if (!configured)
        return {"Not configured"};
    
    switch (config.provider)
    {
        case Provider::Ollama:
            return {"llama3.2", "llama3.1", "mistral", "codellama", "phi3", "gemma2"};
            
        case Provider::Gemini:
            return {"gemini-1.5-flash", "gemini-1.5-pro"};
            
        case Provider::Anthropic:
            return {"claude-3-5-sonnet", "claude-3-haiku", "claude-3-opus"};
            
        case Provider::OpenAI:
            return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo"};
            
        case Provider::OpenRouter:
            return {"openai/gpt-4o", "anthropic/claude-3.5-sonnet", "google/gemini-1.5-pro"};
            
        case Provider::Groq:
            return {"llama-3.1-70b-versatile", "llama-3.1-8b-instant", "mixtral-8x7b"};
            
        default:
            return {"Unknown"};
    }
}

juce::String AiEngine::getProviderName() const
{
    switch (config.provider)
    {
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
    if (!configured)
        return false;
    
    if (config.provider == Provider::Ollama)
    {
        // Test by fetching list of models from Ollama
        juce::String url = config.baseUrl + "/api/tags";
        juce::String response = makeHttpRequest(url, "GET", "", 5000);
        return !response.isEmpty() && response.contains("models");
    }
    
    // For cloud providers, just check if API key is set
    return config.apiKey.isNotEmpty();
}

//==============================================================================
// HTTP Helper
//==============================================================================

juce::String AiEngine::makeHttpRequest(const juce::String& urlStr,
                                        const juce::String& method,
                                        const juce::String& jsonBody,
                                        int timeoutMs,
                                        const juce::String& extraHeaders)
{
    juce::URL url(urlStr);

    juce::String headers;
    headers += "Content-Type: application/json\r\n";
    if (extraHeaders.isNotEmpty())
        headers += extraHeaders;
    
    std::unique_ptr<juce::InputStream> stream;
    
    if (method == "GET")
    {
        stream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(timeoutMs)
            .withExtraHeaders(headers)
        );
    }
    else if (method == "POST")
    {
        // POST with body - use withPOSTData for older JUCE versions
        stream = url.withPOSTData(jsonBody.toRawUTF8()).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(timeoutMs)
            .withExtraHeaders(headers)
        );
    }
    
    // Read response
    juce::String response;
    char buffer[4096];
    while (stream && !stream->isExhausted())
    {
        int bytesRead = stream->read(buffer, sizeof(buffer) - 1);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            response += juce::String(buffer);
        }
        else
        {
            break;
        }
    }
    
    if (!stream)
    {
        lastError = "Failed to create connection";
        return {};
    }
    
    // Get HTTP status code if available
    // Note: JUCE doesn't expose status code directly through base InputStream
    // We check if response is empty or contains error indicators
    if (response.isEmpty() && method == "POST")
    {
        // Could be a connection error or empty response
        // For POST, empty response might still be valid for some APIs
    }
    
    return response;
}

//==============================================================================
// Provider implementations
//==============================================================================

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

// Shared body builder for OpenAI-compatible APIs
static nlohmann::json buildOpenAIBody(const juce::String& model, const juce::String& prompt,
                                       float temperature, int maxTokens)
{
    nlohmann::json body;
    body["model"] = model.toStdString();
    body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt.toStdString()}}});
    body["temperature"] = temperature;
    if (maxTokens > 0)
        body["max_tokens"] = maxTokens;
    return body;
}

juce::String AiEngine::callOllama(const juce::String& prompt)
{
    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["prompt"] = prompt.toStdString();
    body["stream"] = false;
    body["options"]["temperature"] = config.temperature;
    if (config.maxTokens > 0)
        body["options"]["num_predict"] = config.maxTokens;

    juce::String raw = makeHttpRequest(config.baseUrl + "/api/generate", "POST",
                                       juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty())
    {
        if (lastError.isEmpty()) lastError = "Empty response from Ollama. Is it running?";
        return "[ERROR] " + lastError;
    }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        if (j.contains("response"))
            return juce::String(j["response"].get<std::string>());
        if (j.contains("error"))
        {
            lastError = "Ollama: " + juce::String(j["error"].get<std::string>());
            return "[ERROR] " + lastError;
        }
    } catch (const std::exception& e) {
        lastError = juce::String("JSON parse: ") + e.what();
    }

    return "[ERROR] " + lastError + " | Raw: " + raw.substring(0, 200);
}

juce::String AiEngine::callGemini(const juce::String& prompt)
{
    if (config.apiKey.isEmpty())
    {
        lastError = "Gemini API key not configured";
        return "[ERROR] " + lastError;
    }

    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                       + config.model + ":generateContent?key=" + config.apiKey;

    nlohmann::json body;
    body["contents"] = nlohmann::json::array({{{"parts", nlohmann::json::array({{{"text", prompt.toStdString()}}})}}});
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0)
        body["generationConfig"]["maxOutputTokens"] = config.maxTokens;

    juce::String raw = makeHttpRequest(url, "POST", juce::String(body.dump()), config.timeoutMs);
    if (raw.isEmpty())
    {
        if (lastError.isEmpty()) lastError = "Empty response from Gemini API";
        return "[ERROR] " + lastError;
    }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        return juce::String(j.at("candidates").at(0).at("content").at("parts").at(0).at("text").get<std::string>());
    } catch (const std::exception& e) {
        lastError = juce::String("Gemini parse: ") + e.what() + " | " + parseError(raw);
    }

    return "[ERROR] " + lastError;
}

juce::String AiEngine::callAnthropic(const juce::String& prompt)
{
    if (config.apiKey.isEmpty())
    {
        lastError = "Anthropic API key not configured";
        return "[ERROR] " + lastError;
    }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["max_tokens"] = (config.maxTokens > 0 ? config.maxTokens : 1024);
    body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt.toStdString()}}});

    // Anthropic requires x-api-key + anthropic-version, NOT Authorization: Bearer
    juce::String authHeaders = "x-api-key: " + config.apiKey + "\r\n"
                               "anthropic-version: 2023-06-01\r\n";

    juce::String raw = makeHttpRequest("https://api.anthropic.com/v1/messages", "POST",
                                       juce::String(body.dump()), config.timeoutMs, authHeaders);
    if (raw.isEmpty())
    {
        if (lastError.isEmpty()) lastError = "Empty response from Anthropic API";
        return "[ERROR] " + lastError;
    }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        return juce::String(j.at("content").at(0).at("text").get<std::string>());
    } catch (const std::exception& e) {
        lastError = juce::String("Anthropic parse: ") + e.what() + " | " + parseError(raw);
    }

    return "[ERROR] " + lastError;
}

juce::String AiEngine::callOpenAICompatible(const juce::String& url, const juce::String& prompt)
{
    if (config.apiKey.isEmpty())
    {
        lastError = "API key not configured";
        return "[ERROR] " + lastError;
    }

    auto body = buildOpenAIBody(config.model, prompt, config.temperature, config.maxTokens);
    juce::String authHeader = "Authorization: Bearer " + config.apiKey + "\r\n";

    juce::String raw = makeHttpRequest(url, "POST", juce::String(body.dump()),
                                       config.timeoutMs, authHeader);
    if (raw.isEmpty())
    {
        if (lastError.isEmpty()) lastError = "Empty response from " + url;
        return "[ERROR] " + lastError;
    }

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        return juce::String(j.at("choices").at(0).at("message").at("content").get<std::string>());
    } catch (const std::exception& e) {
        lastError = juce::String("Parse error: ") + e.what() + " | " + parseError(raw);
    }

    return "[ERROR] " + lastError;
}

juce::String AiEngine::callOpenAI(const juce::String& prompt)
{
    return callOpenAICompatible("https://api.openai.com/v1/chat/completions", prompt);
}

juce::String AiEngine::callOpenRouter(const juce::String& prompt)
{
    return callOpenAICompatible("https://openrouter.ai/api/v1/chat/completions", prompt);
}

juce::String AiEngine::callGroq(const juce::String& prompt)
{
    return callOpenAICompatible("https://api.groq.com/openai/v1/chat/completions", prompt);
}
