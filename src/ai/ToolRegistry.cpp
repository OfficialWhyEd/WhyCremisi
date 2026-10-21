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

    // ── Catena DSP interna al plugin ────────────────────────────────
    //
    //  Sono i moduli che WhyCremisi ha al proprio interno, distinti dai
    //  plugin caricati nella DAW. Partono tutti bypassati: finche' non
    //  vengono attivati il plugin lascia passare il suono inalterato.
    //
    //  Le descrizioni dicono anche *quando* usare ogni strumento, non
    //  solo cosa fa: e' quello che guida il modello a sceglierlo al
    //  momento giusto invece di ignorarlo.

    registerTool({"dsp.compressor.set",
        "Configura e attiva il compressore interno del plugin. Usalo quando "
        "il mix ha picchi troppo sporgenti, manca di consistenza, o l'utente "
        "chiede piu' controllo sulla dinamica. Il compressore si accende da "
        "solo quando imposti dei valori.", {
        {"threshold", "number", "Soglia in dB: sotto questo livello non comprime", false, -60.0f, 0.0f},
        {"ratio", "number", "Rapporto di compressione, 2 leggero e 10 deciso", false, 1.0f, 20.0f},
        {"attack", "number", "Attacco in ms: basso prende i transienti, alto li lascia passare", false, 0.1f, 200.0f},
        {"release", "number", "Rilascio in ms", false, 5.0f, 2000.0f},
        {"makeup", "number", "Guadagno di recupero in dB", false, 0.0f, 24.0f}
    }});

    registerTool({"dsp.limiter.set",
        "Configura e attiva il limiter interno. Usalo per impedire che il "
        "segnale superi una soglia, tipicamente in fase di finalizzazione o "
        "quando l'utente segnala clipping.", {
        {"ceiling", "number", "Tetto massimo in dB, di norma fra -1 e -0.1", false, -12.0f, 0.0f},
        {"release", "number", "Rilascio in ms", false, 1.0f, 1000.0f}
    }});

    registerTool({"dsp.bypass",
        "Attiva o disattiva un modulo della catena interna. Usalo per "
        "confrontare il suono con e senza lavorazione, o per riportare il "
        "plugin a essere trasparente.", {
        {"module", "string", "Quale modulo: eq, compressor, limiter, oppure all", true},
        {"bypassed", "boolean", "True lo esclude, false lo attiva", true}
    }});

    registerTool({"mix.analyze",
        "Fotografia completa del mix in questo momento: loudness (LUFS "
        "integrato, short-term, momentaneo), true peak, RMS, crest factor, "
        "loudness range, correlazione di fase, centroide e rolloff "
        "spettrale, energia per dieci bande, conteggio dei clipping. "
        "Chiamalo quando l'utente chiede un giudizio sul mix, quando dice "
        "che qualcosa non va senza saper dire cosa, o prima di proporre "
        "una lavorazione: sono le stesse misure su cui si giudica un pezzo "
        "fuori dal DAW, ma prese sul suono che sta passando adesso.", {}});

    registerTool({"ui.showPanels",
        "Mostra nella chat uno o piu' pannelli grafici sotto la tua risposta. "
        "Decidi tu quando servono: usali solo se un grafico o un controllo "
        "aiuta DAVVERO a capire o a fare quello che l'utente sta chiedendo "
        "(es. parla di frequenze, di picchi, di larghezza stereo, di "
        "compressione, oppure chiede di vedere o regolare qualcosa). Per una "
        "domanda a parole, un saluto o una spiegazione NON chiamarlo: la "
        "risposta resta solo testo. Scegli pochi pannelli, di norma 1 o 2.", {
        {"panels", "string", "Pannelli separati da virgola, fra: eq, spectral, loudness, clipping, stereo, vectorscope, compressor, transport, slider (volume), knob (pan), metrics", true}
    }});

    registerTool({"dsp.getState",
        "Riporta com'e' messa adesso la catena interna: quali moduli sono "
        "attivi e con quali valori. Chiamalo prima di modificare qualcosa, "
        "cosi' sai da dove parti invece di indovinare.", {}});
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
    if (messages.size() <= 1) return;

    int targetCount = juce::jmin(maxMessages, (int)messages.size());
    int overByTokens = totalTokens - maxTokens;
    int overByCount = (int)messages.size() - targetCount;

    if (overByTokens <= 0 && overByCount <= 0) return;

    // Determine how many messages to remove — remove oldest non-system pairs
    int removeCount = juce::jmax(overByCount, 1);
    int removed = 0;

    // Scan from index 1 onward (skip index 0 = system prompt)
    for (size_t i = 1; i < messages.size() && removed < removeCount; )
    {
        // Prefer removing user+assistant pairs together
        if (i + 1 < messages.size()
            && messages[i].role == Message::User
            && messages[i + 1].role == Message::Assistant)
        {
            totalTokens -= messages[i].estimatedTokens + messages[i + 1].estimatedTokens;
            messages.erase(messages.begin() + i, messages.begin() + i + 2);
            removed += 2;
        }
        else
        {
            totalTokens -= messages[i].estimatedTokens;
            messages.erase(messages.begin() + i);
            removed += 1;
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

nlohmann::json ContextManager::toJson() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& msg : messages) {
        nlohmann::json j;
        j["role"] = static_cast<int>(msg.role);
        j["content"] = msg.content.toStdString();
        j["estimatedTokens"] = msg.estimatedTokens;
        arr.push_back(j);
    }
    return arr;
}

void ContextManager::fromJson(const nlohmann::json& j)
{
    messages.clear();
    totalTokens = 0;
    if (!j.is_array()) return;
    for (const auto& item : j) {
        Message msg;
        msg.role = static_cast<Message::Role>(item.value("role", 1));
        msg.content = juce::String(item.value("content", ""));
        msg.estimatedTokens = item.value("estimatedTokens", 0);
        messages.push_back(msg);
        totalTokens += msg.estimatedTokens;
    }
}
