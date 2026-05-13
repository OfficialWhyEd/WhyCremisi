#include "AiEngine.h"
#include "AIProvider.h"
#include <nlohmann/json.hpp>

AiEngine::AiEngine() { configured = false; }
AiEngine::~AiEngine() {}

void AiEngine::configure(const Config& cfg)
{
    config = cfg;
    configured = true;
    ensureProvider();
}

void AiEngine::updateConfig(std::function<void(Config&)> updater)
{
    updater(config);
    configured = true;
    ensureProvider();
}

void AiEngine::setPersonalityStyle(AiPersonalityStyle style)
{
    config.personalityStyle = style;
}

// ── Personality Style Helpers ──────────────────────────────

juce::StringArray AiEngine::getPersonalityStyleNames()
{
    return {"Analytical", "Consultative", "Direct", "Creative", "Warm"};
}

juce::String AiEngine::getPersonalityStyleName(AiPersonalityStyle style)
{
    switch (style) {
        case AiPersonalityStyle::Analytical:   return "Analytical";
        case AiPersonalityStyle::Consultative: return "Consultative";
        case AiPersonalityStyle::Direct:       return "Direct";
        case AiPersonalityStyle::Creative:     return "Creative";
        case AiPersonalityStyle::Warm:         return "Warm";
    }
    return "Analytical";
}

juce::String AiEngine::getPersonalityStyleDescription(AiPersonalityStyle style)
{
    switch (style) {
        case AiPersonalityStyle::Analytical:
            return "Precise, data-driven, technical. Shows numbers, spectrum analysis, and exact parameter values.";
        case AiPersonalityStyle::Consultative:
            return "Collaborative, explanatory, educational. Explains why each change helps the mix.";
        case AiPersonalityStyle::Direct:
            return "Concise, action-oriented, efficient. Gets straight to the point with minimal explanation.";
        case AiPersonalityStyle::Creative:
            return "Experimental, bold, artistic. Suggests unconventional approaches and creative chains.";
        case AiPersonalityStyle::Warm:
            return "Supportive, encouraging, human. Builds rapport and celebrates progress.";
    }
    return "";
}

// ── Provider Factory ───────────────────────────────────────

std::unique_ptr<AIProvider> AiEngine::createProvider(Provider type)
{
    AIProvider::Config pcfg;
    pcfg.apiKey = config.apiKey;
    pcfg.model = config.model;
    pcfg.baseUrl = config.baseUrl;
    pcfg.timeoutMs = config.timeoutMs;
    pcfg.maxTokens = config.maxTokens;
    pcfg.temperature = config.temperature;

    switch (type) {
        case Provider::OpenAI:     return std::make_unique<OpenAIProvider>(pcfg);
        case Provider::OpenRouter: return std::make_unique<OpenRouterProvider>(pcfg);
        case Provider::Groq:       return std::make_unique<GroqProvider>(pcfg);
        case Provider::Anthropic:  return std::make_unique<AnthropicProvider>(pcfg);
        case Provider::Ollama:     return std::make_unique<OllamaProvider>(pcfg);
        case Provider::Gemini:     return std::make_unique<GeminiProvider>(pcfg);
    }
    return nullptr;
}

void AiEngine::ensureProvider()
{
    if (!configured) return;
    currentProvider = createProvider(config.provider);
}

// ── Personality Prefix Builder ─────────────────────────────

