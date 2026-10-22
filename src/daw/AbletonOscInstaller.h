#pragma once
#include <juce_core/juce_core.h>

// Installa AbletonOSC (MIT, github.com/ideoforms/AbletonOSC) fra i Remote
// Scripts di Live. E' codice di terzi che gira dentro Live: si avvia solo
// da un'azione esplicita dell'utente, mai da solo.
//
// Resta un passo che nessuno puo' fare al posto dell'utente: in Live,
// Preferenze > Link, Tempo & MIDI > Control Surface = AbletonOSC.
// Non esiste un file di configurazione che lo attivi (verificato nella
// ricerca 13): il plugin lo dice, non finge.
class AbletonOscInstaller
{
public:
    // Commit fissato: si installa sempre lo stesso codice, riletto prima.
    static juce::String pinnedCommit()  { return "0ca68214bd62c9b5cb641ca34006cfd70ba94430"; }
    static juce::String downloadUrl()   { return "https://github.com/ideoforms/AbletonOSC/archive/" + pinnedCommit() + ".zip"; }

    // Cartella Remote Scripts della User Library (vuota se non si trova la home).
    static juce::File remoteScriptsDir();

    // C'e' gia' una cartella AbletonOSC completa in quella cartella?
    static bool isInstalled (const juce::File& remoteScripts);

    struct Result { bool ok = false; juce::String message; int filesWritten = 0; };

    // Estrae lo zip di GitHub (cartella radice "AbletonOSC-<commit>/") in
    // <remoteScripts>/AbletonOSC. Rifiuta percorsi che escono dalla cartella
    // e zip senza i file che AbletonOSC deve avere.
    static Result installFromZip (juce::InputStream& zipData, const juce::File& remoteScripts);

    // Scarica (bloccante, da thread di lavoro) e installa.
    static Result downloadAndInstall (const juce::File& remoteScripts);
};
