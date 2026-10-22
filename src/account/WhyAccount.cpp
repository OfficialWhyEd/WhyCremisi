#include "WhyAccount.h"
#include "../ai/AIProvider.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
    bool inglese (const juce::String& l) { return l.startsWithIgnoreCase ("en"); }
    juce::String T (const juce::String& l, const char* it, const char* en) { return juce::String::fromUTF8 (inglese (l) ? en : it); }

    // POST/GET con codice di stato: su 400/401/422 JUCE da' comunque il corpo.
    int chiama (const juce::String& url, const juce::String& metodo, const juce::String& corpo,
                const juce::String& chiave, const juce::String& bearer, juce::String& risposta)
    {
        int stato = 0;
        juce::String intest = "Content-Type: application/json\r\napikey: " + chiave + "\r\n";
        intest += "Authorization: Bearer " + (bearer.isNotEmpty() ? bearer : chiave) + "\r\n";
        juce::URL u (url);
        if (metodo == "POST") u = u.withPOSTData (corpo);
        auto s = u.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                          .withConnectionTimeoutMs (15000)
                                          .withExtraHeaders (intest)
                                          .withHttpRequestCmd (metodo)
                                          .withStatusCode (&stato));
        if (s == nullptr) { risposta = {}; return 0; }
        risposta = s->readEntireStreamAsString();
        return stato;
    }

    juce::String str (const json& j, const char* k)
    {
        if (j.is_object() && j.contains (k) && j[k].is_string())
            return juce::String::fromUTF8 (j[k].get<std::string>().c_str());
        return {};
    }
}

WhyAccount::Config WhyAccount::caricaConfig()
{
    Config c;
    c.url = juce::SystemStats::getEnvironmentVariable ("WHYCREMISI_SUPABASE_URL", {});
    c.anonKey = juce::SystemStats::getEnvironmentVariable ("WHYCREMISI_SUPABASE_KEY", {});
    if (! c.valida())
    {
        const auto f = cartellaDati().getChildFile ("account.json");
        if (f.existsAsFile())
        {
            try
            {
                auto j = json::parse (f.loadFileAsString().toStdString());
                c.url = str (j, "url");
                c.anonKey = str (j, "anonKey");
            }
            catch (...) {}
        }
    }
    while (c.url.endsWithChar ('/')) c.url = c.url.dropLastCharacters (1);
    return c;
}

juce::File WhyAccount::cartellaDati()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("WhyCremisi");
}

bool WhyAccount::emailPlausibile (const juce::String& e)
{
    const auto s = e.trim();
    const int at = s.indexOfChar ('@');
    return at > 0 && at == s.lastIndexOfChar ('@') && s.substring (at + 1).contains (".")
        && ! s.endsWithChar ('.') && ! s.containsChar (' ');
}

juce::String WhyAccount::messaggioErrore (int stato, const juce::String& codice, const juce::String& serverMsg, const juce::String& l)
{
    const auto c = codice.toLowerCase(), m = serverMsg.toLowerCase();
    if (stato == 0)
        return T (l, "Non riesco a collegarmi al servizio. Controlla la connessione a internet.",
                     "I can't reach the service. Check your internet connection.");
    if (c == "invalid_credentials" || m.contains ("invalid login"))
        return T (l, "Email o password sbagliate.", "Wrong email or password.");
    if (c == "email_not_confirmed" || m.contains ("not confirmed"))
        return T (l, "Devi ancora confermare l'email: apri il messaggio che ti ho mandato e tocca il link.",
                     "You still need to confirm your email: open the message we sent and tap the link.");
    if (c == "user_already_exists" || m.contains ("already registered"))
        return T (l, "Questa email ha già un account. Prova ad accedere.", "This email already has an account. Try signing in.");
    if (c == "weak_password" || m.contains ("password should"))
        return T (l, "La password è troppo debole: usa almeno 8 caratteri.", "The password is too weak: use at least 8 characters.");
    if (c == "over_request_rate_limit" || c == "over_email_send_rate_limit" || stato == 429)
        return T (l, "Troppi tentativi. Aspetta un minuto e riprova.", "Too many attempts. Wait a minute and try again.");
    if (c == "provider_disabled" || m.contains ("provider is not enabled"))
        return T (l, "L'accesso con Google non è ancora attivo su questo servizio.", "Google sign-in is not enabled on this service yet.");
    if (stato == 401 || stato == 403)
        return T (l, "Il servizio ha rifiutato la richiesta (chiave non valida).", "The service refused the request (invalid key).");
    if (stato >= 500)
        return T (l, "Il servizio ha un problema in questo momento. Riprova tra poco.", "The service has a problem right now. Try again shortly.");
    if (serverMsg.isNotEmpty())
        return T (l, "Accesso non riuscito: ", "Sign-in failed: ") + serverMsg;
    return T (l, "Accesso non riuscito (errore ", "Sign-in failed (error ") + juce::String (stato) + ").";
}

