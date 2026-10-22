/*
  ==============================================================================
  ToolCallParsingTest.cpp
  PercheCremisi — test delle tool call in streaming

  Qui si prova la parte che finora non era provata da niente: gli
  accumulatori che ricompongono le tool call dai frammenti, il parser dei
  marcatori testuali e la traduzione dello storico nei dialetti dei
  provider. Niente rete, niente server: si danno in pasto chunk finti.

  Il difetto che questi test sorvegliano: il modello chiedeva uno
  strumento, il plugin lo stampava come testo e non lo eseguiva.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../AIProvider.h"
#include "../AiEngine.h"

class ToolCallParsingTest : public juce::UnitTest
{
public:
    ToolCallParsingTest() : juce::UnitTest ("ToolCallParsing", "whycremisi") {}

    void runTest() override
    {
        testOpenAIFrammenti();
        testOpenAIIndexMancante();
        testOpenAINomeCheCresce();
        testOpenAIJsonInvalido();
        testAnthropicInputJsonDelta();
        testGeminiArgomentiAPezzi();
        testMarcatoriTestuali();
        testStoricoAnthropic();
        testStoricoGemini();
    }

private:
    static juce::String dump (const nlohmann::json& j) { return juce::String (j.dump()); }

    // ── OpenAI: gli argomenti arrivano a pezzi e vanno accumulati ────
    void testOpenAIFrammenti()
    {
        beginTest ("OpenAI: argomenti in tre frammenti diventano un JSON valido");

        ToolCallAccumulator acc;
        acc.addChunk (0, "call_1", "daw.setVolume", "{\"track\":");
        acc.addChunk (0, "", "", "2,\"db\":");
        acc.addChunk (0, "", "", "-6}");

        auto calls = acc.finish();
        expectEquals ((int) calls.size(), 1);
        expectEquals (calls[0].id, juce::String ("call_1"));
        expectEquals (calls[0].name, juce::String ("daw.setVolume"));
        expectEquals ((int) calls[0].arguments["track"], 2);
        expectEquals ((int) calls[0].arguments["db"], -6);
    }

    void testOpenAIIndexMancante()
    {
        beginTest ("OpenAI: chunk senza index finisce nella call 0");

        ToolCallAccumulator acc;
        acc.addChunk (0, "", "dsp.getState", "{}");
        acc.addChunk (0, "", "", "");          // chunk vuoto: non deve rompere
        auto calls = acc.finish();

        expectEquals ((int) calls.size(), 1);
        expectEquals (calls[0].name, juce::String ("dsp.getState"));
        expect (calls[0].arguments.is_object());
    }

    void testOpenAINomeCheCresce()
    {
        beginTest ("OpenAI/llama.cpp: il nome puo' arrivare che cresce");

        ToolCallAccumulator acc;
        acc.addChunk (0, "", "mi", "");
        acc.addChunk (0, "", "mix.analyze", "");
        acc.addChunk (0, "", "", "{}");

        auto calls = acc.finish();
        expectEquals ((int) calls.size(), 1);
        expectEquals (calls[0].name, juce::String ("mix.analyze"));
    }

    void testOpenAIJsonInvalido()
    {
        beginTest ("OpenAI: JSON invalido non si butta via, si segnala");

        ToolCallAccumulator acc;
        acc.addChunk (0, "call_9", "mix.analyze", "{\"sorgente\":");

        auto calls = acc.finish();
        expectEquals ((int) calls.size(), 1);
        expect (calls[0].arguments.is_object());
        expect (calls[0].arguments.contains ("__raw_arguments"),
                "gli argomenti grezzi devono restare leggibili");
    }

    // ── Anthropic: input_json_delta e fine blocco ───────────────────
    void testAnthropicInputJsonDelta()
    {
        beginTest ("Anthropic: input_json_delta compone gli argomenti");

        AnthropicToolAccumulator acc;
        acc.startBlock (1, "toolu_1", "daw.mute");
        acc.addJsonFragment (1, "{\"track\":");
        acc.addJsonFragment (1, " 1}");
        acc.stopBlock (1);

        auto calls = acc.finish();
        expectEquals ((int) calls.size(), 1);
        expectEquals (calls[0].id, juce::String ("toolu_1"));
        expectEquals (calls[0].name, juce::String ("daw.mute"));
        expectEquals ((int) calls[0].arguments["track"], 1);
    }

    // ── Gemini: gli argomenti possono essere una stringa a pezzi ────
    void testGeminiArgomentiAPezzi()
    {
        beginTest ("Gemini: argomenti a frammenti di stringa");

        GeminiToolAccumulator acc;
        acc.addCall ("daw.setTempo", nlohmann::json (std::string ("{\"bpm\":")));
        acc.addArgumentsFragment ("daw.setTempo", "100}");

        auto calls = acc.finish();
        expectEquals ((int) calls.size(), 1);
        expectEquals (calls[0].name, juce::String ("daw.setTempo"));
        expectEquals ((int) calls[0].arguments["bpm"], 100);
    }

    // ── Marcatori scritti come testo (rete di sicurezza) ────────────
    void testMarcatoriTestuali()
    {
        beginTest ("Marcatori [TOOL_CALL:...] nel testo");

        const juce::String testo =
            "Certo. [TOOL_CALL:mix.analyze {\"sorgente\":\"master\"}] "
            "poi [TOOL_CALL:daw.mute {\"track\":1}]";

        auto calls = AiEngine::parseToolMarkers (testo);
        expectEquals ((int) calls.size(), 2);
        expectEquals (calls[0].name, juce::String ("mix.analyze"));
        expectEquals (juce::String (calls[0].arguments["sorgente"].get<std::string>()), juce::String ("master"));
        expectEquals (calls[1].name, juce::String ("daw.mute"));
        expectEquals ((int) calls[1].arguments["track"], 1);
    }

    // ── Storico canonico per dialetto ──────────────────────────────
    void testStoricoAnthropic()
    {
        beginTest ("Anthropic: tool_calls -> tool_use, risultati -> tool_result");

        nlohmann::json storico = nlohmann::json::array();
        storico.push_back ({{"role", "user"}, {"content", "alza il volume"}});
        storico.push_back ({{"role", "assistant"},
                            {"content", ""},
                            {"tool_calls", nlohmann::json::array ({
                                {{"id", "call_1"}, {"type", "function"},
                                 {"function", {{"name", "daw.setVolume"},
                                               {"arguments", "{\"track\":2,\"db\":-6}"}}}}})}});
        storico.push_back ({{"role", "tool"}, {"tool_call_id", "call_1"},
                            {"name", "daw.setVolume"}, {"content", "traccia 2 a -6 dB"}});

        auto out = anthropicMessagesFromNeutral (juce::String (storico.dump()));

        expectEquals ((int) out.size(), 3);
        expectEquals (juce::String (out[1]["role"].get<std::string>()), juce::String ("assistant"));
        expect (out[1]["content"].is_array());
        expectEquals (juce::String (out[1]["content"][0]["type"].get<std::string>()), juce::String ("tool_use"));
        expectEquals (juce::String (out[1]["content"][0]["id"].get<std::string>()), juce::String ("call_1"));
        expectEquals ((int) out[1]["content"][0]["input"]["db"], -6);

        // Il risultato sta in un messaggio utente, come vuole Claude.
        expectEquals (juce::String (out[2]["role"].get<std::string>()), juce::String ("user"));
        expectEquals (juce::String (out[2]["content"][0]["type"].get<std::string>()), juce::String ("tool_result"));
        expectEquals (juce::String (out[2]["content"][0]["tool_use_id"].get<std::string>()), juce::String ("call_1"));
    }

    void testStoricoGemini()
    {
        beginTest ("Gemini: tool_calls -> functionCall, risultati -> functionResponse");

        nlohmann::json storico = nlohmann::json::array();
        storico.push_back ({{"role", "assistant"},
                            {"content", ""},
                            {"tool_calls", nlohmann::json::array ({
                                {{"id", "call_1"}, {"type", "function"},
                                 {"function", {{"name", "daw.setTempo"},
                                               {"arguments", "{\"bpm\":100}"}}}}})}});
        storico.push_back ({{"role", "tool"}, {"tool_call_id", "call_1"},
                            {"name", "daw.setTempo"}, {"content", "tempo 100"}});

        auto out = geminiContentsFromNeutral (juce::String (storico.dump()), "grazie");

        expect (out.is_array());
        expectEquals ((int) out.size(), 3);   // model, user(functionResponse), user(domanda)
        expectEquals (juce::String (out[0]["role"].get<std::string>()), juce::String ("model"));
        expectEquals (juce::String (out[0]["parts"][0]["functionCall"]["name"].get<std::string>()),
                      juce::String ("daw.setTempo"));
        expectEquals ((int) out[0]["parts"][0]["functionCall"]["args"]["bpm"], 100);
        expectEquals (juce::String (out[1]["parts"][0]["functionResponse"]["name"].get<std::string>()),
                      juce::String ("daw.setTempo"));
        expectEquals (juce::String (out[2]["parts"][0]["text"].get<std::string>()), juce::String ("grazie"));
    }
};

static ToolCallParsingTest toolCallParsingTest;
