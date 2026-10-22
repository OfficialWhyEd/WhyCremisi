#include "WhyUpdate.h"

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <unistd.h>
#endif

#ifndef WHYCREMISI_VERSION
 #define WHYCREMISI_VERSION "0.0.0"
#endif

static const char* kRepo = "OfficialWhyEd/WhyCremisi";

juce::String WhyUpdate::versioneCorrente() { return WHYCREMISI_VERSION; }

juce::String WhyUpdate::piattaforma()
{
   #if JUCE_WINDOWS
    return "windows";
   #elif JUCE_MAC
    return "macos";
   #else
    return "linux";
   #endif
}

int WhyUpdate::confronta (const juce::String& a, const juce::String& b)
{
    auto parti = [] (const juce::String& s)
    {
        juce::StringArray p;
        p.addTokens (s.upToFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false).trim(), ".", {});
        return p;
    };
    const auto pa = parti (a), pb = parti (b);
    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const int x = i < pa.size() ? pa[i].getIntValue() : 0;
        const int y = i < pb.size() ? pb[i].getIntValue() : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

WhyUpdate::Rilascio WhyUpdate::interpretaManifest (const juce::String& json, const juce::String& piatt)
{
    Rilascio r;
    const auto v = juce::JSON::parse (json);
    if (! v.isObject()) return r;
    const auto p = v.getProperty (piatt, {});
    if (! p.isObject()) return r;
    r.versione = v.getProperty ("version", {}).toString().trim();
    r.note = v.getProperty ("notes", {}).toString();
    r.url = p.getProperty ("url", {}).toString().trim();
    r.sha256 = p.getProperty ("sha256", {}).toString().trim().toLowerCase();
    r.dimensione = (juce::int64) p.getProperty ("size", 0);
    if (! r.valido()) return {};
    return r;
}

juce::String WhyUpdate::urlManifest (const juce::String& canale)
{
    const auto sovr = juce::SystemStats::getEnvironmentVariable ("WHYCREMISI_UPDATE_URL", {});
    if (sovr.isNotEmpty()) return sovr;
    return "https://github.com/" + juce::String (kRepo) + "/releases/download/"
           + (canale.isEmpty() ? juce::String ("beta") : canale) + "/latest.json";
}

bool WhyUpdate::sha256Corrisponde (const juce::File& f, const juce::String& atteso)
{
    if (! f.existsAsFile() || atteso.length() != 64) return false;
    juce::FileInputStream in (f);
    if (! in.openedOk()) return false;
    return juce::SHA256 (in).toHexString().equalsIgnoreCase (atteso);
}

juce::String WhyUpdate::comandoAllaChiusura (const juce::File& installer, int pid, const juce::String& piatt)
{
    const auto f = installer.getFullPathName();
    if (piatt == "windows")
        return "powershell -NoProfile -WindowStyle Hidden -Command \"try { Wait-Process -Id " + juce::String (pid)
               + " -ErrorAction SilentlyContinue } catch {}; Start-Process -FilePath '" + f.replace ("'", "''")
               + "' -ArgumentList '/SILENT','/SUPPRESSMSGBOXES','/NORESTART','/FROMUPDATE'\"";
    // linux: il pacchetto e' un .tar.gz con install.sh dentro
    return "sh -c 'while kill -0 " + juce::String (pid) + " 2>/dev/null; do sleep 2; done; "
           "d=$(mktemp -d) && tar -xzf \"" + f + "\" -C \"$d\" && sh \"$d\"/*/install.sh --silent; rm -rf \"$d\"'";
}

juce::File WhyUpdate::fileImpostazioni()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("WhyCremisi").getChildFile ("aggiornamenti.json");
}

WhyUpdate::Impostazioni WhyUpdate::caricaImpostazioni()
{
    Impostazioni s;
    const auto v = juce::JSON::parse (fileImpostazioni());
    if (v.isObject())
    {
        if (v.hasProperty ("auto")) s.automatico = (bool) v.getProperty ("auto", true);
        const auto c = v.getProperty ("channel", {}).toString().trim();
        if (c.isNotEmpty()) s.canale = c;
    }
    if (juce::SystemStats::getEnvironmentVariable ("WHYCREMISI_NO_UPDATE", {}).isNotEmpty())
        s.automatico = false;
    return s;
}

