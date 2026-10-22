#include "ChainProvider.h"

ChainProvider::ChainProvider (std::vector<std::shared_ptr<AIProvider>> providers)
    : chain (std::move (providers))
{
}

static juce::String razione (const AIProvider& p, const juce::String& errore)
{
    return p.getName() + ": " + (errore.isNotEmpty() ? errore : juce::String ("nessuna risposta"));
}

AIProvider::Result ChainProvider::sendPrompt (const juce::String& systemPrompt, const juce::String& userMessage)
{
    beginRequest();
    lastAnswerer.clear();

    Result ultimo;
    juce::StringArray fallimenti;

    for (auto& p : chain)
    {
        if (abortRequested) { lastAborted = true; break; }

        active = p.get();
        auto r = p->sendPrompt (systemPrompt, userMessage);
        active = nullptr;

        if (r.success)
        {
            lastAnswerer = p->getName();
            return r;
        }

        ultimo = r;
        fallimenti.add (razione (*p, r.error));
        if (p->wasAborted()) { lastAborted = true; break; }
    }

    ultimo.success = false;
    ultimo.error = fallimenti.isEmpty() ? juce::String ("Nessun provider configurato.")
                                        : fallimenti.joinIntoString (" | ");
    lastError = ultimo.error;
    return ultimo;
}

void ChainProvider::sendPromptStreaming (const juce::String& systemPrompt, const juce::String& userMessage,
                                         StreamCallback onChunk)
{
    beginRequest();
    lastAnswerer.clear();

    juce::StringArray fallimenti;

    for (auto& p : chain)
    {
        if (abortRequested) { lastAborted = true; break; }

        bool uscitoTesto = false;
        active = p.get();
        p->sendPromptStreaming (systemPrompt, userMessage,
            [&] (const juce::String& chunk, bool done)
            {
                if (chunk.isNotEmpty())
                {
                    uscitoTesto = true;
                    onChunk (chunk, false);
                }
                // la chiusura la decide la catena, non il singolo provider
                juce::ignoreUnused (done);
            });
        active = nullptr;

        const bool fallito = p->getLastError().isNotEmpty() && ! uscitoTesto;

        if (p->wasAborted()) { lastAborted = true; break; }

        if (uscitoTesto || ! fallito)
        {
            // Risposta (anche se interrotta a meta') o risposta vuota senza errore.
            lastAnswerer = p->getName();
            if (p->getLastError().isNotEmpty())
            {
                lastError = razione (*p, p->getLastError());
                lastStatusCode = p->getLastStatusCode();
            }
            onChunk ({}, true);
            return;
        }

        fallimenti.add (razione (*p, p->getLastError()));
        lastStatusCode = p->getLastStatusCode();
    }

    lastError = fallimenti.isEmpty() ? (lastAborted ? juce::String() : juce::String ("Nessun provider configurato."))
                                     : fallimenti.joinIntoString (" | ");
    onChunk ({}, true);
}

void ChainProvider::abort()
{
    abortRequested = true;
    if (auto* a = active.load())
        a->abort();
}

bool ChainProvider::testConnection()
{
    for (auto& p : chain)
        if (p->testConnection())
            return true;
    return false;
}

juce::StringArray ChainProvider::getAvailableModels()
{
    return chain.empty() ? juce::StringArray() : chain.front()->getAvailableModels();
}

juce::String ChainProvider::getName() const
{
    juce::StringArray nomi;
    for (auto& p : chain)
        nomi.add (p->getName());
    return "Catena (" + nomi.joinIntoString (" > ") + ")";
}

void ChainProvider::setToolsJson (const juce::String& json)
{
    AIProvider::setToolsJson (json);
    for (auto& p : chain) p->setToolsJson (json);
}

void ChainProvider::setContextMessages (const juce::String& json)
{
    AIProvider::setContextMessages (json);
    for (auto& p : chain) p->setContextMessages (json);
}
