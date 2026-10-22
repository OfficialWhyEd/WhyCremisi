#include "ReaperSetup.h"

juce::File ReaperSetup::cartellaReaper()
{
   #if JUCE_WINDOWS
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("REAPER");
   #elif JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("REAPER");
   #else
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/REAPER");
   #endif
}

juce::File ReaperSetup::fileIni() { return cartellaReaper().getChildFile ("reaper.ini"); }

bool ReaperSetup::inEsecuzione()
{
    juce::ChildProcess p;
   #if JUCE_WINDOWS
    if (! p.start ("tasklist /FI \"IMAGENAME eq reaper.exe\" /NH")) return false;
    return p.readAllProcessOutput().containsIgnoreCase ("reaper.exe");
   #else
    if (! p.start ("pgrep -x reaper")) return false;
    return p.readAllProcessOutput().trim().isNotEmpty();
   #endif
}

bool ReaperSetup::giaCollegato (const juce::String& ini)
{
    for (auto riga : juce::StringArray::fromLines (ini))
        if (riga.trim().startsWith ("csurf_") && riga.contains (" OSC \"WhyCremisi\"")) return true;
        else if (riga.trim().startsWith ("csurf_") && riga.contains ("=OSC \"WhyCremisi\"")) return true;
    return false;
}

juce::String ReaperSetup::aggiungiSuperficie (const juce::String& ini)
{
    if (giaCollegato (ini)) return ini;
    const bool crlf = ini.contains ("\r\n");
    auto righe = juce::StringArray::fromLines (ini);

    int cnt = 0, posCnt = -1, ultimoCsurf = -1, inizioSezione = -1, fineSezione = righe.size();
    for (int i = 0; i < righe.size(); ++i)
    {
        const auto r = righe[i].trim();
        if (r.startsWith ("[") && r.endsWith ("]"))
        {
            if (r.equalsIgnoreCase ("[REAPER]")) inizioSezione = i;
            else if (inizioSezione >= 0) { fineSezione = i; break; }
        }
        if (inizioSezione >= 0)
        {
            if (r.startsWith ("csurf_cnt=")) { cnt = r.fromFirstOccurrenceOf ("=", false, false).getIntValue(); posCnt = i; }
            else if (r.startsWith ("csurf_") && r.containsChar ('=') && ! r.startsWith ("csurfrate")) ultimoCsurf = i;
        }
    }
    if (inizioSezione < 0) return ini;   // niente sezione [REAPER]: non invento un file che Reaper non ha

    const auto riga = "csurf_" + juce::String (cnt) + "=OSC \"WhyCremisi\" 7 " + juce::String (portaReaper)
                      + " \"127.0.0.1\" " + juce::String (portaPlugin) + " 1024 10 \"\"";
    if (posCnt >= 0)
    {
        righe.set (posCnt, "csurf_cnt=" + juce::String (cnt + 1));
        const int dopo = juce::jmax (ultimoCsurf, posCnt);
        righe.insert (dopo + 1, riga);
    }
    else
    {
        righe.insert (fineSezione, riga);
        righe.insert (fineSezione, "csurf_cnt=1");   // cnt era 0: la riga appena messa e' csurf_0
    }
    return righe.joinIntoString (crlf ? "\r\n" : "\n") + (crlf ? "\r\n" : "\n");
}

ReaperSetup::Esito ReaperSetup::collega (const juce::File& ini, bool reaperAperto)
{
    Esito e;
    if (! ini.existsAsFile())
    {
        e.messaggio = "Reaper non è stato ancora avviato su questo computer (manca reaper.ini). Aprilo una volta, chiudilo e riprova.";
        return e;
    }
    const auto testo = ini.loadFileAsString();
    if (giaCollegato (testo)) { e.ok = e.eraGia = true; e.messaggio = "Reaper era già collegato."; return e; }
    if (reaperAperto)
    {
        e.messaggio = "Chiudi Reaper, poi premi di nuovo Collega: se lo scrivo mentre è aperto, Reaper cancella la modifica.";
        return e;
    }
    const auto nuovo = aggiungiSuperficie (testo);
    if (nuovo == testo) { e.messaggio = "Non ho trovato dove scrivere in reaper.ini."; return e; }
    ini.copyFileTo (ini.getSiblingFile ("reaper.ini.whycremisi-backup"));
    if (! ini.replaceWithText (nuovo)) { e.messaggio = "Non riesco a scrivere reaper.ini."; return e; }
    e.ok = true;
    e.messaggio = "Reaper collegato. Riaprilo: la superficie OSC \"WhyCremisi\" è già attiva (copia di sicurezza: reaper.ini.whycremisi-backup).";
    return e;
}

ReaperSetup::Esito ReaperSetup::collega() { return collega (fileIni(), inEsecuzione()); }
