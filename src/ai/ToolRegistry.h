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

// Esito di uno strumento, in forma indipendente dalla DAW.
//
// Prima ogni ramo costruiva una stringa a mano: da li' venivano i messaggi
// "Live non ha risposto" letti da chi usa Reaper. Ora l'esito ha una forma
// comune e il testo per il modello si genera in un punto solo.
enum class ToolOutcome
{
    Done,          // eseguito
    Unsupported,   // questa DAW non sa farlo
    Rejected,      // argomenti non validi: non eseguito
    Failed,        // eseguito e fallito
    Unconfirmed    // inviato, nessuna prova che la DAW abbia agito
};

enum class ToolConfirmation
{
    NotApplicable, // non tocca la DAW
    Unconfirmed,   // inviato e basta
    Confirmed      // riletto dalla DAW
};

struct ToolResult {
    juce::String toolCallId;
    juce::String name;
    bool success = false;
    juce::String output;

    ToolOutcome outcome = ToolOutcome::Done;
    ToolConfirmation confirmation = ToolConfirmation::NotApplicable;
    nlohmann::json data = nlohmann::json::object();
    juce::StringArray warnings;
};

/** Testo che il modello legge, generato dall'esito. Un punto solo, cosi'
    tutti i tool parlano la stessa lingua qualunque sia la DAW. */
juce::String renderToolResult (const ToolResult& r);

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

        // Storico canonico: un assistant che ha chiesto strumenti porta qui
        // le tool call (formato OpenAI, che e' il denominatore comune); un
        // messaggio Tool porta l'id della call a cui risponde e il nome dello
        // strumento. Ogni provider li traduce nel proprio dialetto.
        juce::String toolCallsJson;
        juce::String toolCallId;
        juce::String toolName;
    };

    ContextManager(int maxTokens = 32000, int maxMessages = 500);

    void addMessage(const Message& msg);
    void trimToBudget();
    std::vector<Message> getMessages() const { return messages; }
    int getTotalTokens() const { return totalTokens; }
    void setMaxTokens(int max) { maxTokens = max; }
    void setMaxMessages(int max) { maxMessages = max; }

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
