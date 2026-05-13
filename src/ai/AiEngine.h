#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>
#include <map>

class AIProvider;
class ToolRegistry;
class ContextManager;

enum class AiPersonalityStyle {
    Analytical,
    Consultative,
    Direct,
    Creative,
    Warm
};

class AiEngine
{
public:
    enum class Provider {
        Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq
    };

    struct Config {
        Provider provider = Provider::Ollama;
        juce::String apiKey;
        juce::String model = "llama3.2";
        juce::String baseUrl = "http://localhost:11434";
        int timeoutMs = 30000;
        int maxTokens = 2048;
        float temperature = 0.7f;
        AiPersonalityStyle personalityStyle = AiPersonalityStyle::Analytical;
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
        juce::String rawToolResponse;
    };

    struct WidgetInfo {
        juce::String widgetId;
        juce::String label;
        float min = 0.0f;
        float max = 1.0f;
        float currentValue = 0.0f;
        juce::String unit;
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

    using StreamCallback = std::function<void(const juce::String& chunk, bool isDone)>;
    void sendPromptStreaming(const juce::String& prompt, StreamCallback onChunk);
    void sendStructuredStreaming(const juce::String& prompt, StreamCallback onChunk,
                                  std::function<void(const StructuredResponse&)> onComplete);
    void abortRequest();

    juce::StringArray getAvailableModels();
    bool testConnection();
    juce::String getLastError() const { return lastError; }
    bool isConfigured() const { return configured; }
    juce::String getProviderName() const;
    juce::String getModelName() const { return config.model; }

    void setWidgetList(const std::vector<WidgetInfo>& widgets);
    void setContext(const juce::String& meterData, const juce::String& transportData);
    void setPersonalityContext(const juce::String& context);
    void setAgentWorkspaceContext(const juce::String& context);
    void setPersonalityStyle(AiPersonalityStyle style);
    void setDawName(const juce::String& name) { dawName = name; }

    juce::String buildSystemPrompt() const;

    using ActionCallback = std::function<void(const AiAction& action)>;
    void setActionCallback(ActionCallback cb) { actionCallback = cb; }

    static juce::StringArray getPersonalityStyleNames();
    static juce::String getPersonalityStyleName(AiPersonalityStyle style);
    static juce::String getPersonalityStyleDescription(AiPersonalityStyle style);

    // Tool integration
    void setToolsEnabled(bool enabled) { toolsEnabled = enabled; }
    juce::String buildToolsJson() const;
    using ToolExecutorFn = std::function<struct ToolResult(const struct ToolCall&)>;
    void setToolExecutor(ToolExecutorFn exec);

    // Context management
    void addConversationMessage(const juce::String& role, const juce::String& content);
    void clearConversationContext();
    juce::String buildContextMessagesJson() const;

private:
    Config config;
    bool configured = false;
    bool toolsEnabled = true;
    mutable juce::String lastError;

    std::vector<WidgetInfo> widgets;
    juce::String lastMeterData;
    juce::String lastTransportData;
    juce::String personalityContext;
    juce::String agentWorkspaceContext;
    juce::String dawName;
    ActionCallback actionCallback;

    std::unique_ptr<AIProvider> currentProvider;
    std::unique_ptr<AIProvider> createProvider(Provider type);
    void ensureProvider();
    void syncProviderConfig();

    std::unique_ptr<ToolRegistry> toolRegistry;
    std::unique_ptr<ContextManager> contextManager;

    StructuredResponse parseStructuredResponse(const juce::String& raw) const;
    juce::String buildPersonalityPrefix() const;
    void syncWidgetTools();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AiEngine)
};