bool WhyUpdate::salvaImpostazioni (const Impostazioni& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("auto", s.automatico);
    o->setProperty ("channel", s.canale);
    const auto f = fileImpostazioni();
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (juce::JSON::toString (juce::var (o)));
}

WhyUpdate::Controllo WhyUpdate::controlla (const juce::String& lingua)
{
    Controllo c;
    const bool it = lingua != "en";
    const auto url = urlManifest (caricaImpostazioni().canale);
    int stato = 0;
    auto in = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                     .withConnectionTimeoutMs (8000).withStatusCode (&stato)
                                                     .withNumRedirectsToFollow (5));
    if (in == nullptr || stato == 0)
    {
        c.errore = it ? "Non riesco a controllare gli aggiornamenti: nessuna connessione." : "Cannot check for updates: no connection.";
        return c;
    }
    if (stato != 200)
    {
        c.errore = it ? "Nessun aggiornamento pubblicato (codice " + juce::String (stato) + ")."
                      : "No update published (code " + juce::String (stato) + ").";
        return c;
    }
    c.rilascio = interpretaManifest (in->readEntireStreamAsString(), piattaforma());
    if (! c.rilascio.valido())
    {
        c.errore = it ? "L'elenco aggiornamenti non è leggibile per questa piattaforma." : "The update list is not readable for this platform.";
        return c;
    }
    c.riuscito = true;
    c.nuova = confronta (versioneCorrente(), c.rilascio.versione) < 0;
    return c;
}

bool WhyUpdate::scarica (const Rilascio& r, juce::File& dest, juce::String& errore,
                         const std::atomic<bool>* stop, std::function<void (int)> progresso)
{
    if (! r.valido()) { errore = "manifest non valido"; return false; }
    const auto cartella = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WhyCremisi-update");
    cartella.createDirectory();
    dest = cartella.getChildFile (juce::URL (r.url).getFileName());
    if (sha256Corrisponde (dest, r.sha256)) { if (progresso) progresso (100); return true; }
    dest.deleteFile();

    auto in = juce::URL (r.url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                       .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (8));
    if (in == nullptr) { errore = "download non avviato"; return false; }
    juce::FileOutputStream out (dest);
    if (! out.openedOk()) { errore = "non posso scrivere in " + cartella.getFullPathName(); return false; }
    const auto totale = in->getTotalLength() > 0 ? in->getTotalLength() : r.dimensione;
    juce::int64 letti = 0;
    char buf[64 * 1024];
    int ultimo = -1;
    while (! in->isExhausted())
    {
        if (stop != nullptr && stop->load()) { out.flush(); dest.deleteFile(); errore = "interrotto"; return false; }
        const int n = in->read (buf, (int) sizeof (buf));
        if (n < 0) { errore = "download interrotto"; dest.deleteFile(); return false; }
        if (n == 0) break;
        out.write (buf, (size_t) n);
        letti += n;
        if (progresso && totale > 0)
        {
            const int p = (int) (letti * 100 / totale);
            if (p != ultimo) { ultimo = p; progresso (p); }
        }
    }
    out.flush();
    if (! sha256Corrisponde (dest, r.sha256))
    {
        dest.deleteFile();
        errore = "il file scaricato non corrisponde alla firma SHA-256: scartato";
        return false;
    }
    return true;
}

bool WhyUpdate::installaAllaChiusura (const juce::File& installer, juce::String& errore)
{
    if (! installer.existsAsFile()) { errore = "installer non trovato"; return false; }
   #if JUCE_WINDOWS
    const int pid = (int) GetCurrentProcessId();
    auto cmd = comandoAllaChiusura (installer, pid, "windows");
    STARTUPINFOW si {}; si.cb = sizeof (si);
    PROCESS_INFORMATION pi {};
    std::wstring w = cmd.toWideCharPointer();
    if (! CreateProcessW (nullptr, w.data(), nullptr, nullptr, FALSE,
                          CREATE_NO_WINDOW | DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi))
    { errore = "non riesco ad avviare l'aggiornamento"; return false; }
    CloseHandle (pi.hProcess); CloseHandle (pi.hThread);
    return true;
   #else
    const int pid = (int) getpid();
    const auto cmd = "nohup " + comandoAllaChiusura (installer, pid, "linux") + " >/dev/null 2>&1 &";
    if (std::system (cmd.toRawUTF8()) != 0) { errore = "non riesco ad avviare l'aggiornamento"; return false; }
    return true;
   #endif
}
