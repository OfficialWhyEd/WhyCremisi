/*
  ==============================================================================
  SessionManager.cpp
  ==============================================================================
*/

#include "SessionManager.h"

SessionManager::SessionManager() = default;

SessionManager::~SessionManager()
{
    if (sessionActive.load())
        endSession();
}

//==============================================================================
juce::File SessionManager::getAppDataDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("WhyCremisi");
}

juce::File SessionManager::getSessionsDirectory() const
{
    return getAppDataDirectory().getChildFile("sessions");
}

juce::File SessionManager::getLongTermMemoryFile() const
{
    return getAppDataDirectory().getChildFile("memory.json");
}

//==============================================================================
juce::String SessionManager::generateSessionId() const
{
    auto now = juce::Time::getCurrentTime();
    return now.formatted("%Y%m%d_%H%M%S");
}

//==============================================================================
void SessionManager::startSession(const juce::String& dawHint)
{
    juce::ScopedLock sl(lock);

    if (sessionActive.load())
        closeSessionFile(); // close previous if still open

    sessionId        = generateSessionId();
    sessionStartMs   = juce::Time::currentTimeMillis();
    eventCount.store(0);
    lastOscLogMs     = 0;

    // Ensure directory exists
    getSessionsDirectory().createDirectory();

    currentSessionFile = getSessionsDirectory().getChildFile(sessionId + ".json");

    // Write header object to file
    nlohmann::json header;
    header["session_id"]   = sessionId.toStdString();
    header["started_at"]   = sessionStartMs;
    header["ended_at"]     = nullptr;
    header["daw"]          = dawHint.isNotEmpty() ? dawHint.toStdString() : "Unknown";
    header["bpm_at_start"] = 120.0;
    header["events"]       = nlohmann::json::array();

    currentSessionFile.replaceWithText(juce::String(header.dump(2).data()));
    sessionActive.store(true);

    DBG("[SessionManager] Session started: " + sessionId);
}

void SessionManager::endSession()
{
    if (!sessionActive.load()) return;

    juce::ScopedLock sl(lock);
    sessionActive.store(false);
    closeSessionFile();
    updateLongTermMemory();
    DBG("[SessionManager] Session ended: " + sessionId);
}

//==============================================================================
// Append event to the session file (line-based JSONL inside events array)
// We use an append approach: read file, parse, add event, write back
// Rate-limited to avoid disk thrash on high-frequency OSC events.
void SessionManager::appendEvent(const nlohmann::json& event)
{
    if (!sessionActive.load() || !currentSessionFile.existsAsFile()) return;

    juce::ScopedLock sl(lock);
    eventCount.fetch_add(1);

    try
    {
        juce::String content = currentSessionFile.loadFileAsString();
        auto root = nlohmann::json::parse(content.toStdString());
        root["events"].push_back(event);
        currentSessionFile.replaceWithText(juce::String(root.dump(2).data()));
    }
    catch (...) {} // never crash the audio thread
}

void SessionManager::writeSessionHeader()
{
    // already handled in startSession
}

void SessionManager::closeSessionFile()
{
    if (!currentSessionFile.existsAsFile()) return;
    try
    {
        juce::String content = currentSessionFile.loadFileAsString();
        auto root = nlohmann::json::parse(content.toStdString());
        root["ended_at"]         = juce::Time::currentTimeMillis();
        root["duration_seconds"] = (juce::Time::currentTimeMillis() - sessionStartMs) / 1000;
        root["total_events"]     = eventCount.load();
        currentSessionFile.replaceWithText(juce::String(root.dump(2).data()));
    }
    catch (...) {}
}

//==============================================================================
void SessionManager::updateLongTermMemory()
{
    auto memFile = getLongTermMemoryFile();
    nlohmann::json mem;

    if (memFile.existsAsFile())
    {
        try { mem = nlohmann::json::parse(memFile.loadFileAsString().toStdString()); }
        catch (...) {}
    }

    if (!mem.contains("version"))       mem["version"] = "1.0";
    if (!mem.contains("total_sessions")) mem["total_sessions"] = 0;
    if (!mem.contains("total_events"))   mem["total_events"] = 0;
    if (!mem.contains("sessions_index")) mem["sessions_index"] = nlohmann::json::array();

    mem["total_sessions"] = mem["total_sessions"].get<int>() + 1;
    mem["total_events"]   = mem["total_events"].get<int>() + eventCount.load();
    mem["last_updated"]   = juce::Time::currentTimeMillis();

    nlohmann::json entry;
    entry["id"]       = sessionId.toStdString();
    entry["duration"] = (juce::Time::currentTimeMillis() - sessionStartMs) / 1000;
    entry["events"]   = eventCount.load();
    mem["sessions_index"].push_back(entry);

    // Keep only last 100 sessions in index
    if (mem["sessions_index"].size() > 100)
        mem["sessions_index"].erase(mem["sessions_index"].begin());

    getAppDataDirectory().createDirectory();
    memFile.replaceWithText(juce::String(mem.dump(2).data()));
}

//==============================================================================
// Event loggers
//==============================================================================

void SessionManager::logOscEvent(const juce::String& address, float value)
{
    // Rate-limit OSC logging to avoid massive files from meter messages
    int64_t now = juce::Time::currentTimeMillis();
    if (now - lastOscLogMs < OSC_LOG_INTERVAL_MS && address.contains("meter"))
        return;
    if (address.contains("meter"))
        lastOscLogMs = now;

    nlohmann::json ev;
    ev["t"]       = now;
    ev["type"]    = "osc";
    ev["address"] = address.toStdString();
    ev["value"]   = value;
    appendEvent(ev);
}

void SessionManager::logAiPrompt(const juce::String& prompt, const juce::String& provider)
{
    nlohmann::json ev;
    ev["t"]        = juce::Time::currentTimeMillis();
    ev["type"]     = "ai_prompt";
    ev["prompt"]   = prompt.substring(0, 500).toStdString(); // truncate very long prompts
    ev["provider"] = provider.toStdString();
    appendEvent(ev);
}

void SessionManager::logAiResponse(const juce::String& response, int durationMs)
{
    nlohmann::json ev;
    ev["t"]           = juce::Time::currentTimeMillis();
    ev["type"]        = "ai_response";
    ev["length"]      = response.length();
    ev["duration_ms"] = durationMs;
    ev["preview"]     = response.substring(0, 200).toStdString();
    appendEvent(ev);
}

void SessionManager::logTransport(bool isPlaying, bool isRecording, float bpm)
{
    nlohmann::json ev;
    ev["t"]           = juce::Time::currentTimeMillis();
    ev["type"]        = "transport";
    ev["isPlaying"]   = isPlaying;
    ev["isRecording"] = isRecording;
    ev["bpm"]         = bpm;
    appendEvent(ev);
}

void SessionManager::logDawCommand(const juce::String& command)
{
    nlohmann::json ev;
    ev["t"]       = juce::Time::currentTimeMillis();
    ev["type"]    = "daw_command";
    ev["command"] = command.toStdString();
    appendEvent(ev);
}

void SessionManager::logError(const juce::String& code, const juce::String& msg)
{
    nlohmann::json ev;
    ev["t"]    = juce::Time::currentTimeMillis();
    ev["type"] = "error";
    ev["code"] = code.toStdString();
    ev["msg"]  = msg.toStdString();
    appendEvent(ev);
}
