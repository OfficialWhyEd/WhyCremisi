#pragma once

#include <juce_core/juce_core.h>
#include "DawDetector.h"

// ── Cosa una DAW sa fare ───────────────────────────────────────────
//
//  Il contratto dei tool e' uno solo; le capacita' no. Questa tabella dice,
//  per ogni tool, su quali DAW ha senso offrirlo.
//
//  Perche' serve: senza, il modello riceve anche i tool che su quella DAW
//  non esistono (per esempio i device di Live mentre gira REAPER), li chiama,
//  e ottiene messaggi che parlano di un'altra DAW. Il risultato sembra un
//  guasto di comunicazione ed e' invece un contratto che promette troppo.
namespace DawCaps
{
    // Elenco completo dei tool validi per la DAW indicata.
    juce::StringArray toolsFor (DawType daw);

    // Il tool ha senso su questa DAW?
    bool supports (const juce::String& toolName, DawType daw);

    // Perche' no, in una riga utile al modello.
    juce::String reasonFor (const juce::String& toolName, DawType daw);

    /** Tutti i tool che WY conosce, su qualunque DAW (sospesi inclusi).
        Serve per dire al modello cosa esiste in generale, non solo qui. */
    juce::StringArray allTools();

    /** I tool che esistono ma NON sono disponibili su questa DAW. */
    juce::StringArray unavailableFor (DawType daw);

    // Blocco di testo da mettere nel system prompt: quale DAW e' aperta,
    // cosa puo' usare ORA, cosa esiste altrove e cosa e' sospeso.
    juce::String promptBlock (DawType daw);
    juce::String promptBlock (DawType daw, const juce::StringArray& chiamabili);

    // Nome leggibile della DAW (riusa DawDetector).
    juce::String name (DawType daw);
}
