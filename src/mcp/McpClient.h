#pragma once
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct subprocess_s;

// Client minimale del Model Context Protocol, trasporto stdio (JSON-RPC, una riga per messaggio).
// Avvia un server MCP come processo figlio, fa initialize, elenca i tool e li chiama.
// Quello che un server restituisce e' DATO: chi lo usa non lo tratta mai come un ordine.
class McpClient
{
public:
    struct Tool
    {
        std::string name;
        std::string description;
        nlohmann::json inputSchema;
    };

    McpClient();
    ~McpClient();

    // command: programma e argomenti (su Windows per npx: {"cmd","/c","npx",...}).
    bool start (const std::vector<std::string>& command, std::string& error, int timeoutMs = 15000);
    void stop();
    bool isRunning() const { return running; }

    bool listTools (std::vector<Tool>& out, std::string& error, int timeoutMs = 15000);

    // text: il testo dei contenuti restituiti. isError: il server dice che il tool e' fallito.
    bool callTool (const std::string& name, const nlohmann::json& arguments,
                   std::string& text, bool& isError, std::string& error, int timeoutMs = 60000);

    const std::string& serverName() const { return serverNameValue; }

private:
    bool request (const std::string& method, const nlohmann::json& params,
                  nlohmann::json& result, std::string& error, int timeoutMs);
    bool sendLine (const nlohmann::json& message);
    void readerLoop();

    std::unique_ptr<subprocess_s> proc;
    std::thread reader;
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<nlohmann::json> inbox;
    bool running = false;
    bool readerDone = false;
    int nextId = 1;
    std::string serverNameValue;
};
