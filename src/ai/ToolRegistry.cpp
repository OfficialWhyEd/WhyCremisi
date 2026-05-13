#include "ToolRegistry.h"

// ═══════════════════════════════════════════════════════════════════
//  ToolRegistry
// ═══════════════════════════════════════════════════════════════════

ToolRegistry::ToolRegistry()
{
    registerBuiltinTools();
}

void ToolRegistry::registerBuiltinTools()
{
    // Transport
    registerTool({"daw.transport.play", "Start playback", {
        {"target", "string", "Track to play from (empty = current position)", false}
    }});
    registerTool({"daw.transport.stop", "Stop playback", {}});
    registerTool({"daw.transport.record", "Toggle recording", {
        {"arm", "boolean", "Whether to arm recording", false}
    }});

    // Track controls
    registerTool({"daw.track.setVolume", "Set track volume", {
        {"track", "number", "Track index (0-based)", true, 0, 255},
        {"volume", "number", "Volume in dB (-96 to 12)", true, -96.0f, 12.0f}
    }});
    registerTool({"daw.track.setPan", "Set track pan", {
        {"track", "number", "Track index (0-based)", true, 0, 255},
        {"pan", "number", "Pan value (-1 to 1)", true, -1.0f, 1.0f}
    }});
    registerTool({"daw.track.mute", "Mute/unmute track", {
        {"track", "number", "Track index (0-based)", true, 0, 255},
        {"mute", "boolean", "True = mute, false = unmute", true}
    }});
    registerTool({"daw.track.solo", "Solo/unsolo track", {
        {"track", "number", "Track index (0-based)", true, 0, 255},
        {"solo", "boolean", "True = solo, false = unsolo", true}
    }});

    // Plugin parameters
    registerTool({"daw.plugin.setParam", "Set a plugin parameter", {
        {"track", "number", "Track index (0-based)", true, 0, 255},
        {"plugin", "string", "Plugin name", true},
        {"param", "string", "Parameter name", true},
        {"value", "number", "Parameter value (normalized 0-1)", true, 0.0f, 1.0f}
    }});
    registerTool({"daw.plugin.bypass", "Bypass/unbypass a plugin", {
        {"track", "number", "Track index", true, 0, 255},
        {"plugin", "string", "Plugin name", true},
        {"bypass", "boolean", "True = bypass, false = enable", true}
    }});

    // Project tempo
    registerTool({"daw.transport.setTempo", "Set project tempo in BPM", {
        {"bpm", "number", "Tempo in beats per minute", true, 20.0f, 999.0f}
    }});

    // Markers
    registerTool({"daw.marker.goto", "Go to marker", {
        {"marker", "string", "Marker name or number", true}
    }});
    registerTool({"daw.marker.set", "Set a marker at current position", {
        {"name", "string", "Marker name", false}
    }});
}

void ToolRegistry::registerTool(const ToolDefinition& tool)
{
    tools[tool.name] = tool;
}

void ToolRegistry::setWidgetTools(const std::vector<std::pair<juce::String, juce::String>>& widgets)
{
    for (const auto& [id, label] : widgets) {
        ToolDefinition def;
        def.name = "widget.set_" + id;
        def.description = "Set " + label + " (" + id + ")";
        def.parameters = {
            {"value", "number", "Normalized value 0-1", true, 0.0f, 1.0f}
        };
        registerTool(def);
    }
}

std::vector<ToolDefinition> ToolRegistry::getAllTools() const
{
    std::vector<ToolDefinition> result;
    for (const auto& [name, def] : tools)
        result.push_back(def);
    return result;
}

ToolDefinition* ToolRegistry::findTool(const juce::String& name)
{
    auto it = tools.find(name);
    return it != tools.end() ? &it->second : nullptr;
}

nlohmann::json ToolRegistry::paramToJsonSchema(const ToolParameter& param) const
{
    nlohmann::json schema;
    if (param.type == "number") {
        schema["type"] = "number";
        schema["minimum"] = param.minVal;
        schema["maximum"] = param.maxVal;
    } else if (param.type == "boolean") {
        schema["type"] = "boolean";
    } else if (param.type == "string") {
        schema["type"] = "string";
        if (!param.enumValues.empty()) {
            schema["enum"] = nlohmann::json::array();
            for (const auto& v : param.enumValues)
                schema["enum"].push_back(v.toStdString());
        }
    }
    schema["description"] = param.description.toStdString();
    return schema;
}

