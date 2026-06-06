/*
   ==============================================================================
   WebViewBridge.h
   WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  Communication bridge between C++ and React UI
   ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_core/juce_core.h>
#include <functional>
#include <nlohmann/json.hpp>

/**
 * Bridge per comunicazione bidirezionale C++ <-> JavaScript/React
 * 
 * Meccanismo:
 * - C++ -> JS: goToURL("javascript:...")
 * - JS -> C++: URL interception (pageAboutToLoad su app://message/...)
 */
class WebViewBridge
{
public:
    WebViewBridge();
    ~WebViewBridge();
    
    // Initialize the web view - chiama questa dopo aver creato il WebBrowserComponent
    void initialize(juce::WebBrowserComponent* webBrowser);
    void shutdown();
    
    // Verifica se il bridge è inizializzato
    bool isInitialized() const { return webView != nullptr; }
    
    // Invia messaggio JSON al frontend React
    // Usa goToURL("javascript:...") per compatibilità JUCE
    void sendToFrontend(const nlohmann::json& message);
    
    // Shortcuts per tipi di messaggi comuni
    void sendTransport(bool isPlaying, bool isRecording, float bpm, float position);
    void sendTrackUpdate(int trackId, const juce::String& name, float volumeDb, float pan);
    void sendMeter(int trackId, float leftDb, float rightDb);
    void sendAIResponse(const juce::String& requestId, const juce::String& content, 
                        const juce::String& provider = "ollama", bool isComplete = true);
    void sendAIStream(const juce::String& requestId, const juce::String& chunk, bool isDone);
    void sendOSCMessage(const juce::String& address, float value);
    void sendWidgetCreate(const juce::String& widgetId, const juce::String& widgetType,
                          const juce::String& title, const nlohmann::json& config);
    void sendWidgetUpdate(const juce::String& widgetId, const nlohmann::json& values);
    void sendWidgetRemove(const juce::String& widgetId);
    void sendError(const juce::String& code, const juce::String& message, const juce::String& severity = "error");
    
    // Callback per messaggi ricevuti dal frontend
    using FrontendMessageCallback = std::function<void(const nlohmann::json&)>;
    void setFrontendMessageCallback(FrontendMessageCallback callback);
    
    // Gestisce messaggi dal frontend (chiamato da PluginEditor::pageAboutToLoad)
    // Ritorna true se l'URL è stato consumato (non navigare)
    bool handleMessageFromURL(const juce::String& url);
    
    // DEPRECATED: Usa handleMessageFromURL invece
    void handleMessageFromFrontend(const juce::String& jsonString);
    
private:
    juce::WebBrowserComponent* webView = nullptr;
    FrontendMessageCallback frontendMessageCallback;
    
    // Helper per escape JSON sicuro per JavaScript
    juce::String escapeForJavaScript(const juce::String& input);
    
    // Helper per creare messaggio base
    nlohmann::json createBaseMessage(const juce::String& type, const juce::String& id = "");
};