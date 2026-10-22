/*
  ==============================================================================
  ToolValidationTest.cpp
  WhyCremisi — gli argomenti delle tool call si controllano prima di eseguire

  Prima executeTool lanciava l'azione con qualunque argomento: un modello
  piccolo che sbagliava un tipo o un valore faceva eseguire l'azione sbagliata
  e il risultato risultava "riuscito". Ora l'errore torna al modello con il
  motivo, e l'azione non parte.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../ToolRegistry.h"

class ToolValidationTest : public juce::UnitTest
{
public:
    ToolValidationTest() : juce::UnitTest ("ToolValidation", "whycremisi") {}

    void runTest() override
    {
        ToolDefinition def;
        def.name = "prova";
        ToolParameter a; a.name = "track"; a.type = "number"; a.required = true; a.minVal = 0.0f; a.maxVal = 7.0f;
        ToolParameter b; b.name = "mode"; b.type = "string"; b.enumValues = { "solo", "mute" };
        ToolParameter c; c.name = "on"; c.type = "boolean";
        ToolParameter d; d.name = "value"; d.type = "number";
        def.parameters = { a, b, c, d };

        beginTest ("Argomenti validi passano");
        {
            nlohmann::json j = { { "track", 3 }, { "mode", "solo" }, { "on", true } };
            expect (ToolRegistry::validateArguments (def, j).isEmpty());
        }

        beginTest ("Obbligatorio mancante");
        {
            nlohmann::json j = { { "mode", "solo" } };
            expect (ToolRegistry::validateArguments (def, j).contains ("track"));
        }

        beginTest ("Tipo sbagliato");
        {
            nlohmann::json j = { { "track", "abc" } };
            expect (ToolRegistry::validateArguments (def, j).isNotEmpty());
            nlohmann::json k = { { "track", 1 }, { "on", 5 } };
            expect (ToolRegistry::validateArguments (def, k).isNotEmpty());
        }

        beginTest ("Stringhe numeriche e booleane vengono convertite");
        {
            nlohmann::json j = { { "track", "5" }, { "on", "true" } };
            expect (ToolRegistry::validateArguments (def, j).isEmpty());
            expect (j["track"].is_number());
            expect (j["on"].is_boolean() && j["on"].get<bool>());
        }

        beginTest ("Fuori intervallo");
        {
            nlohmann::json j = { { "track", 9 } };
            auto e = ToolRegistry::validateArguments (def, j);
            expect (e.contains ("track") && e.contains ("7"));
        }

        beginTest ("Valore non ammesso dall'enum");
        {
            nlohmann::json j = { { "track", 1 }, { "mode", "boh" } };
            auto e = ToolRegistry::validateArguments (def, j);
            expect (e.contains ("solo") && e.contains ("mute"));
        }

        beginTest ("Senza intervallo dichiarato qualsiasi numero va bene");
        {
            nlohmann::json j = { { "track", 1 }, { "value", -12345.5 } };
            expect (ToolRegistry::validateArguments (def, j).isEmpty());
        }

        beginTest ("executeTool non lancia l'azione se gli argomenti sono invalidi");
        {
            ToolRegistry reg;
            reg.registerTool (def);
            ToolCall call; call.id = "1"; call.name = "prova"; call.arguments = { { "track", 99 } };
            auto r = reg.executeTool (call);
            expect (! r.success);
            expect (r.output.contains ("non eseguito"));
        }
    }
};

static ToolValidationTest toolValidationTest;
