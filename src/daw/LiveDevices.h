#pragma once

#include "../osc/OscHandler.h"
#include <juce_core/juce_core.h>
#include <vector>

/** Un parametro di un device di Live, come lo riporta AbletonOSC. */
struct LiveParam
{
    int          index = 0;
    juce::String name;
    double       value = 0.0;
    double       min   = 0.0;
    double       max   = 1.0;
};

/** Lettura dei plugin di una traccia di Live.

    AbletonOSC risponde ai comandi sui device con una lista dopo gli indici:
        /live/track/get/devices/name        [traccia]          -> (traccia, nome, nome, ...)
        /live/device/get/parameters/name    [traccia, device]  -> (traccia, device, nome, ...)
        /live/device/get/parameters/value   idem, valori reali
        /live/device/get/parameters/min     idem
        /live/device/get/parameters/max     idem
    Qui si interpretano quelle risposte, senza parlare con Live: cosi' si
    provano con risposte finte e non serve Ableton. */
namespace LiveDevices
{
    /** Toglie i `prefisso` indici iniziali e restituisce i nomi che seguono. */
    juce::StringArray nomiDaRisposta (const std::vector<OscHandler::OscArg>& args, int prefisso);

    /** Toglie i `prefisso` indici iniziali e restituisce i numeri che seguono. */
    std::vector<double> numeriDaRisposta (const std::vector<OscHandler::OscArg>& args, int prefisso);

    /** Unisce nomi, valori, minimi e massimi. Se le liste hanno lunghezze
        diverse usa la piu' corta: meglio meno parametri che parametri sbagliati. */
    std::vector<LiveParam> uniscimi (const juce::StringArray& nomi,
                                     const std::vector<double>& valori,
                                     const std::vector<double>& minimi,
                                     const std::vector<double>& massimi);

    /** Trova un device per nome (senza maiuscole; prima esatto, poi "contiene")
        o per numero scritto come testo. Ritorna -1 se non c'e' o se e' ambiguo;
        `errore` dice perche'. */
    int trovaPerNome (const juce::StringArray& nomi, const juce::String& cerca, juce::String& errore);

    /** Come sopra, ma sui parametri. */
    int trovaParametro (const std::vector<LiveParam>& params, const juce::String& cerca, juce::String& errore);

    /** 0..1 -> valore reale del parametro (limitato), e il contrario. */
    double daNormalizzato (const LiveParam& p, double n);
    double aNormalizzato (const LiveParam& p, double reale);

    /** Il valore e' arrivato dove serviva? Tolleranza 1% dell'escursione. */
    bool uguali (const LiveParam& p, double a, double b);
}
