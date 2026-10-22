/*
  ==============================================================================
  AiEngine.h
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  AI Engine multi-provider

  Provider supportati: Ollama, OpenAI, Gemini, Anthropic, OpenRouter, Groq, ClaudeCode, Codex.
  Function calling via ToolRegistry, gestione del contesto via ContextManager.
  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AIProvider.h"
#include "ToolRegistry.h"
#include "../daw/DawDetector.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

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
        Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq, ClaudeCode, Codex, LlamaCpp
    };

    // ── Nome canonico di un provider ───────────────────────────
    //
    //  Un'unica coppia di funzioni per tradurre fra enum e stringa, usata da
    //  UI, salvataggio e ripristino. Prima ogni punto del codice aveva la sua
    //  lista scritta a mano: il salvataggio scriveva "Ollama" con la
    //  maiuscola, il ripristino confrontava con "ollama" minuscolo, e nessun
    //  confronto andava mai a segno. Risultato: le impostazioni sembravano
    //  salvate ma a ogni riavvio il provider tornava a quello predefinito.
    static juce::String providerToId (Provider p);
    static Provider providerFromId (const juce::String& id, Provider fallback = Provider::Ollama);

    struct Config {
        Provider provider = Provider::Ollama;
        juce::String apiKey;
        juce::String model = "llama3.2";
        juce::String baseUrl = "http://localhost:11434";
        int timeoutMs = 30000;
        int maxTokens = 2048;
        float temperature = 0.7f;
        AiPersonalityStyle personalityStyle = AiPersonalityStyle::Analytical;

        // Token di un abbonamento Claude, in alternativa alla chiave API.
        juce::String authToken;
        // Profondita' di ragionamento su Claude: low | medium | high | xhigh | max.
        juce::String effort;

        // Strade di riserva, provate nell'ordine se quella principale
        // fallisce prima di rispondere (quota finita, servizio spento).
        struct Riserva {
            Provider provider = Provider::OpenRouter;
            juce::String apiKey;
            juce::String model;
            juce::String baseUrl;
        };
        std::vector<Riserva> riserve;
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
        // Box da mostrare sotto la risposta, scelti dal modello (solo id validi).
        juce::StringArray panels;
        bool success = false;
        juce::String rawToolResponse;
        // Strumenti richiesti da chi non ha le funzioni native (Claude, ChatGPT,
        // Gemini, Ollama): li esegue resolveTextToolCalls.
        std::vector<ToolCall> calls;
    };

    /** Esegue gli strumenti chiesti nel JSON e rimanda al modello i risultati
        veri (max 3 giri). Il testo finale parte da quello che la DAW ha
        confermato, non da quello che il modello pensava di aver fatto. */
    StructuredResponse resolveTextToolCalls (const juce::String& userPrompt, StructuredResponse first);

    /** Esegue le tool call arrivate native (gia' complete) e rimanda al modello
        i risultati veri, max 3 giri. Percorso unico: da qui in poi non conta
        piu' se la chiamata e' nata native o dal protocollo testuale. */
    juce::String resolveToolCalls (const juce::String& userPrompt,
                                   const juce::String& modelText,
                                   const std::vector<AIProvider::ToolCallResult>& calls);

    /** Marcatori [TOOL_CALL:nome {json}] scritti come testo dal modello.
        Rete di sicurezza: non sono il percorso principale. */
    static std::vector<ToolCall> parseToolMarkers (const juce::String& text);

    /** Storico canonico: assistant con le tool call, poi un messaggio per
        ogni risultato con l'id della call a cui risponde. */
    void addAssistantToolCalls (const juce::String& text,
                                const std::vector<ToolCall>& calls);
    void addToolResult (const ToolResult& r);

    /** DAW attualmente collegata: decide quali tool vengono offerti al
        modello e cosa dice il system prompt. */
    void setActiveDaw (DawType daw) { activeDaw.store ((int) daw); }
    DawType getActiveDaw() const { return (DawType) activeDaw.load(); }

    /** Tool call complete dell'ultimo streaming (vuote se il modello non ne ha chieste). */
    std::vector<AIProvider::ToolCallResult> getStreamToolCalls() const;

    /** Box che la chat sa disegnare. Unica lista: la usano il tool ui.showPanels
        e il campo "panels" del JSON dei fornitori senza strumenti nativi. */
    static const juce::StringArray& validPanelIds();

    /** Tiene solo gli id validi, senza doppioni (minuscolo, ordine di arrivo). */
    static juce::StringArray filterPanelIds (const juce::StringArray& requested);

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

    // Motivo dell'ultimo fallimento riportato dal fornitore. Serve al bridge
    // per dire alla UI *cosa* e' andato storto invece di consegnare una
    // risposta vuota.
    juce::String getProviderLastError() const;
    bool isConfigured() const { return configured; }

    // ── Consumo ────────────────────────────────────────────────
    // Somma dei token spesi da quando la sessione e' iniziata, cosi' la UI
    // puo' mostrare quanto si sta consumando invece di lasciarlo indovinare.
    struct SessionUsage {
        long long inputTokens = 0;
        long long outputTokens = 0;
        long long cacheReadTokens = 0;
        long long cacheWriteTokens = 0;
        int requests = 0;

        long long total() const {
            return inputTokens + outputTokens + cacheReadTokens + cacheWriteTokens;
        }
    };

    SessionUsage getSessionUsage() const;
    void resetSessionUsage();

    // Notificata a ogni risposta: parametri (consumo della singola richiesta,
    // totale di sessione). Serve al bridge per aggiornare la UI in tempo reale.
    std::function<void(const AIProvider::Usage&, const SessionUsage&)> onUsageUpdate;
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

    // Session persistence
    bool saveSessionState();
    bool loadSessionState();
    static juce::File getSessionFilePath();

    // Config validation
    struct ConfigValidation {
        bool valid = true;
        juce::StringArray warnings;
        juce::StringArray errors;
    };
    ConfigValidation validateConfig() const;
    juce::String getConfigSummary() const;

    // Public for use by OscBridge streaming path
    StructuredResponse parseStructuredResponse(const juce::String& raw) const;
    void finalizeStreamingResponse(const juce::String& prompt, const juce::String& response);

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

    // shared_ptr e non unique_ptr: una richiesta in streaming dura anche
    // decine di secondi e non puo' tenere occupato engineMutex per tutto quel
    // tempo, altrimenti ogni cambio di modello o di provider fatto nel
    // frattempo blocca l'interfaccia. Si prende una copia del puntatore sotto
    // il lock, si rilascia, e la rete la si aspetta liberi.
    std::shared_ptr<AIProvider> currentProvider;
    std::unique_ptr<AIProvider> createProvider(Provider type);
    std::unique_ptr<AIProvider> createProvider(Provider type, const juce::String& apiKey,
                                               const juce::String& model, const juce::String& baseUrl);
    void ensureProvider();
    void syncProviderConfig();

    std::unique_ptr<ToolRegistry> toolRegistry;
    std::unique_ptr<ContextManager> contextManager;

    juce::String buildPersonalityPrefix() const;
    void syncWidgetTools();

    // DAW attiva: 0 = Unknown. Atomico perche' la scrive il bridge e la
    // legge il thread della richiesta AI.
    std::atomic<int> activeDaw{ (int) DawType::Unknown };

    mutable std::mutex engineMutex;

    // Protetto da usageMutex: le risposte possono arrivare da thread diversi.
    mutable std::mutex usageMutex;
    SessionUsage sessionUsage;
    void accumulateUsage (const AIProvider::Usage& usage);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AiEngine)
};
