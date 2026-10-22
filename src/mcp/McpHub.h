#pragma once
#include "McpClient.h"
#include <juce_core/juce_core.h>
#include <map>
#include <memory>
#include <mutex>

// I server MCP che la persona ha scelto, letti da un file mcp.json (stesso formato di Claude:
//   {"mcpServers": {"nome": {"command": "npx", "args": ["-y", "pacchetto"]}}} ).
// Il plugin non aggiunge server da solo. Un server parte alla prima richiesta e resta acceso.
class McpHub
{
public:
    // Quanti server trovati nel file (0 se manca o e' rotto).
    int loadConfig (const juce::File& file);

    std::vector<std::string> serverNames() const;

    // Una riga per tool: "server.tool: descrizione". Avvia i server che non sono ancora accesi.
    bool describeTools (std::string& out, std::string& error);

    bool call (const std::string& server, const std::string& tool, const nlohmann::json& args,
               std::string& text, bool& isError, std::string& error);

    void stopAll();

private:
    struct Entry
    {
        std::vector<std::string> command;
        std::unique_ptr<McpClient> client;
    };

    bool ensureStarted (Entry& e, std::string& error);

    mutable std::mutex mutex;
    std::map<std::string, Entry> servers;
};