juce::String AiEngine::buildPersonalityPrefix() const
{
    juce::String prefix;
    prefix += "## Personality: " + getPersonalityStyleName(config.personalityStyle) + "\n";
    prefix += getPersonalityStyleDescription(config.personalityStyle) + "\n\n";

    switch (config.personalityStyle) {
        case AiPersonalityStyle::Analytical:
            prefix += "Always include specific numbers (gain in dB, frequency in Hz, ratio values). ";
            prefix += "Reference the spectrum analyzer and correlation meter data in your reasoning. ";
            prefix += "Format suggestions as: [Parameter] → [value] (reason: [technical justification])\n";
            break;
        case AiPersonalityStyle::Consultative:
            prefix += "Present options with pros and cons. ";
            prefix += "Ask the user for their preference before making changes. ";
            prefix += "Explain the sonic impact of each suggestion in musical terms. ";
            prefix += "Use phrases like 'We could try...' and 'One approach would be...'\n";
            break;
        case AiPersonalityStyle::Direct:
            prefix += "Be brief and actionable. Use bullet points. ";
            prefix += "State the problem, the fix, and the expected result in one sentence each. ";
            prefix += "Skip explanations unless the user asks for them.\n";
            break;
        case AiPersonalityStyle::Creative:
            prefix += "Think outside the box. Suggest routing, parallel processing, and modulation. ";
            prefix += "Reference classic gear and unconventional techniques. ";
            prefix += "Use vivid sonic descriptions. ";
            prefix += "Label suggestions as: [Safe] → standard approach, [Bold] → creative approach\n";
            break;
        case AiPersonalityStyle::Warm:
            prefix += "Use a friendly, conversational tone. Start with a positive observation. ";
            prefix += "Encourage experimentation. Celebrate good mix decisions. ";
            prefix += "Use analogies and relatable language. ";
            prefix += "Ask how the user feels about each suggestion.\n";
            break;
    }

    return prefix;
}

// ── System Prompt Builder ──────────────────────────────────

juce::String AiEngine::buildSystemPrompt() const
{
    juce::String prompt;
    prompt += "You are WhyCremisi, an AI mixing engineer assistant.\n";
    prompt += "You can control the mix by outputting structured JSON actions.\n";
    prompt += "Always respond in JSON format:\n";
    prompt += "{\n";
    prompt += "  \"text\": \"Your natural language explanation to the user\",\n";
    prompt += "  \"actions\": [\n";
    prompt += "    { \"widgetId\": \"<id>\", \"value\": <0.0-1.0>, \"description\": \"what this does\" }\n";
    prompt += "  ]\n";
    prompt += "}\n\n";

    prompt += buildPersonalityPrefix();
    prompt += "\n";

    if (dawName.isNotEmpty())
        prompt += "Connected DAW: " + dawName + "\n\n";

    prompt += "Available controls:\n";
    for (const auto& w : widgets) {
        prompt += "- " + w.widgetId + " (" + w.label + "): range " + juce::String(w.min) + "-" + juce::String(w.max);
        if (w.unit.isNotEmpty()) prompt += " " + w.unit;
        prompt += ", current: " + juce::String(w.currentValue);
        prompt += "\n";
    }

    if (lastMeterData.isNotEmpty())
        prompt += "\nPlugin chain:\n" + lastMeterData + "\n";
    if (lastTransportData.isNotEmpty())
        prompt += "\nTransport:\n" + lastTransportData + "\n";
    if (agentWorkspaceContext.isNotEmpty())
        prompt += "\n=== AGENT WORKSPACE ===\n" + agentWorkspaceContext + "\n";
    if (personalityContext.isNotEmpty())
        prompt += "\n=== PERSONALITY (session memory) ===\n" + personalityContext + "\n";

    prompt += "\nIMPORTANT: values must be in 0.0-1.0 normalized range.\n";
    prompt += "Only output valid JSON, no other text outside the JSON block.\n";
    return prompt;
}

// ── Widget / Context Setters ───────────────────────────────

void AiEngine::setWidgetList(const std::vector<WidgetInfo>& w) { widgets = w; }
void AiEngine::setContext(const juce::String& meterData, const juce::String& transportData)
{
    lastMeterData = meterData;
    lastTransportData = transportData;
}
void AiEngine::setPersonalityContext(const juce::String& context) { personalityContext = context; }
void AiEngine::setAgentWorkspaceContext(const juce::String& context) { agentWorkspaceContext = context; }

// ── Structured Prompt (sync) ───────────────────────────────

