#include "../WhyUpdate.h"
#include "../../daw/ReaperSetup.h"
#include <juce_core/juce_core.h>

class WhyUpdateTest : public juce::UnitTest
{
public:
    WhyUpdateTest() : juce::UnitTest ("WhyUpdateTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("confronto versioni");
        expect (WhyUpdate::confronta ("0.2.0", "0.2.1") < 0);
        expect (WhyUpdate::confronta ("0.10.0", "0.9.9") > 0);
        expect (WhyUpdate::confronta ("1.0", "1.0.0") == 0);
        expect (WhyUpdate::confronta ("0.2.0-beta", "0.2.0") == 0);
        expect (WhyUpdate::versioneCorrente().isNotEmpty());

        beginTest ("elenco aggiornamenti");
        const juce::String hash = juce::String::repeatedString ("a", 64);
        const juce::String ok = R"({"version":"0.3.0","notes":"n","windows":{"url":"https://x.it/a.exe","sha256":")" + hash
                                + R"(","size":10},"linux":{"url":"https://x.it/a.tar.gz","sha256":")" + hash + R"("}})";
        auto r = WhyUpdate::interpretaManifest (ok, "windows");
        expect (r.valido() && r.versione == "0.3.0" && r.dimensione == 10);
        expect (WhyUpdate::interpretaManifest (ok, "linux").url.endsWith (".tar.gz"));
        expect (! WhyUpdate::interpretaManifest (ok, "macos").valido());
        expect (! WhyUpdate::interpretaManifest ("non json", "windows").valido());
        expect (! WhyUpdate::interpretaManifest (ok.replace ("https://", "http://"), "windows").valido());   // solo https
        expect (! WhyUpdate::interpretaManifest (ok.replace (hash, "corto"), "windows").valido());

        beginTest ("sha256 del file scaricato");
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("whycremisi-sha-prova.bin");
        f.replaceWithText ("abc");
        const juce::String shaAbc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        expect (WhyUpdate::sha256Corrisponde (f, shaAbc));
        expect (WhyUpdate::sha256Corrisponde (f, shaAbc.toUpperCase()));
        expect (! WhyUpdate::sha256Corrisponde (f, hash));
        f.deleteFile();
        expect (! WhyUpdate::sha256Corrisponde (f, shaAbc));

        beginTest ("comando alla chiusura della DAW");
        const juce::File inst ("C:/Temp/it's/WhyCremisi-Setup.exe");
        const auto w = WhyUpdate::comandoAllaChiusura (inst, 1234, "windows");
        expect (w.contains ("Wait-Process -Id 1234") && w.contains ("/SILENT") && w.contains ("/FROMUPDATE") && w.contains ("it''s"));
        const auto l = WhyUpdate::comandoAllaChiusura (juce::File ("/tmp/x.tar.gz"), 77, "linux");
        expect (l.contains ("kill -0 77") && l.contains ("install.sh --silent"));

        beginTest ("flag aggiornamenti automatici");
        {
            const auto file = WhyUpdate::fileImpostazioni();
            const bool esisteva = file.existsAsFile();
            const auto prima = esisteva ? file.loadFileAsString() : juce::String();

            WhyUpdate::Impostazioni s;
            s.automatico = false;
            s.canale = "prova";
            expect (WhyUpdate::salvaImpostazioni (s));
            auto l = WhyUpdate::caricaImpostazioni();
            expect (! l.automatico && l.canale == "prova");
            s.automatico = true;
            WhyUpdate::salvaImpostazioni (s);
            if (juce::SystemStats::getEnvironmentVariable ("WHYCREMISI_NO_UPDATE", {}).isEmpty())
                expect (WhyUpdate::caricaImpostazioni().automatico);

            if (esisteva) file.replaceWithText (prima); else file.deleteFile();
        }

        beginTest ("controllo senza rete o con indirizzo sbagliato dice il motivo");
        {
            const auto c = WhyUpdate::controlla ("it");
            // senza pubblicazione o senza rete: non deve segnalare una nuova versione
            expect (! c.nuova);
            expect (c.riuscito || c.errore.isNotEmpty());
        }

        beginTest ("Reaper: collegamento automatico");
        {
            const juce::String ini = juce::String ("[REAPER]\r\nfoo=1\r\ncsurf_cnt=2\r\ncsurf_0=MCU 0 9 1 1 0\r\n")
                + "csurf_1=OSC \"OpenClaw Bridge\" 7 8000 \"127.0.0.1\" 9000 1024 10 \"\"\r\ncsurfrate=15\r\n[altro]\r\nx=1\r\n";
            expect (! ReaperSetup::giaCollegato (ini));
            const auto n = ReaperSetup::aggiungiSuperficie (ini);
            expect (n.contains ("csurf_cnt=3"));
            expect (n.contains ("csurf_2=OSC \"WhyCremisi\" 7 8000 \"127.0.0.1\" 9000 1024 10 \"\""));
            expect (n.indexOf ("csurf_2=") > n.indexOf ("csurf_1=") && n.indexOf ("csurf_2=") < n.indexOf ("[altro]"));
            expect (n.contains ("OpenClaw Bridge") && n.contains ("csurfrate=15") && n.contains ("\r\n"));
            expect (ReaperSetup::giaCollegato (n));
            expect (ReaperSetup::aggiungiSuperficie (n) == n);   // due volte non duplica

            const auto v = ReaperSetup::aggiungiSuperficie ("[REAPER]\nfoo=1\n[altro]\n");
            expect (v.contains ("csurf_cnt=1") && v.contains ("csurf_0=OSC \"WhyCremisi\""));
            expect (ReaperSetup::aggiungiSuperficie ("niente sezione") == "niente sezione");

            auto cartella = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("whycremisi-reaper-prova");
            cartella.deleteRecursively(); cartella.createDirectory();
            auto f = cartella.getChildFile ("reaper.ini");
            expect (! ReaperSetup::collega (f, false).ok);                      // Reaper mai avviato
            f.replaceWithText (ini);
            expect (! ReaperSetup::collega (f, true).ok);                       // Reaper aperto: non scrive
            expect (f.loadFileAsString() == ini);
            auto e = ReaperSetup::collega (f, false);
            expect (e.ok && ! e.eraGia && f.loadFileAsString().contains ("WhyCremisi"));
            expect (cartella.getChildFile ("reaper.ini.whycremisi-backup").loadFileAsString() == ini);
            expect (ReaperSetup::collega (f, false).eraGia);
            cartella.deleteRecursively();
        }
    }
};

static WhyUpdateTest whyUpdateTest;
