#include "LiveDevices.h"
#include <cmath>

namespace LiveDevices
{
    juce::StringArray nomiDaRisposta (const std::vector<OscHandler::OscArg>& args, int prefisso)
    {
        juce::StringArray nomi;
        for (size_t i = (size_t) juce::jmax (0, prefisso); i < args.size(); ++i)
            if (args[i].isString)
                nomi.add (args[i].text);
        return nomi;
    }

    std::vector<double> numeriDaRisposta (const std::vector<OscHandler::OscArg>& args, int prefisso)
    {
        std::vector<double> v;
        for (size_t i = (size_t) juce::jmax (0, prefisso); i < args.size(); ++i)
            if (! args[i].isString)
                v.push_back (args[i].number);
        return v;
    }

    std::vector<LiveParam> uniscimi (const juce::StringArray& nomi, const std::vector<double>& valori,
                                     const std::vector<double>& minimi, const std::vector<double>& massimi)
    {
        const size_t n = std::min ({ (size_t) nomi.size(), valori.size(), minimi.size(), massimi.size() });
        std::vector<LiveParam> out;
        for (size_t i = 0; i < n; ++i)
            out.push_back ({ (int) i, nomi[(int) i], valori[i], minimi[i], massimi[i] });
        return out;
    }

    static int trova (const juce::StringArray& nomi, const juce::String& cercaIn, juce::String& errore,
                      const juce::String& cosa)
    {
        const auto cerca = cercaIn.trim();
        if (cerca.isEmpty()) { errore = "Manca il nome o il numero del " + cosa + "."; return -1; }

        if (cerca.containsOnly ("0123456789"))
        {
            const int n = cerca.getIntValue();
            if (n >= 0 && n < nomi.size()) return n;
            errore = cosa + " " + cerca + " non esiste: ce ne sono " + juce::String (nomi.size()) + " (da 0).";
            return -1;
        }

        for (int i = 0; i < nomi.size(); ++i)
            if (nomi[i].equalsIgnoreCase (cerca)) return i;

        juce::Array<int> simili;
        for (int i = 0; i < nomi.size(); ++i)
            if (nomi[i].containsIgnoreCase (cerca)) simili.add (i);

        if (simili.size() == 1) return simili[0];
        if (simili.isEmpty())
        {
            errore = "Nessun " + cosa + " chiamato '" + cerca + "'. Ci sono: " + nomi.joinIntoString (", ") + ".";
            return -1;
        }
        juce::StringArray candidati;
        for (int i : simili) candidati.add (juce::String (i) + " " + nomi[i]);
        errore = "'" + cerca + "' e' ambiguo, puo' essere: " + candidati.joinIntoString (", ") + ". Indica il numero.";
        return -1;
    }

    int trovaPerNome (const juce::StringArray& nomi, const juce::String& cerca, juce::String& errore)
    {
        return trova (nomi, cerca, errore, "device");
    }

    int trovaParametro (const std::vector<LiveParam>& params, const juce::String& cerca, juce::String& errore)
    {
        juce::StringArray nomi;
        for (const auto& p : params) nomi.add (p.name);
        return trova (nomi, cerca, errore, "parametro");
    }

    double daNormalizzato (const LiveParam& p, double n)
    {
        return p.min + juce::jlimit (0.0, 1.0, n) * (p.max - p.min);
    }

    double aNormalizzato (const LiveParam& p, double reale)
    {
        const double span = p.max - p.min;
        return span > 0.0 ? juce::jlimit (0.0, 1.0, (reale - p.min) / span) : 0.0;
    }

    bool uguali (const LiveParam& p, double a, double b)
    {
        const double span = std::abs (p.max - p.min);
        return std::abs (a - b) <= juce::jmax (1e-6, 0.01 * span);
    }
}
