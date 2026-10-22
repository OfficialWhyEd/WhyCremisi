#include "AbletonOscInstaller.h"

juce::File AbletonOscInstaller::remoteScriptsDir()
{
   #if JUCE_WINDOWS
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Ableton/User Library/Remote Scripts");
   #elif JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("Ableton/User Library/Remote Scripts");
   #else
    return {};
   #endif
}

bool AbletonOscInstaller::isInstalled (const juce::File& remoteScripts)
{
    const auto dir = remoteScripts.getChildFile ("AbletonOSC");
    return dir.getChildFile ("__init__.py").existsAsFile()
        && dir.getChildFile ("manager.py").existsAsFile();
}

AbletonOscInstaller::Result AbletonOscInstaller::installFromZip (juce::InputStream& zipData,
                                                                const juce::File& remoteScripts)
{
    Result r;
    if (remoteScripts == juce::File())
    {
        r.message = "Cartella Remote Scripts di Live non trovata su questo sistema.";
        return r;
    }

    juce::ZipFile zip (zipData);
    const auto target = remoteScripts.getChildFile ("AbletonOSC");
    const auto targetPath = target.getFullPathName();

    // Prima si controlla tutto, poi si scrive: niente cartelle a meta'.
    bool haveInit = false, haveManager = false;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto name = zip.getEntry (i)->filename;
        const auto slash = name.indexOfChar ('/');
        const auto rel = slash >= 0 ? name.substring (slash + 1) : juce::String();
        if (rel == "__init__.py") haveInit = true;
        if (rel == "manager.py")  haveManager = true;
    }
    if (! haveInit || ! haveManager)
    {
        r.message = "Lo zip non contiene AbletonOSC (mancano __init__.py o manager.py).";
        return r;
    }
    if (target.exists())
    {
        r.message = "AbletonOSC e' gia' presente: non lo sovrascrivo.";
        return r;
    }

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* e = zip.getEntry (i);
        const auto slash = e->filename.indexOfChar ('/');
        if (slash < 0) continue;
        const auto rel = e->filename.substring (slash + 1);
        if (rel.isEmpty() || rel.endsWithChar ('/')) continue;
        // Solo i file che servono a Live; i test e le guide restano fuori.
        if (rel.startsWith ("tests/") || rel.startsWith (".github/") || rel.startsWith ("client/")) continue;

        const auto dest = target.getChildFile (rel);
        if (! dest.getFullPathName().startsWith (targetPath + juce::File::getSeparatorString()))
        {
            target.deleteRecursively();
            r.message = "Percorso non valido nello zip: " + rel;
            return r;
        }
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
        if (in == nullptr || ! dest.getParentDirectory().createDirectory())
        {
            target.deleteRecursively();
            r.message = "Non riesco a leggere o scrivere: " + rel;
            return r;
        }
        juce::FileOutputStream out (dest);
        if (out.failedToOpen() || out.writeFromInputStream (*in, -1) < 0)
        {
            target.deleteRecursively();
            r.message = "Scrittura fallita: " + rel;
            return r;
        }
        ++r.filesWritten;
    }

    if (! isInstalled (remoteScripts))
    {
        target.deleteRecursively();
        r.message = "Installazione incompleta, annullata.";
        return r;
    }
    r.ok = true;
    r.message = "AbletonOSC copiato in " + target.getFullPathName()
              + ". Ultimo passo in Live: Preferenze > Link, Tempo & MIDI > Control Surface = AbletonOSC.";
    return r;
}

AbletonOscInstaller::Result AbletonOscInstaller::downloadAndInstall (const juce::File& remoteScripts)
{
    Result r;
    auto stream = juce::URL (downloadUrl()).createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (5));
    if (stream == nullptr)
    {
        r.message = "Download non riuscito: controlla la connessione.";
        return r;
    }
    juce::MemoryBlock data;
    stream->readIntoMemoryBlock (data);
    juce::MemoryInputStream mem (data, false);
    return installFromZip (mem, remoteScripts);
}
