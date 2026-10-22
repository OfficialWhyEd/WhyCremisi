#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <functional>

// Aggiornamenti di WhyCremisi. All'apertura il plugin legge latest.json allegato alla
// release "beta" su GitHub, confronta la versione, lo dice nella UI e (se l'utente
// accetta) scarica l'installer, ne controlla lo SHA-256 e lo lancia quando la DAW si
// chiude: un plugin caricato non puo' sostituire se stesso. Niente librerie esterne.
// Il flag "auto" in aggiornamenti.json (cartella dati) spegne il controllo.
struct WhyUpdate
{
    struct Rilascio
    {
        juce::String versione, note, url, sha256;
        juce::int64 dimensione = 0;
        bool valido() const { return versione.isNotEmpty() && url.startsWith ("https://") && sha256.length() == 64; }
    };

    struct Impostazioni
    {
        bool automatico = true;               // controlla da solo all'apertura
        juce::String canale = "beta";         // tag della release su GitHub
    };

    static juce::String versioneCorrente();   // da WHYCREMISI_VERSION (CMake, file VERSION)
    static juce::String piattaforma();        // "windows" | "linux" | "macos"

    // Parti pure (testabili senza rete)
    static int confronta (const juce::String& a, const juce::String& b);      // <0, 0, >0
    static Rilascio interpretaManifest (const juce::String& json, const juce::String& piattaforma);
    static juce::String urlManifest (const juce::String& canale);              // WHYCREMISI_UPDATE_URL sovrascrive
    static bool sha256Corrisponde (const juce::File& f, const juce::String& atteso);
    static juce::String comandoAllaChiusura (const juce::File& installer, int pidDaw, const juce::String& piattaforma);

    // Impostazioni su disco
    static juce::File fileImpostazioni();
    static Impostazioni caricaImpostazioni();
    static bool salvaImpostazioni (const Impostazioni&);

    // Con la rete (bloccano: su un thread di lavoro)
    struct Controllo { bool riuscito = false; bool nuova = false; Rilascio rilascio; juce::String errore; };
    static Controllo controlla (const juce::String& lingua);
    static bool scarica (const Rilascio&, juce::File& destinazione, juce::String& errore,
                         const std::atomic<bool>* stop, std::function<void (int)> progresso);
    static bool installaAllaChiusura (const juce::File& installer, juce::String& errore);
};
