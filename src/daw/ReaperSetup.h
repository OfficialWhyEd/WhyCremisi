#pragma once
#include <juce_core/juce_core.h>

// Collega Reaper a WhyCremisi senza passi a mano: aggiunge in reaper.ini la superficie
// di controllo OSC "WhyCremisi" (Reaper ascolta su 8000, risponde al plugin su 9000).
// Formato verificato su un reaper.ini reale: csurf_N=OSC "nome" modo portaLocale "ip" portaDevice maxPacket buffer "".
// Reaper deve essere CHIUSO (altrimenti riscrive il file alla chiusura): si fa una copia di sicurezza.
struct ReaperSetup
{
    static constexpr int portaReaper = 8000;   // dove ascolta Reaper
    static constexpr int portaPlugin = 9000;   // dove ascolta il plugin

    static juce::File cartellaReaper();        // %APPDATA%/REAPER, ~/.config/REAPER, ~/Library/Application Support/REAPER
    static juce::File fileIni();
    static bool inEsecuzione();                // Reaper e' aperto adesso?

    // Parti pure
    static bool giaCollegato (const juce::String& ini);
    static juce::String aggiungiSuperficie (const juce::String& ini);  // ritorna l'ini nuovo (uguale se gia' presente)

    // Con il disco: ok=true se collegato (anche se lo era gia'); messaggio dice cosa e' successo
    struct Esito { bool ok = false; bool eraGia = false; juce::String messaggio; };
    static Esito collega (const juce::File& ini, bool reaperAperto);
    static Esito collega();
};
