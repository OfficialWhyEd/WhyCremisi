#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>

// Account WhyCremisi: accesso con email e password oppure con Google.
// Servizio: Supabase Auth (GoTrue, open source). Si parla in REST, niente SDK.
// L'indirizzo del progetto e la chiave "anon" (pubblica per natura) stanno in
// account.json nella cartella dati dell'utente oppure nelle variabili
// WHYCREMISI_SUPABASE_URL / WHYCREMISI_SUPABASE_KEY. Senza, l'accesso dice
// chiaramente che il servizio non e' ancora collegato.
struct WhyAccount
{
    struct Config
    {
        juce::String url, anonKey;
        bool valida() const { return (url.startsWith ("https://") || url.startsWith ("http://127.0.0.1")) && anonKey.isNotEmpty(); }
    };

    struct Sessione
    {
        juce::String accessToken, refreshToken, email, userId;
        juce::int64 scadeMs = 0;   // istante (ms epoch) in cui l'access token scade
        bool valida() const { return accessToken.isNotEmpty() && userId.isNotEmpty(); }
    };

    struct Esito
    {
        bool ok = false;
        bool serveConferma = false;   // registrazione fatta, manca il clic nella mail
        juce::String errore;          // frase leggibile, con il motivo
        Sessione sessione;
    };

    static Config caricaConfig();
    static juce::File cartellaDati();

    // Parti pure (testabili senza rete)
    static Esito interpretaRisposta (int statoHttp, const juce::String& corpo, const juce::String& lingua);
    static juce::String messaggioErrore (int statoHttp, const juce::String& codice,
                                         const juce::String& messaggioServer, const juce::String& lingua);
    static juce::String urlGoogle (const Config& c, int porta, const juce::String& challenge);
    static bool emailPlausibile (const juce::String& email);

    // Con la rete (bloccano: da chiamare su un thread di lavoro)
    static Esito registrati (const Config&, const juce::String& email, const juce::String& password, const juce::String& lingua);
    static Esito accedi     (const Config&, const juce::String& email, const juce::String& password, const juce::String& lingua);
    static Esito accediConGoogle (const Config&, int timeoutSecondi, const juce::String& lingua,
                                  std::function<void (const juce::String&)> apriUrl, const std::atomic<bool>* stop);
    static Esito rinnova (const Config&, const juce::String& refreshToken, const juce::String& lingua);
    static Esito recuperaPassword (const Config&, const juce::String& email, const juce::String& lingua);

    // Sessione salvata sul disco dell'utente (la cifratura e' la fase sicurezza)
    static bool salva (const Sessione&);
    static Sessione carica();
    static void esci();
};
