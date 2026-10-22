/*
  ==============================================================================
  Utf8Test.cpp
  WhyCremisi — le lettere accentate non devono diventare "Ã¨"

  Sintomo: "è" arrivava in chat come "Ã¨". Causa: byte UTF-8 letti uno alla
  volta in un juce::String (che li prende come Latin-1) o convertiti con
  juce::String(const char*). Questi test fissano il modo giusto di costruire
  le stringhe, compreso un carattere spezzato fra due letture di rete.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../AIProvider.h"
#include <string>

class Utf8Test : public juce::UnitTest
{
public:
    Utf8Test() : juce::UnitTest ("Utf8", "whycremisi") {}

    void runTest() override
    {
        const std::string frase = "Perch\xC3\xA9 \xC3\xA8 l'\xE2\x82\xAC pi\xC3\xB9 caro? S\xC3\xAC, \xC3\xA0 po\xC3\xB2.";
        const juce::String atteso = juce::String::fromUTF8 (frase.c_str());

        beginTest ("std::string -> juce::String e' UTF-8");
        expect (juce::String (frase) == atteso);
        expect (juce::String (frase).contains (juce::String::charToString (0x00E8)));
        expect (! juce::String (frase).contains ("\xC3\x83"));   // nessun "Ã"

        beginTest ("riga accumulata a byte e decodificata una volta sola");
        std::string bytes;
        for (char c : frase) bytes += c;
        expect (juce::String::fromUTF8 (bytes.data(), (int) bytes.size()) == atteso);

        beginTest ("carattere spezzato fra due blocchi di rete");
        const auto taglio = frase.find ("\xC3\xA9") + 1;      // a meta' di "é"
        std::string a = frase.substr (0, taglio), b = frase.substr (taglio);
        expect (juce::String::fromUTF8 ((a + b).data(), (int) (a + b).size()) == atteso);

        beginTest ("JSON con accenti: valore -> juce::String");
        const auto j = nlohmann::json::parse (std::string ("{\"content\":\"") + frase + "\"}");
        expect (juce::String (j["content"].get<std::string>()) == atteso);

        beginTest ("JSON con escape \u00e8");
        const auto j2 = nlohmann::json::parse ("{\"c\":\"caff\u00e8\"}");
        expect (juce::String (j2["c"].get<std::string>()) == juce::String::fromUTF8 ("caff\xC3\xA8"));

        beginTest ("andata e ritorno su file");
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("whycremisi-utf8.txt");
        f.replaceWithText (atteso);
        expect (f.loadFileAsString() == atteso);
        f.deleteFile();
    }
};

static Utf8Test utf8Test;