WhyAccount::Esito WhyAccount::interpretaRisposta (int stato, const juce::String& corpo, const juce::String& l)
{
    Esito e;
    json j;
    try { j = json::parse (corpo.toStdString()); } catch (...) {}

    if (stato >= 200 && stato < 300 && j.is_object())
    {
        const auto at = str (j, "access_token");
        if (at.isNotEmpty())
        {
            e.sessione.accessToken = at;
            e.sessione.refreshToken = str (j, "refresh_token");
            const auto u = j.contains ("user") ? j["user"] : json();
            e.sessione.email = str (u, "email");
            e.sessione.userId = str (u, "id");
            const int sec = (j.contains ("expires_in") && j["expires_in"].is_number()) ? j["expires_in"].get<int>() : 3600;
            e.sessione.scadeMs = juce::Time::currentTimeMillis() + (juce::int64) sec * 1000;
            e.ok = e.sessione.valida();
            if (! e.ok) e.errore = messaggioErrore (stato, "", "", l);
            return e;
        }
        // registrazione senza sessione: Supabase attende la conferma dell'email
        if (str (j, "id").isNotEmpty())
        {
            e.serveConferma = true;
            e.errore = T (l, "Ti ho mandato una mail: tocca il link per confermare, poi accedi.",
                             "We sent you an email: tap the link to confirm, then sign in.");
            return e;
        }
    }
    const auto codice = str (j, "error_code").isNotEmpty() ? str (j, "error_code") : str (j, "error");
    const auto msg = str (j, "msg").isNotEmpty() ? str (j, "msg")
                   : (str (j, "error_description").isNotEmpty() ? str (j, "error_description") : str (j, "message"));
    e.errore = messaggioErrore (stato, codice, msg, l);
    return e;
}

juce::String WhyAccount::urlGoogle (const Config& c, int porta, const juce::String& challenge)
{
    return c.url + "/auth/v1/authorize?provider=google&redirect_to="
         + juce::URL::addEscapeChars ("http://127.0.0.1:" + juce::String (porta) + "/", true)
         + "&code_challenge=" + challenge + "&code_challenge_method=s256";
}

static WhyAccount::Esito senzaConfig (const juce::String& l)
{
    WhyAccount::Esito e;
    e.errore = T (l, "Il servizio di accesso non è ancora collegato (manca account.json).",
                     "The sign-in service isn't connected yet (account.json is missing).");
    return e;
}

static WhyAccount::Esito passwordFlow (const WhyAccount::Config& c, const juce::String& percorso, const juce::String& email,
                                       const juce::String& password, const juce::String& l)
{
    if (! c.valida()) return senzaConfig (l);
    WhyAccount::Esito e;
    if (! WhyAccount::emailPlausibile (email))
    {
        e.errore = T (l, "L'email non sembra scritta bene.", "The email doesn't look right.");
        return e;
    }
    if (password.length() < 8)
    {
        e.errore = T (l, "La password deve avere almeno 8 caratteri.", "The password needs at least 8 characters.");
        return e;
    }
    juce::String r;
    json b = { { "email", email.trim().toStdString() }, { "password", password.toStdString() } };
    const int s = chiama (c.url + percorso, "POST", juce::String (b.dump()), c.anonKey, {}, r);
    return WhyAccount::interpretaRisposta (s, r, l);
}

WhyAccount::Esito WhyAccount::registrati (const Config& c, const juce::String& e, const juce::String& p, const juce::String& l)
{ return passwordFlow (c, "/auth/v1/signup", e, p, l); }

WhyAccount::Esito WhyAccount::accedi (const Config& c, const juce::String& e, const juce::String& p, const juce::String& l)
{ return passwordFlow (c, "/auth/v1/token?grant_type=password", e, p, l); }

WhyAccount::Esito WhyAccount::rinnova (const Config& c, const juce::String& rt, const juce::String& l)
{
    if (! c.valida()) return senzaConfig (l);
    juce::String r;
    json b = { { "refresh_token", rt.toStdString() } };
    const int s = chiama (c.url + "/auth/v1/token?grant_type=refresh_token", "POST", juce::String (b.dump()), c.anonKey, {}, r);
    return interpretaRisposta (s, r, l);
}

WhyAccount::Esito WhyAccount::recuperaPassword (const Config& c, const juce::String& email, const juce::String& l)
{
    if (! c.valida()) return senzaConfig (l);
    Esito e;
    if (! emailPlausibile (email)) { e.errore = T (l, "L'email non sembra scritta bene.", "The email doesn't look right."); return e; }
    juce::String r;
    json b = { { "email", email.trim().toStdString() } };
    const int s = chiama (c.url + "/auth/v1/recover", "POST", juce::String (b.dump()), c.anonKey, {}, r);
    if (s >= 200 && s < 300)
    {
        e.ok = true;
        e.errore = T (l, "Se l'email esiste, ti arriva il link per scegliere una nuova password.",
                         "If the email exists, you'll get a link to choose a new password.");
        return e;
    }
    return interpretaRisposta (s, r, l);
}

