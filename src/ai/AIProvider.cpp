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

    if (config.toolsJson.isNotEmpty()) {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    juce::String authHeader = "Authorization: Bearer " + config.apiKey + "\r\n";
    juce::String raw = makeHttpRequest(getApiUrl(), "POST", juce::String(body.dump()), config.timeoutMs, authHeader);
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);
    if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
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
    if (!onChunk || config.apiKey.isEmpty()) {
        if (onChunk) onChunk("", true);
        return;
    }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["messages"] = buildOpenAIMessages(systemPrompt, userMessage, config.contextMessages);
    body["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["max_tokens"] = config.maxTokens;
    body["stream"] = true;

    if (config.toolsJson.isNotEmpty()) {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    juce::String authHeader = "Authorization: Bearer " + config.apiKey + "\r\n";
    juce::URL url(getApiUrl());

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(config.timeoutMs)
        .withExtraHeaders(authHeader)
        .withNumRedirectsToFollow(5);

    auto stream = url.withPOSTData(body.dump()).createInputStream(options);
    if (!stream) { onChunk("", true); return; }

    juce::String line;
    char ch;
    while (stream->isExhausted() == false && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            if (line.startsWith("data: ")) {
                juce::String data = line.substring(6).trim();
                if (data == "[DONE]") break;
                auto j = parseJsonSafe(data);
                if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
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

    if (!abortRequested.load())
        onChunk("", true);
    abortRequested = false;
}

bool OpenAIProvider::testConnection()
{
    return true;
}

juce::StringArray OpenAIProvider::getAvailableModels()
{
    return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-3.5-turbo"};
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

AIProvider::Result AnthropicProvider::sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage)
{
    Result result;
    if (config.apiKey.isEmpty()) { result.error = "API key not configured"; return result; }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["max_tokens"] = (config.maxTokens > 0 ? config.maxTokens : 1024);
    if (systemPrompt.isNotEmpty())
        body["system"] = systemPrompt.toStdString();
    body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", userMessage.toStdString()}}});

    if (config.toolsJson.isNotEmpty()) {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    juce::String headers = "x-api-key: " + config.apiKey + "\r\nanthropic-version: 2023-06-01\r\n";
    juce::String raw = makeHttpRequest("https://api.anthropic.com/v1/messages", "POST",
                                        juce::String(body.dump()), config.timeoutMs, headers);
    if (raw.isEmpty()) { result.error = "Empty response"; return result; }

    auto j = parseJsonSafe(raw);
    if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
        for (auto& block : j["content"]) {
            juce::String type = juce::String(block["type"].get<std::string>());
            if (type == "text") {
                result.text = juce::String(block["text"].get<std::string>());
            } else if (type == "tool_use") {
                ToolCallResult tcr;
                tcr.id = juce::String(block["id"].get<std::string>());
                tcr.name = juce::String(block["name"].get<std::string>());
                if (block.contains("input"))
                    tcr.arguments = block["input"];
                result.toolCalls.push_back(tcr);
            }
        }
        result.success = true;
    } else {
        result.error = extractError(raw);
    }
    return result;
}

void AnthropicProvider::sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk)
{
    if (!onChunk || config.apiKey.isEmpty()) { if (onChunk) onChunk("", true); return; }

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["max_tokens"] = (config.maxTokens > 0 ? config.maxTokens : 1024);
    if (systemPrompt.isNotEmpty())
        body["system"] = systemPrompt.toStdString();
    body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", userMessage.toStdString()}}});
    body["stream"] = true;

    if (config.toolsJson.isNotEmpty()) {
        auto tools = parseJsonSafe(config.toolsJson);
        if (tools.is_array() && !tools.empty())
            body["tools"] = tools;
    }

    juce::String headers = "x-api-key: " + config.apiKey + "\r\nanthropic-version: 2023-06-01\r\n";

    juce::URL url("https://api.anthropic.com/v1/messages");
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(config.timeoutMs)
        .withExtraHeaders(headers)
        .withNumRedirectsToFollow(5);
    auto stream = url.withPOSTData(body.dump()).createInputStream(options);
    if (!stream) { onChunk("", true); return; }

    juce::String line;
    char ch;
    while (!stream->isExhausted() && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            if (line.startsWith("data: ")) {
                auto j = parseJsonSafe(line.substring(6));
                if (j.contains("type")) {
                    juce::String type = juce::String(j["type"].get<std::string>());
                    if (type == "content_block_delta" && j.contains("delta") && j["delta"].contains("text")) {
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

    onChunk("", true);
    abortRequested = false;
}

bool AnthropicProvider::testConnection()
{
    if (config.apiKey.isEmpty()) return false;
    juce::String headers = "x-api-key: " + config.apiKey + "\r\nanthropic-version: 2023-06-01\r\n";
    juce::String raw = makeHttpRequest("https://api.anthropic.com/v1/messages", "POST",
        "{\"model\":\"claude-3-haiku-20240307\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\"ping\"}]}",
        10000, headers);
    return !raw.contains("error") && raw.isNotEmpty();
}

juce::StringArray AnthropicProvider::getAvailableModels()
{
    return {"claude-3-5-sonnet-20241022", "claude-3-5-haiku-20241022", "claude-3-opus-20240229"};
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

    juce::String fullPrompt = systemPrompt.isNotEmpty()
        ? systemPrompt + "\n\n" + userMessage
        : userMessage;

    nlohmann::json body;
    body["model"] = config.model.toStdString();
    body["prompt"] = fullPrompt.toStdString();
    body["stream"] = true;
    body["options"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["options"]["num_predict"] = config.maxTokens;

    juce::URL url(config.baseUrl + "/api/generate");
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(config.timeoutMs);
    auto stream = url.withPOSTData(body.dump()).createInputStream(options);
    if (!stream) { onChunk("", true); return; }

    juce::String line;
    char ch;
    while (!stream->isExhausted() && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            auto j = parseJsonSafe(line);
            if (j.contains("response"))
                onChunk(juce::String(j["response"].get<std::string>()), false);
            if (j.contains("done") && j["done"].get<bool>())
                break;
            line.clear();
        } else {
            line += ch;
        }
    }

    onChunk("", true);
    abortRequested = false;
}

bool OllamaProvider::testConnection()
{
    juce::String raw = makeHttp(config.baseUrl + "/api/tags", "GET", "", 5000);
    return raw.contains("models");
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
    if (!onChunk || config.apiKey.isEmpty()) { if (onChunk) onChunk("", true); return; }

    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                       + config.model + ":streamGenerateContent?alt=sse&key=" + config.apiKey;

    nlohmann::json body;
    if (systemPrompt.isNotEmpty())
        body["system_instruction"]["parts"] = nlohmann::json::array({{{"text", systemPrompt.toStdString()}}});
    body["contents"] = nlohmann::json::array({{{"parts", nlohmann::json::array({{{"text", userMessage.toStdString()}}})}}});
    body["generationConfig"]["temperature"] = config.temperature;
    if (config.maxTokens > 0) body["generationConfig"]["maxOutputTokens"] = config.maxTokens;

    juce::URL juceUrl(url);
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(config.timeoutMs);
    auto stream = juceUrl.withPOSTData(body.dump()).createInputStream(options);
    if (!stream) { onChunk("", true); return; }

    juce::String line;
    char ch;
    while (!stream->isExhausted() && !abortRequested.load()) {
        if (stream->read(&ch, 1) == 0) break;
        if (ch == '\n') {
            if (line.startsWith("data: ")) {
                auto j = parseJsonSafe(line.substring(6));
                try {
                    auto candidates = j["candidates"];
                    if (!candidates.is_null() && candidates.is_array() && !candidates.empty()) {
                        auto parts = candidates[0]["content"]["parts"];
                        if (!parts.is_null() && parts.is_array() && !parts.empty()) {
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

    onChunk("", true);
    abortRequested = false;
}

bool GeminiProvider::testConnection()
{
    if (config.apiKey.isEmpty()) return false;
    juce::String url = "https://generativelanguage.googleapis.com/v1beta/models?key=" + config.apiKey;
    juce::String raw = makeHttpRequest(url, "GET", "", 10000);
    return raw.contains("models");
}

juce::StringArray GeminiProvider::getAvailableModels()
{
    return {"gemini-1.5-flash", "gemini-1.5-pro", "gemini-2.0-flash-exp"};
}
