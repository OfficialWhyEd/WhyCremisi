#include "../McpClient.h"
#include <juce_core/juce_core.h>

namespace
{
    const char* const fakeServer = R"PY(
import sys, json
for line in sys.stdin:
    m = json.loads(line)
    if 'id' not in m:
        continue
    sys.stderr.write("rumore su stderr\n"); sys.stderr.flush()
    meth = m['method']
    if meth == 'initialize':
        r = {'protocolVersion': '2025-06-18', 'capabilities': {'tools': {}}, 'serverInfo': {'name': 'finto', 'version': '0'}}
    elif meth == 'tools/list':
        r = {'tools': [{'name': 'somma', 'description': 'Somma due numeri', 'inputSchema': {'type': 'object'}}]}
    elif meth == 'tools/call':
        a = m['params']['arguments']
        if m['params']['name'] == 'somma':
            r = {'content': [{'type': 'text', 'text': str(a['a'] + a['b'])}]}
        else:
            r = {'isError': True, 'content': [{'type': 'text', 'text': 'tool sconosciuto'}]}
    else:
        print(json.dumps({'jsonrpc': '2.0', 'id': m['id'], 'error': {'code': -32601, 'message': 'no'}}), flush=True)
        continue
    print(json.dumps({'jsonrpc': '2.0', 'id': m['id'], 'result': r}), flush=True)
)PY";
}

class McpClientTest : public juce::UnitTest
{
public:
    McpClientTest() : juce::UnitTest ("McpClientTest", "whycremisi") {}

    void runTest() override
    {
        auto script = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("wc-finto-mcp.py");
        script.replaceWithText (fakeServer);

        beginTest ("Avvio, elenco e chiamata di un server finto");
        {
            McpClient c;
            std::string err;
            if (! c.start ({ "python", script.getFullPathName().toStdString() }, err, 8000))
            {
                logMessage ("python non disponibile, test saltato: " + juce::String (err));
            }
            else
            {
                expectEquals (juce::String (c.serverName()), juce::String ("finto"));
                std::vector<McpClient::Tool> tools;
                expect (c.listTools (tools, err), juce::String (err));
                expectEquals ((int) tools.size(), 1);
                if (! tools.empty()) expectEquals (juce::String (tools[0].name), juce::String ("somma"));

                std::string text; bool isErr = true;
                expect (c.callTool ("somma", { { "a", 2 }, { "b", 40 } }, text, isErr, err), juce::String (err));
                expectEquals (juce::String (text), juce::String ("42"));
                expect (! isErr);

                expect (c.callTool ("altro", nlohmann::json::object(), text, isErr, err));
                expect (isErr);
                expectEquals (juce::String (text), juce::String ("tool sconosciuto"));
            }
        }

        beginTest ("Un programma che non esiste da errore, non si blocca");
        {
            McpClient c;
            std::string err;
            expect (! c.start ({ "programma-che-non-esiste-wc" }, err, 2000));
            expect (! err.empty());
        }

        script.deleteFile();
    }
};

static McpClientTest mcpClientTest;
