#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>

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
    };

    struct Result {
        juce::String text;
        bool success = false;
        juce::String error;
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

protected:
    Config config;
    std::atomic<bool> abortRequested{false};
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

protected:
    virtual juce::String getApiUrl() const;
    juce::String makeHttpRequest(const juce::String& url, const juce::String& method,
                                  const juce::String& jsonBody, int timeoutMs,
                                  const juce::String& extraHeaders = {});
};

class AnthropicProvider : public AIProvider
{
public:
    AnthropicProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override { abortRequested = true; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "Anthropic Claude"; }

private:
    juce::String makeHttpRequest(const juce::String& url, const juce::String& method,
                                  const juce::String& jsonBody, int timeoutMs,
                                  const juce::String& extraHeaders = {});
};

class OllamaProvider : public AIProvider
{
public:
    OllamaProvider(const Config& cfg) { configure(cfg); }
    Result sendPrompt(const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming(const juce::String& systemPrompt, const juce::String& userMessage, StreamCallback onChunk) override;
    void abort() override { abortRequested = true; }
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override { return "Ollama"; }
};

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

private:
    juce::String makeHttpRequest(const juce::String& url, const juce::String& method,
                                  const juce::String& jsonBody, int timeoutMs,
                                  const juce::String& extraHeaders = {});
    void setSearchEnabled(bool enabled) { searchEnabled = enabled; }
    bool searchEnabled = false;
};

class OpenRouterProvider : public OpenAIProvider
{
public:
    using OpenAIProvider::OpenAIProvider;
    juce::String getName() const override { return "OpenRouter"; }
private:
    juce::String getApiUrl() const override { return "https://openrouter.ai/api/v1/chat/completions"; }
};

class GroqProvider : public OpenAIProvider
{
public:
    using OpenAIProvider::OpenAIProvider;
    juce::String getName() const override { return "Groq"; }
private:
    juce::String getApiUrl() const override { return "https://api.groq.com/openai/v1/chat/completions"; }
};