WhyAccount::Esito WhyAccount::accediConGoogle (const Config& c, int timeoutSecondi, const juce::String& l,
                                               std::function<void (const juce::String&)> apriUrl, const std::atomic<bool>* stop)
{
    if (! c.valida()) return senzaConfig (l);
    Esito e;
    juce::StreamingSocket ascolto;
    if (! ascolto.createListener (0, "127.0.0.1"))
    {
        e.errore = T (l, "Non riesco ad aprire la porta locale per il rientro.", "I can't open the local port for the return.");
        return e;
    }
    const auto sfida = OpenRouterAuth::creaSfida();
    apriUrl (urlGoogle (c, ascolto.getBoundPort(), sfida.challenge));

    const auto fine = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutSecondi * 1000u;
    juce::String codice, rifiuto;
    while (juce::Time::getMillisecondCounter() < fine && codice.isEmpty() && rifiuto.isEmpty() && ! (stop && stop->load()))
    {
        if (ascolto.waitUntilReady (true, 500) != 1) continue;
        std::unique_ptr<juce::StreamingSocket> client (ascolto.waitForNextConnection());
        if (client == nullptr) continue;
        char buf[4096] = {};
        client->waitUntilReady (true, 2000);
        const int n = client->read (buf, sizeof (buf) - 1, false);
        if (n > 0)
        {
            const auto riga = juce::String::fromUTF8 (buf, n).upToFirstOccurrenceOf ("\r\n", false, false);
            codice = OpenRouterAuth::codiceDaRichiesta (riga);
            const int q = riga.indexOf ("error_description=");
            if (codice.isEmpty() && q >= 0)
                rifiuto = juce::URL::removeEscapeChars (riga.substring (q + 18).upToFirstOccurrenceOf (" ", false, false)
                                                            .upToFirstOccurrenceOf ("&", false, false).replace ("+", " "));
        }
        const bool bene = codice.isNotEmpty();
        const juce::String stile = "<html><meta charset='utf-8'><body style='font-family:sans-serif;background:#0b0b0b;color:#eee;text-align:center;padding-top:20vh'><h2>";
        const juce::String pagina = stile + juce::String::fromUTF8 (bene ? "Fatto. Puoi tornare a WhyCremisi."
                                                                          : "Accesso non completato. Torna a WhyCremisi.")
                                  + "</h2></body></html>";
        const juce::String ris = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                               + juce::String ((int) pagina.getNumBytesAsUTF8()) + "\r\n\r\n" + pagina;
        client->write (ris.toRawUTF8(), (int) ris.getNumBytesAsUTF8());
    }
    if (rifiuto.isNotEmpty())
    {
        e.errore = messaggioErrore (400, "", rifiuto, l);
        return e;
    }
    if (codice.isEmpty())
    {
        e.errore = T (l, "L'accesso con Google non è stato completato in tempo.", "Google sign-in wasn't completed in time.");
        return e;
    }
    juce::String r;
    json b = { { "auth_code", codice.toStdString() }, { "code_verifier", sfida.verifier.toStdString() } };
    const int s = chiama (c.url + "/auth/v1/token?grant_type=pkce", "POST", juce::String (b.dump()), c.anonKey, {}, r);
    return interpretaRisposta (s, r, l);
}

bool WhyAccount::salva (const Sessione& s)
{
    cartellaDati().createDirectory();
    json j = { { "accessToken", s.accessToken.toStdString() }, { "refreshToken", s.refreshToken.toStdString() },
               { "email", s.email.toStdString() }, { "userId", s.userId.toStdString() }, { "scadeMs", (long long) s.scadeMs } };
    return cartellaDati().getChildFile ("sessione-account.json").replaceWithText (juce::String (j.dump()));
}

WhyAccount::Sessione WhyAccount::carica()
{
    Sessione s;
    const auto f = cartellaDati().getChildFile ("sessione-account.json");
    if (! f.existsAsFile()) return s;
    try
    {
        auto j = json::parse (f.loadFileAsString().toStdString());
        s.accessToken = str (j, "accessToken"); s.refreshToken = str (j, "refreshToken");
        s.email = str (j, "email"); s.userId = str (j, "userId");
        s.scadeMs = j.contains ("scadeMs") && j["scadeMs"].is_number() ? (juce::int64) j["scadeMs"].get<long long>() : 0;
    }
    catch (...) {}
    return s;
}

void WhyAccount::esci() { cartellaDati().getChildFile ("sessione-account.json").deleteFile(); }
