#pragma once

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <map>
#include <functional>

struct ToolParameter {
    juce::String name;
    juce::String type;
    juce::String description;
    bool required = false;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    std::vector<juce::String> enumValues;
};

struct ToolDefinition {
    juce::String name;
    juce::String description;
    std::vector<ToolParameter> parameters;
};

struct ToolCall {
    juce::String id;
    juce::String name;
    nlohmann::json arguments;
};

struct ToolResult {
    juce::String toolCallId;
    juce::String name;
    bool success = false;
    juce::String output;
};

class ToolRegistry
{
public:
    ToolRegistry();

    void registerTool(const ToolDefinition& tool);
    void setWidgetTools(const std::vector<std::pair<juce::String, juce::String>>& widgets);

    std::vector<ToolDefinition> getAllTools() const;
    ToolDefinition* findTool(const juce::String& name);

    nlohmann::json getOpenAITools() const;
    nlohmann::json getAnthropicTools() const;

    ToolResult executeTool(const ToolCall& call);
    std::vector<ToolResult> executeTools(const std::vector<ToolCall>& calls);

    using ToolExecutor = std::function<ToolResult(const ToolCall&)>;
    void setExecutor(ToolExecutor exec) { executor = exec; }

private:
    std::map<juce::String, ToolDefinition> tools;
    ToolExecutor executor;

    nlohmann::json paramToJsonSchema(const ToolParameter& param) const;
    void registerBuiltinTools();
};

class ContextManager
{
public:
    struct Message {
        enum Role { System, User, Assistant, Tool };
        Role role = User;
        juce::String content;
        int estimatedTokens = 0;
    };

    ContextManager(int maxTokens = 32000, int maxMessages = 100);

    void addMessage(const Message& msg);
    void trimToBudget();
    std::vector<Message> getMessages() const { return messages; }
    int getTotalTokens() const { return totalTokens; }
    void setMaxTokens(int max) { maxTokens = max; }

    juce::String buildContextString(const juce::String& systemPrompt) const;

    static int estimateTokens(const juce::String& text);

    // Session persistence
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::vector<Message> messages;
    int maxTokens;
    int maxMessages;
    int totalTokens = 0;

    void recalculateTokens();
};
