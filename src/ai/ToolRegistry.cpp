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

    registerTool({"samples.scan",
        "Scansiona una cartella di sample (anche le sottocartelle): misura ogni "
        "file (durata, picco, peso dello spettro, tempo dei loop) e lo classifica "
        "(kick, snare, hat, 808, loop...). L'indice resta salvato: una seconda "
        "scansione rilegge solo i file cambiati. Chiamalo la prima volta che "
        "l'utente indica dove tiene i suoi suoni.", {
        {"folder", "string", "Percorso completo della cartella dei sample", true}
    }});

    registerTool({"samples.search",
        "Cerca nell'indice dei sample dell'utente per ruolo e gusto. Serve per "
        "rispondere a 'che kick scuri ho?' o per scegliere un suono prima di "
        "costruire qualcosa.", {
        {"role", "string", "kick, snare, clap, hat, openhat, perc, bass808, bass, loop, melodic, vocal, fx (vuoto = qualsiasi)", false},
        {"query", "string", "Parole libere: scuro, chiaro, corto, lungo, nomi", false},
        {"limit", "number", "Quanti risultati, di norma 8", false, 1, 30}
    }});

    registerTool({"beat.create",
        "Costruisce un beat vero dai sample dell'utente: sceglie i suoni "
        "dall'indice, li mette sulla griglia dello stile e scrive mix.wav piu' "
        "uno stem per ogni corsia in Documenti/WhyCremisi/Beats, tutti della "
        "stessa lunghezza e allineati. Con Ableton Live e AbletonOSC scrive "
        "anche la clip MIDI delle note (kick 36, snare 38, clap 39, hat 42, "
        "open hat 46). Se l'indice e' vuoto, passa 'folder' per scansionare.", {
        {"style", "string", "trap, drill, boombap, lofi, house, techno, dnb (vuoto = lo deduce da 'query')", false},
        {"bpm", "number", "Tempo; vuoto = quello tipico dello stile", false, 40, 240},
        {"bars", "number", "Battute, di norma 4 (massimo 32)", false, 1, 32},
        {"seed", "number", "Stesso seme = stesso beat; cambialo per una variante", false, 0, 999999},
        {"query", "string", "Gusto dei suoni, es. 'dark trap scuro'", false},
        {"folder", "string", "Cartella sample da scansionare prima, se l'indice e' vuoto", false},
        {"track", "number", "Traccia di Live (da 0) dove scrivere la clip MIDI; vuoto = nessuna clip", false, 0, 255},
        {"slot", "number", "Slot della clip, di norma 0", false, 0, 127}
    }});

    registerTool({"mix.levels",
        "Legge da Ableton Live lo stato REALE del mixer, traccia per traccia: "
        "volume in dB, pan e mute. Usalo prima di toccare il mix e dopo, per "
        "provare che i valori sono quelli che servono. Solo Live.", {
        {"tracks", "number", "Quante tracce leggere partendo dalla 0 (di norma 8, massimo 32)", false, 1, 32}
    }});

    registerTool({"mix.snapshot.save",
        "Salva con un nome lo stato attuale del mixer (volume, pan, mute di "
        "ogni traccia, riletti da Live) per poterlo richiamare dopo.", {
        {"name", "string", "Nome dell'istantanea, es. 'mix-prima-del-drop'", true},
        {"tracks", "number", "Quante tracce salvare, di norma 8", false, 1, 32}
    }});

    registerTool({"mix.snapshot.recall",
        "Rimette il mixer di Live com'era in un'istantanea salvata e rilegge "
        "ogni traccia per dire quali valori sono tornati davvero.", {
        {"name", "string", "Nome dell'istantanea da richiamare", true}
    }});

    registerTool({"device.list",
        "Elenca i device e plugin di una traccia di Ableton Live, letti davvero "
        "da Live (numero e nome). Solo Live.", {
        {"track", "number", "Traccia di Live, da 0", true, 0, 255}
    }});

    registerTool({"device.params",
        "Legge i parametri di un plugin su una traccia di Live: nome, valore "
        "attuale, minimo e massimo reali. Con 'query' filtra per nome (es. "
        "'cutoff'); senza filtro mostra i primi 40.", {
        {"track", "number", "Traccia di Live, da 0", true, 0, 255},
        {"device", "string", "Nome (anche parziale) o numero del device, da device.list", true},
        {"query", "string", "Filtra i parametri il cui nome contiene questo testo", false},
        {"limit", "number", "Quanti parametri mostrare (di norma 40, massimo 200)", false, 1, 200}
    }});

    registerTool({"device.setParam",
        "Imposta un parametro di un plugin su una traccia di Live e lo rilegge: "
        "dice 'riconfermato' solo se Live ha davvero quel valore. Prima leggi "
        "device.params per sapere minimo e massimo. Passa 'value' nelle unita' "
        "del parametro, oppure 'normalized' da 0 a 1 sulla sua escursione.", {
        {"track", "number", "Traccia di Live, da 0", true, 0, 255},
        {"device", "string", "Nome (anche parziale) o numero del device", true},
        {"param", "string", "Nome (anche parziale) o numero del parametro", true},
        {"value", "number", "Valore nelle unita' del parametro, entro il suo minimo e massimo", false},
        {"normalized", "number", "Alternativa a value: da 0 (minimo) a 1 (massimo)", false, 0, 1}
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
