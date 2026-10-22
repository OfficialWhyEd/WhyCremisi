#include "../WhyAccount.h"
#include <juce_core/juce_core.h>

class WhyAccountTest : public juce::UnitTest
{
public:
    WhyAccountTest() : juce::UnitTest ("WhyAccountTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("email");
        expect (WhyAccount::emailPlausibile ("a@b.it"));
        expect (! WhyAccount::emailPlausibile ("a@b"));
        expect (! WhyAccount::emailPlausibile ("a b@c.it"));
        expect (! WhyAccount::emailPlausibile ("@c.it"));

        beginTest ("risposta di accesso");
        const juce::String ok = R"({"access_token":"AT","refresh_token":"RT","expires_in":3600,"user":{"id":"u1","email":"x@y.it"}})";
        auto e = WhyAccount::interpretaRisposta (200, ok, "it");
        expect (e.ok && e.sessione.accessToken == "AT" && e.sessione.email == "x@y.it" && e.sessione.scadeMs > juce::Time::currentTimeMillis());

        beginTest ("registrazione che attende la conferma");
        e = WhyAccount::interpretaRisposta (200, R"({"id":"u2","email":"x@y.it"})", "it");
        expect (! e.ok && e.serveConferma && e.errore.isNotEmpty());

        beginTest ("errori con il motivo, in italiano e inglese");
        e = WhyAccount::interpretaRisposta (400, R"({"error_code":"invalid_credentials","msg":"Invalid login credentials"})", "it");
        expect (! e.ok && e.errore.contains ("sbagliate"));
        e = WhyAccount::interpretaRisposta (400, R"({"error_code":"invalid_credentials"})", "en");
        expect (e.errore.contains ("Wrong"));
        e = WhyAccount::interpretaRisposta (0, {}, "it");
        expect (e.errore.contains ("connessione"));
        e = WhyAccount::interpretaRisposta (422, R"({"error_code":"user_already_exists"})", "it");
        expect (e.errore.contains ("già un account"));
        e = WhyAccount::interpretaRisposta (400, R"({"error_code":"provider_disabled"})", "it");
        expect (e.errore.contains ("Google"));
        expect (WhyAccount::interpretaRisposta (500, "boh", "it").errore.contains ("problema"));

        beginTest ("senza configurazione dice perche'");
        WhyAccount::Config vuota;
        e = WhyAccount::accedi (vuota, "a@b.it", "password1", "it");
        expect (! e.ok && e.errore.contains ("account.json"));
        WhyAccount::Config c { "https://x.supabase.co", "KEY" };
        expect (WhyAccount::accedi (c, "non-email", "password1", "it").errore.contains ("email"));
        expect (WhyAccount::accedi (c, "a@b.it", "corta", "en").errore.contains ("8"));

        const auto serverProva = juce::SystemStats::getEnvironmentVariable ("WHYCREMISI_TEST_URL", {});
        if (serverProva.isNotEmpty())
        {
            beginTest ("contro un server di prova locale");
            WhyAccount::Config s { serverProva, "KEY" };
            auto r1 = WhyAccount::accedi (s, "bad@b.it", "password1", "it");
            expect (! r1.ok && r1.errore.contains ("sbagliate"), r1.errore);
            auto r2 = WhyAccount::accedi (s, "ok@b.it", "password1", "it");
            expect (r2.ok && r2.sessione.accessToken == "AT", r2.errore);
            auto r3 = WhyAccount::registrati (s, "new@b.it", "password1", "it");
            expect (r3.serveConferma, r3.errore);
        }

        beginTest ("indirizzo Google");
        const auto u = WhyAccount::urlGoogle (c, 5555, "CH");
        expect (u.startsWith ("https://x.supabase.co/auth/v1/authorize?provider=google"));
        expect (u.contains ("127.0.0.1%3A5555") && u.contains ("code_challenge=CH") && u.contains ("s256"));
    }
};

static WhyAccountTest whyAccountTest;
