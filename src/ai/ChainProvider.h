#pragma once
#include "AIProvider.h"
#include <memory>
#include <vector>

//  Catena di provider: prova il primo, se fallisce PRIMA di aver prodotto
//  testo passa al successivo (DeepSeek, poi un modello gratuito, poi Gemini,
//  Groq, locale). Cosi' l'utente non vede mai "quota finita" se un'altra
//  strada funziona. Se il testo e' gia' iniziato a uscire non si cambia
//  provider a meta' frase: l'errore viene mostrato com'e'.
//
//  I fallimenti vengono tutti elencati nell'errore finale, cosi' si capisce
//  quale strada ha detto cosa. Nessun successo viene dichiarato se nessuno
//  ha risposto davvero.
class ChainProvider : public AIProvider
{
public:
    explicit ChainProvider (std::vector<std::shared_ptr<AIProvider>> providers);

    Result sendPrompt (const juce::String& systemPrompt, const juce::String& userMessage) override;
    void sendPromptStreaming (const juce::String& systemPrompt, const juce::String& userMessage,
                              StreamCallback onChunk) override;
    void abort() override;
    bool testConnection() override;
    juce::StringArray getAvailableModels() override;
    juce::String getName() const override;

    void setToolsJson (const juce::String& json) override;
    void setContextMessages (const juce::String& json) override;

    /** Nome del provider che ha risposto all'ultima richiesta (vuoto se nessuno). */
    juce::String getLastAnswerer() const { return lastAnswerer; }
    int size() const { return static_cast<int> (chain.size()); }

private:
    std::vector<std::shared_ptr<AIProvider>> chain;
    std::atomic<AIProvider*> active { nullptr };
    juce::String lastAnswerer;
};
