#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

class AiEngine
{
public:
    enum class Provider { 
        Ollama,
        Gemini,
        Anthropic,
        OpenAI,
        OpenRouter,
        Groq
    };
    
    struct Config {
        Provider provider = Provider::Ollama;
        juce::String apiKey;
        juce::String model = "llama3.2";
        juce::String baseUrl = "http://localhost:11434";
        int timeoutMs = 30000;
        int maxTokens = 2048;
        float temperature = 0.7f;
    };

    struct AiAction {
        juce::String widgetId;
        float value = 0.0f;
        float previousValue = 0.0f;
        juce::String description;
    };

    struct StructuredResponse {
        juce::String text;
        std::vector<AiAction> actions;
        bool success = false;
    };

    struct WidgetInfo {
        juce::String widgetId;
        juce::String label;
        float min = 0.0f;
        float max = 1.0f;
        float currentValue = 0.0f;
        juce::String unit = "";
    };

    AiEngine();
    ~AiEngine();
    
    void configure(const Config& config);
    void updateConfig(std::function<void(Config&)> updater);
    const Config& getConfig() const { return config; }

    juce::String sendPrompt(const juce::String& prompt);

    using ResponseCallback = std::function<void(const juce::String& response, bool success)>;
    void sendPromptAsync(const juce::String& prompt, ResponseCallback callback);

    StructuredResponse sendPromptStructured(const juce::String& prompt);
    
    using StructuredCallback = std::function<void(const StructuredResponse& response)>;
    void sendPromptAsyncStructured(const juce::String& prompt, StructuredCallback callback);

    juce::StringArray getAvailableModels();
    bool testConnection();
    juce::String getLastError() const { return lastError; }
    bool isConfigured() const { return configured; }
    juce::String getProviderName() const;
    juce::String getModelName() const { return config.model; }

    void setWidgetList(const std::vector<WidgetInfo>& widgets);
    void setContext(const juce::String& meterData, const juce::String& transportData);
    juce::String buildSystemPrompt() const;

    using ActionCallback = std::function<void(const AiAction& action)>;
    void setActionCallback(ActionCallback cb) { actionCallback = cb; }

private:
    Config config;
    bool configured = false;
    mutable juce::String lastError;

    std::vector<WidgetInfo> widgets;
    juce::String lastMeterData;
    juce::String lastTransportData;
    ActionCallback actionCallback;

    StructuredResponse parseStructuredResponse(const juce::String& raw) const;

    juce::String makeHttpRequest(const juce::String& url,
                                  const juce::String& method,
                                  const juce::String& jsonBody,
                                  int timeoutMs,
                                  const juce::String& extraHeaders = {});
    
    juce::String callOllama(const juce::String& prompt);
    juce::String callGemini(const juce::String& prompt);
    juce::String callAnthropic(const juce::String& prompt);
    juce::String callOpenAI(const juce::String& prompt);
    juce::String callOpenRouter(const juce::String& prompt);
    juce::String callGroq(const juce::String& prompt);
    juce::String callOpenAICompatible(const juce::String& url, const juce::String& prompt, const juce::String& systemPrompt = {});

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AiEngine)
};