nlohmann::json ToolRegistry::getOpenAITools() const
{
    nlohmann::json toolsArray = nlohmann::json::array();
    for (const auto& [name, def] : tools) {
        nlohmann::json tool;
        tool["type"] = "function";
        tool["function"]["name"] = def.name.toStdString();
        tool["function"]["description"] = def.description.toStdString();
        tool["function"]["parameters"]["type"] = "object";
        tool["function"]["parameters"]["properties"] = nlohmann::json::object();
        tool["function"]["parameters"]["required"] = nlohmann::json::array();
        for (const auto& p : def.parameters) {
            tool["function"]["parameters"]["properties"][p.name.toStdString()] = paramToJsonSchema(p);
            if (p.required)
                tool["function"]["parameters"]["required"].push_back(p.name.toStdString());
        }
        toolsArray.push_back(tool);
    }
    return toolsArray;
}

nlohmann::json ToolRegistry::getAnthropicTools() const
{
    nlohmann::json toolsArray = nlohmann::json::array();
    for (const auto& [name, def] : tools) {
        nlohmann::json tool;
        tool["name"] = def.name.toStdString();
        tool["description"] = def.description.toStdString();
        tool["input_schema"]["type"] = "object";
        tool["input_schema"]["properties"] = nlohmann::json::object();
        for (const auto& p : def.parameters)
            tool["input_schema"]["properties"][p.name.toStdString()] = paramToJsonSchema(p);
        if (std::any_of(def.parameters.begin(), def.parameters.end(), [](const auto& p){ return p.required; }))
            tool["input_schema"]["required"] = nlohmann::json::array();
        for (const auto& p : def.parameters)
            if (p.required)
                tool["input_schema"]["required"].push_back(p.name.toStdString());
        toolsArray.push_back(tool);
    }
    return toolsArray;
}

ToolResult ToolRegistry::executeTool(const ToolCall& call)
{
    if (executor)
        return executor(call);

    ToolResult result;
    result.toolCallId = call.id;
    result.name = call.name;
    result.success = false;
    result.output = "No executor registered";
    return result;
}

std::vector<ToolResult> ToolRegistry::executeTools(const std::vector<ToolCall>& calls)
{
    std::vector<ToolResult> results;
    for (const auto& call : calls)
        results.push_back(executeTool(call));
    return results;
}

// ═══════════════════════════════════════════════════════════════════
//  ContextManager
// ═══════════════════════════════════════════════════════════════════

ContextManager::ContextManager(int maxT, int maxM)
    : maxTokens(maxT), maxMessages(maxM) {}

int ContextManager::estimateTokens(const juce::String& text)
{
    return text.length() / 4 + 10;
}

void ContextManager::addMessage(const Message& msg)
{
    messages.push_back(msg);
    totalTokens += msg.estimatedTokens > 0 ? msg.estimatedTokens : estimateTokens(msg.content);
    trimToBudget();
}

void ContextManager::trimToBudget()
{
    while ((totalTokens > maxTokens || (int)messages.size() > maxMessages) && messages.size() > 1)
    {
        // Never remove the first message (system prompt)
        if (messages.size() > 1 && messages[1].role != Message::System) {
            totalTokens -= messages[1].estimatedTokens;
            messages.erase(messages.begin() + 1);
        } else if (messages.size() > 2) {
            totalTokens -= messages[2].estimatedTokens;
            messages.erase(messages.begin() + 2);
        } else {
            break;
        }
    }
}

void ContextManager::recalculateTokens()
{
    totalTokens = 0;
    for (auto& msg : messages) {
        msg.estimatedTokens = estimateTokens(msg.content);
        totalTokens += msg.estimatedTokens;
    }
}

juce::String ContextManager::buildContextString(const juce::String& systemPrompt) const
{
    juce::String result;
    for (const auto& msg : messages) {
        switch (msg.role) {
            case Message::System:    result += "[System]\n"    + msg.content + "\n\n"; break;
            case Message::User:      result += "[User]\n"      + msg.content + "\n\n"; break;
            case Message::Assistant: result += "[Assistant]\n" + msg.content + "\n\n"; break;
            case Message::Tool:      result += "[Tool]\n"      + msg.content + "\n\n"; break;
        }
    }
    return result;
}
