/*
  ==============================================================================
  AiEngine.h
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  AI Engine for multi-provider support
  
  Phase 2 IN PROGRESS: Ollama local implemented
  Phase 3: Will add Gemini, Anthropic, OpenAI, OpenRouter, Groq
  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

class AiEngine
{
public:
    enum class Provider { 
        Ollama,      // Local LLM (default)
        Gemini,      // Google Gemini
        Anthropic,   // Claude
        OpenAI,      // GPT
        OpenRouter,  // Multi-provider
        Groq         // Fast inference
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
    
    AiEngine();
    ~AiEngine();
    
    //==============================================================================
    /** Configure the AI provider */
    void configure(const Config& config);
    
    //==============================================================================
    /** Update partial config (preserves other settings) */
    void updateConfig(std::function<void(Config&)> updater);
    
    //==============================================================================
    /** Get current config */
    const Config& getConfig() const { return config; }
    
    //==============================================================================
    /** Send a prompt and get response (synchronous - blocks!) */
    juce::String sendPrompt(const juce::String& prompt);
    
    //==============================================================================
    /** Send a prompt asynchronously (non-blocking) */
    using ResponseCallback = std::function<void(const juce::String& response, bool success)>;
    void sendPromptAsync(const juce::String& prompt, ResponseCallback callback);
    
    //==============================================================================
    /** Get available models for current provider */
    juce::StringArray getAvailableModels();
    
    //==============================================================================
    /** Test connection to provider */
    bool testConnection();
    
    //==============================================================================
    /** Get last error message */
    juce::String getLastError() const { return lastError; }
    
    //==============================================================================
    /** Check if configured */
    bool isConfigured() const { return configured; }
    
    //==============================================================================
    /** Get current provider name */
    juce::String getProviderName() const;
    
    //==============================================================================
    /** Get current model name */
    juce::String getModelName() const { return config.model; }

private:
    Config config;
    bool configured = false;
    juce::String lastError;
    
    //==============================================================================
    /** HTTP helper for making requests */
    juce::String makeHttpRequest(const juce::String& url,
                                  const juce::String& method,
                                  const juce::String& jsonBody,
                                  int timeoutMs,
                                  const juce::String& extraHeaders = {});
    
    //==============================================================================
    /** Internal implementations for each provider */
    juce::String callOllama(const juce::String& prompt);
    juce::String callGemini(const juce::String& prompt);
    juce::String callAnthropic(const juce::String& prompt);
    juce::String callOpenAI(const juce::String& prompt);
    juce::String callOpenRouter(const juce::String& prompt);
    juce::String callGroq(const juce::String& prompt);
    juce::String callOpenAICompatible(const juce::String& url, const juce::String& prompt);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AiEngine)
};