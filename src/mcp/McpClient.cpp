#include "McpClient.h"
#include "../../third_party/subprocess/subprocess.h"
#include <chrono>

McpClient::McpClient() = default;
McpClient::~McpClient() { stop(); }

bool McpClient::start (const std::vector<std::string>& command, std::string& error, int timeoutMs)
{
    stop();
    if (command.empty()) { error = "comando vuoto"; return false; }

    std::vector<const char*> argv;
    for (const auto& c : command) argv.push_back (c.c_str());
    argv.push_back (nullptr);

    proc = std::make_unique<subprocess_s>();
    // stderr nello stesso flusso: le righe che non sono JSON si scartano.
    const int opts = subprocess_option_combined_stdout_stderr | subprocess_option_inherit_environment
                   | subprocess_option_enable_async | subprocess_option_no_window
                   | subprocess_option_search_user_path;
    if (subprocess_create_ex (argv.data(), opts, nullptr, nullptr, proc.get()) != 0)
    {
        proc.reset();
        error = "impossibile avviare il server: " + command[0];
        return false;
    }
    running = true;
    readerDone = false;
    {
        std::lock_guard<std::mutex> lock (mutex);
        inbox.clear();
    }
    reader = std::thread ([this] { readerLoop(); });

    nlohmann::json result;
    const nlohmann::json params = {
        { "protocolVersion", "2025-06-18" },
        { "capabilities", nlohmann::json::object() },
        { "clientInfo", { { "name", "WhyCremisi" }, { "version", "1" } } }
    };
    if (! request ("initialize", params, result, error, timeoutMs)) { stop(); return false; }
    if (result.contains ("serverInfo") && result["serverInfo"].contains ("name")
        && result["serverInfo"]["name"].is_string())
        serverNameValue = result["serverInfo"]["name"].get<std::string>();
    sendLine ({ { "jsonrpc", "2.0" }, { "method", "notifications/initialized" } });
    return true;
}

void McpClient::stop()
{
    if (proc && running)
        subprocess_terminate (proc.get());   // sblocca anche la lettura
    running = false;
    if (reader.joinable()) reader.join();
    if (proc) { subprocess_destroy (proc.get()); proc.reset(); }
    std::lock_guard<std::mutex> lock (mutex);
    inbox.clear();
}

bool McpClient::sendLine (const nlohmann::json& message)
{
    if (! proc || ! running) return false;
    FILE* in = subprocess_stdin (proc.get());
    if (in == nullptr) return false;
    const auto line = message.dump() + "\n";
    if (fwrite (line.data(), 1, line.size(), in) != line.size()) return false;
    return fflush (in) == 0;
}

void McpClient::readerLoop()
{
    std::string pending;
    char buf[4096];
    for (;;)
    {
        const unsigned n = subprocess_read_stdout (proc.get(), buf, sizeof (buf));
        if (n == 0) break;
        pending.append (buf, n);
        size_t nl;
        while ((nl = pending.find ('\n')) != std::string::npos)
        {
            std::string line = pending.substr (0, nl);
            pending.erase (0, nl + 1);
            while (! line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty() || line[0] != '{') continue;
            auto msg = nlohmann::json::parse (line, nullptr, false);
            if (msg.is_discarded()) continue;
            { std::lock_guard<std::mutex> lock (mutex); inbox.push_back (std::move (msg)); }
            cv.notify_all();
        }
    }
    { std::lock_guard<std::mutex> lock (mutex); readerDone = true; }
    cv.notify_all();
}

bool McpClient::request (const std::string& method, const nlohmann::json& params,
                         nlohmann::json& result, std::string& error, int timeoutMs)
{
    const int id = nextId++;
    nlohmann::json msg = { { "jsonrpc", "2.0" }, { "id", id }, { "method", method } };
    if (! params.is_null()) msg["params"] = params;
    if (! sendLine (msg)) { error = "il server non risponde (scrittura fallita)"; return false; }

    const auto fine = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
    std::unique_lock<std::mutex> lock (mutex);
    for (;;)
    {
        for (auto it = inbox.begin(); it != inbox.end(); ++it)
        {
            if (! it->contains ("id") || ! (*it)["id"].is_number_integer() || (*it)["id"].get<int>() != id)
                continue;
            auto m = std::move (*it);
            inbox.erase (it);
            if (m.contains ("error"))
            {
                error = m["error"].value ("message", std::string ("errore del server"));
                return false;
            }
            result = m.value ("result", nlohmann::json::object());
            return true;
        }
        // Richieste del server verso di noi (ping, roots...): non le supportiamo, restano da parte.
        if (readerDone) { error = "il server si e' chiuso"; return false; }
        if (cv.wait_until (lock, fine) == std::cv_status::timeout) { error = "tempo scaduto"; return false; }
    }
}

bool McpClient::listTools (std::vector<Tool>& out, std::string& error, int timeoutMs)
{
    out.clear();
    std::string cursor;
    for (int pagina = 0; pagina < 20; ++pagina)
    {
        nlohmann::json result;
        nlohmann::json params = cursor.empty() ? nlohmann::json() : nlohmann::json { { "cursor", cursor } };
        if (! request ("tools/list", params, result, error, timeoutMs)) return false;
        if (result.contains ("tools") && result["tools"].is_array())
            for (const auto& t : result["tools"])
            {
                if (! t.contains ("name") || ! t["name"].is_string()) continue;
                Tool tool;
                tool.name = t["name"].get<std::string>();
                if (t.contains ("description") && t["description"].is_string())
                    tool.description = t["description"].get<std::string>();
                tool.inputSchema = t.value ("inputSchema", nlohmann::json::object());
                out.push_back (std::move (tool));
            }
        cursor = (result.contains ("nextCursor") && result["nextCursor"].is_string())
                     ? result["nextCursor"].get<std::string>() : std::string();
        if (cursor.empty()) break;
    }
    return true;
}

bool McpClient::callTool (const std::string& name, const nlohmann::json& arguments,
                          std::string& text, bool& isError, std::string& error, int timeoutMs)
{
    text.clear();
    isError = false;
    nlohmann::json result;
    const nlohmann::json params = { { "name", name },
                                    { "arguments", arguments.is_null() ? nlohmann::json::object() : arguments } };
    if (! request ("tools/call", params, result, error, timeoutMs)) return false;
    isError = result.value ("isError", false);
    if (result.contains ("content") && result["content"].is_array())
        for (const auto& c : result["content"])
        {
            if (! text.empty()) text += "\n";
            if (c.value ("type", std::string()) == "text" && c.contains ("text") && c["text"].is_string())
                text += c["text"].get<std::string>();
            else
                text += "[contenuto " + c.value ("type", std::string ("sconosciuto")) + " non mostrato]";
        }
    return true;
}