AiEngine::StructuredResponse AiEngine::sendPromptStructured(const juce::String& prompt)
{
    if (!configured || !currentProvider) {
        StructuredResponse r;
        r.text = "[AI] Not configured.";
        r.success = false;
        return r;
    }

    juce::String systemPrompt = buildSystemPrompt();
    juce::String fullPrompt = "Context:\n" + systemPrompt + "\n\nUser request:\n" + prompt;
    auto result = currentProvider->sendPrompt(systemPrompt, prompt);

    if (!result.success) {
        StructuredResponse r;
        r.text = "[ERROR] " + result.error;
        r.success = false;
        lastError = result.error;
        return r;
    }

    auto response = parseStructuredResponse(result.text);
    if (response.success && actionCallback) {
        for (const auto& action : response.actions)
            actionCallback(action);
    }
    return response;
}

void AiEngine::sendPromptAsyncStructured(const juce::String& prompt, StructuredCallback callback)
{
    auto result = sendPromptStructured(prompt);
    if (callback) callback(result);
}

// ── Streaming ──────────────────────────────────────────────

void AiEngine::sendPromptStreaming(const juce::String& prompt, StreamCallback onChunk)
{
    if (!configured || !currentProvider || !onChunk) {
        if (onChunk) onChunk("", true);
        return;
    }

    juce::String systemPrompt = buildSystemPrompt();
    currentProvider->sendPromptStreaming(systemPrompt, prompt, onChunk);
}

void AiEngine::sendStructuredStreaming(const juce::String& prompt, StreamCallback onChunk,
                                        std::function<void(const StructuredResponse&)> onComplete)
{
    if (!configured || !currentProvider) {
        if (onComplete) {
            StructuredResponse r;
            r.text = "[AI] Not configured.";
            onComplete(r);
        }
        return;
    }

    juce::String systemPrompt = buildSystemPrompt();
    juce::String accumulated;

    currentProvider->sendPromptStreaming(systemPrompt, prompt,
        [&](const juce::String& chunk, bool isDone) {
            if (!chunk.isEmpty()) accumulated += chunk;
            if (onChunk) onChunk(chunk, isDone);
            if (isDone && onComplete) {
                auto response = parseStructuredResponse(accumulated);
                if (response.success && actionCallback) {
                    for (const auto& action : response.actions)
                        actionCallback(action);
                }
                onComplete(response);
            }
        });
}

void AiEngine::abortRequest()
{
    if (currentProvider)
        currentProvider->abort();
}

// ── Sync passthrough ───────────────────────────────────────

juce::String AiEngine::sendPrompt(const juce::String& prompt)
{
    auto r = sendPromptStructured(prompt);
    return r.text;
}

void AiEngine::sendPromptAsync(const juce::String& prompt, ResponseCallback callback)
{
    auto r = sendPromptStructured(prompt);
    if (callback) callback(r.text, r.success);
}

// ── getAvailableModels, getProviderName ────────────────────

juce::StringArray AiEngine::getAvailableModels()
{
    if (!configured) return {"Not configured"};
    ensureProvider();
    if (currentProvider) return currentProvider->getAvailableModels();
    return {"Unknown"};
}

juce::String AiEngine::getProviderName() const
{
    if (currentProvider) return currentProvider->getName();
    switch (config.provider) {
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
    if (!configured) return false;
    ensureProvider();
    if (currentProvider) return currentProvider->testConnection();
    return false;
}

// ── Response Parser ────────────────────────────────────────

AiEngine::StructuredResponse AiEngine::parseStructuredResponse(const juce::String& raw) const
{
    StructuredResponse response;
    response.success = false;
    if (raw.isEmpty()) return response;

    try {
        auto j = nlohmann::json::parse(raw.toStdString());
        if (j.contains("text"))
            response.text = juce::String(j["text"].get<std::string>());
        if (j.contains("actions") && j["actions"].is_array()) {
            for (const auto& a : j["actions"]) {
                AiAction action;
                if (a.contains("widgetId"))
                    action.widgetId = juce::String(a["widgetId"].get<std::string>());
                if (a.contains("value"))
                    action.value = a["value"].get<float>();
                if (a.contains("description"))
                    action.description = juce::String(a["description"].get<std::string>());
                for (const auto& w : widgets) {
                    if (w.widgetId == action.widgetId) {
                        action.previousValue = w.currentValue;
                        break;
                    }
                }
                response.actions.push_back(action);
            }
        }
        response.success = true;
    } catch (const std::exception& e) {
        response.text = raw;
        response.success = true;
    }
    return response;
}
