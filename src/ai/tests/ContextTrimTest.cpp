/*
  ==============================================================================
  ContextTrimTest.cpp
  PercheCremisi — il taglio del contesto non deve rompere lo storico

  Sintomo che questi test sorvegliano: con il contesto che cresce, le tool
  call cominciavano a fallire "a caso". Il taglio rimuoveva singoli messaggi
  e poteva lasciare un messaggio "tool" senza la tool_call che lo precede (o
  il contrario): i server OpenAI-compatibili rispondono 400, e siccome il
  taglio scatta solo quando il contesto e' grande, sembrava casuale.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../ToolRegistry.h"

class ContextTrimTest : public juce::UnitTest
{
public:
    ContextTrimTest() : juce::UnitTest ("ContextTrim", "whycremisi") {}

    void runTest() override
    {
        testTagliaBlocchiInteri();
        testNonLasciaToolOrfani();
        testUltimoBloccoSopravvive();
    }

private:
    static ContextManager::Message msg (ContextManager::Message::Role r, const juce::String& testo,
                                        int token = 400)
    {
        ContextManager::Message m;
        m.role = r;
        m.content = testo;
        m.estimatedTokens = token;
        return m;
    }

    static bool toolOrfani (const std::vector<ContextManager::Message>& msgs)
    {
        // Ogni messaggio "tool" deve avere, prima di se', un assistant con
        // tool_calls non tagliato.
        bool assistantConCall = false;
        for (const auto& m : msgs)
        {
            if (m.role == ContextManager::Message::Assistant && m.toolCallsJson.isNotEmpty())
                assistantConCall = true;
            else if (m.role == ContextManager::Message::User)
                assistantConCall = false;
            else if (m.role == ContextManager::Message::Tool && ! assistantConCall)
                return true;
        }
        return false;
    }

    void testTagliaBlocchiInteri()
    {
        beginTest ("Taglio: un turno con tool_calls se ne va intero");

        ContextManager cm (1000, 500);

        cm.addMessage (msg (ContextManager::Message::User, "vecchio"));
        auto a = msg (ContextManager::Message::Assistant, "");
        a.toolCallsJson = R"([{"id":"c1","type":"function","function":{"name":"daw.track.create","arguments":"{}"}}])";
        cm.addMessage (a);
        auto t = msg (ContextManager::Message::Tool, "traccia creata");
        t.toolCallId = "c1";
        t.toolName = "daw.track.create";
        cm.addMessage (t);

        cm.addMessage (msg (ContextManager::Message::User, "nuovo"));

        cm.trimToBudget();
        const auto msgs = cm.getMessages();

        expect (! msgs.empty());
        expectEquals ((int) msgs.size(), 1, "resta solo il blocco piu' recente");
        if (! msgs.empty())
        {
            expect (msgs[0].role == ContextManager::Message::User);
            expectEquals (msgs[0].content, juce::String ("nuovo"));
        }
    }

    void testNonLasciaToolOrfani()
    {
        beginTest ("Taglio: mai un messaggio tool senza la sua tool_call");

        ContextManager cm (1200, 500);
        for (int giro = 0; giro < 7; ++giro)
        {
            cm.addMessage (msg (ContextManager::Message::User, "u" + juce::String (giro)));
            auto a = msg (ContextManager::Message::Assistant, "");
            a.toolCallsJson = R"([{"id":"c","type":"function","function":{"name":"daw.track.mute","arguments":"{}"}}])";
            cm.addMessage (a);
            auto t = msg (ContextManager::Message::Tool, "ok");
            t.toolCallId = "c";
            cm.addMessage (t);
        }

        cm.trimToBudget();
        expect (! toolOrfani (cm.getMessages()), "nessun tool orfano dopo il taglio");
    }

    void testUltimoBloccoSopravvive()
    {
        beginTest ("Taglio: il turno corrente non viene mai rimosso");

        ContextManager cm (200, 500);   // budget piccolissimo
        cm.addMessage (msg (ContextManager::Message::User, "primo", 900));
        cm.addMessage (msg (ContextManager::Message::User, "corrente", 900));

        cm.trimToBudget();
        const auto msgs = cm.getMessages();

        expect (! msgs.empty());
        if (! msgs.empty())
            expectEquals (msgs.back().content, juce::String ("corrente"));
    }
};

static ContextTrimTest contextTrimTest;
