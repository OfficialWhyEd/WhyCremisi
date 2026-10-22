#include "McpHub.h"

int McpHub::loadConfig (const juce::File& file)
{
    std::lock_guard<std::mutex> lock (mutex);
    servers.clear();
    if (! file.existsAsFile()) return 0;

    const auto doc = nlohmann::json::parse (file.loadFileAsString().toStdString(), nullptr, false);
    if (doc.is_discarded() || ! doc.contains ("mcpServers") || ! doc["mcpServers"].is_object()) return 0;

    for (const auto& [name, def] : doc["mcpServers"].items())
    {
        if (! def.is_object() || ! def.contains ("command") || ! def["command"].is_string()) continue;
        Entry e;
        e.command.push_back (def["command"].get<std::string>());
        if (def.contains ("args") && def["args"].is_array())
            for (const auto& a : def["args"])
                if (a.is_string()) e.command.push_back (a.get<std::string>());
        servers.emplace (name, std::move (e));
    }
    return (int) servers.size();
}

std::vector<std::string> McpHub::serverNames() const
{
    std::lock_guard<std::mutex> lock (mutex);
    std::vector<std::string> out;
    for (const auto& kv : servers) out.push_back (kv.first);
    return out;
}

bool McpHub::ensureStarted (Entry& e, std::string& error)
{
    if (e.client && e.client->isRunning()) return true;
    e.client = std::make_unique<McpClient>();
    if (e.client->start (e.command, error)) return true;
    e.client.reset();
    return false;
}

bool McpHub::describeTools (std::string& out, std::string& error)
{
    std::lock_guard<std::mutex> lock (mutex);
    out.clear();
    for (auto& [name, e] : servers)
    {
        std::string err;
        std::vector<McpClient::Tool> tools;
        if (! ensureStarted (e, err) || ! e.client->listTools (tools, err))
        {
            out += name + ": non disponibile (" + err + ")\n";
            continue;
        }
        for (const auto& t : tools)
            out += name + "." + t.name + ": " + t.description.substr (0, 160) + "\n";
    }
    if (servers.empty()) error = "nessun server MCP configurato";
    return ! servers.empty();
}

bool McpHub::call (const std::string& server, const std::string& tool, const nlohmann::json& args,
                   std::string& text, bool& isError, std::string& error)
{
    std::lock_guard<std::mutex> lock (mutex);
    auto it = servers.find (server);
    if (it == servers.end()) { error = "server MCP '" + server + "' non configurato"; return false; }
    if (! ensureStarted (it->second, error)) return false;
    return it->second.client->callTool (tool, args, text, isError, error);
}

void McpHub::stopAll()
{
    std::lock_guard<std::mutex> lock (mutex);
    for (auto& kv : servers) if (kv.second.client) kv.second.client->stop();
}
