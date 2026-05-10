/*
  ==============================================================================
  SessionManager.cpp
  WhyCremisi — Persistent Session Memory & Long-Term Knowledge Base
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
// Paths
//==============================================================================
juce::File SessionManager::getAppDataDirectory() const
{
    // On macOS: ~/Library/Application Support/WhyCremisi
    // On Windows: C:/Users/<name>/AppData/Roaming/WhyCremisi
    // On Linux:  ~/.config/WhyCremisi
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                       .getChildFile("Library/Application Support/WhyCremisi");
#elif JUCE_WINDOWS
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("WhyCremisi");
#else
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                       .getChildFile(".config/WhyCremisi");
#endif
}

juce::File SessionManager::getSessionsDirectory() const
{
    return getAppDataDirectory().getChildFile("sessions");
}

juce::File SessionManager::getMemoryFile() const
{
    return getAppDataDirectory().getChildFile("memory.json");
}

juce::File SessionManager::getCurrentSessionFile() const
{
    return getAppDataDirectory().getChildFile("current.json");
}

juce::File SessionManager::getActiveSessionDir() const
{
    return sessionDir;
}

//==============================================================================
// Timestamp helpers
//==============================================================================
juce::String SessionManager::humanTime(int64_t ms)
{
    juce::Time t(ms);
    return t.toString(true, true, true, true);
}

//==============================================================================
// Session lifecycle
//==============================================================================
void SessionManager::startSession(const juce::String& dawHint)
{
    juce::ScopedLock sl(lock);

    if (sessionActive.load())
        closeSession();

    sessionStartMs = juce::Time::currentTimeMillis();

    // Session ID = YYYYMMDD_HHMMSS (unique enough for our purposes)
    juce::Time now(sessionStartMs);
    sessionId = now.formatted("%Y%m%d_%H%M%S");

    // Create session directory
    sessionDir   = getSessionsDirectory().getChildFile(sessionId);
    eventsFile   = sessionDir.getChildFile("events.jsonl");
    headerFile   = sessionDir.getChildFile("header.json");
    summaryFile  = sessionDir.getChildFile("summary.json");
    currentFile  = getCurrentSessionFile();

    sessionDir.createDirectory();

    // Reset counters
    eventCount.store(0);
    countTransport.store(0);
    countOsc.store(0);
    countDawCommand.store(0);
    countAiPrompt.store(0);
    countAiResponse.store(0);
    countError.store(0);
    lastMeterLogMs    = 0;
    lastPositionLogMs = 0;
    lastIsPlaying     = false;
    lastIsRecording   = false;
    lastBpm           = 120.0f;
    lastPosition      = 0.0f;

    // Write header.json
    nlohmann::json header;
    header["session_id"]      = sessionId.toStdString();
    header["plugin_version"]  = "1.0.0";
    header["daw"]             = dawHint.toStdString();
    header["started_at_ms"]   = sessionStartMs;
    header["started_at"]      = humanTime(sessionStartMs).toStdString();
    header["ended_at"]        = nullptr;
    header["duration_seconds"]= nullptr;
    headerFile.replaceWithText(juce::String(header.dump(2).data()));

    // Truncate events file (fresh start)
    eventsFile.replaceWithText("");

    sessionActive.store(true);

    DBG("[SessionManager] Session started: " + sessionId);

    // Log the open event itself
    auto ev = makeEventBase("session_open");
    ev["daw"] = dawHint.toStdString();
    appendEvent(ev);
}

void SessionManager::endSession()
{
    if (!sessionActive.load()) return;

    juce::ScopedLock sl(lock);
    sessionActive.store(false);

    // Log close event
    auto ev = makeEventBase("session_close");
    ev["total_events"] = eventCount.load();
    appendEvent(ev);

    closeSession();
    updateMemory();

    DBG("[SessionManager] Session ended: " + sessionId + " (" +
        juce::String(eventCount.load()) + " events)");
}

void SessionManager::closeSession()
{
    int64_t endMs    = juce::Time::currentTimeMillis();
    int64_t durSec   = (endMs - sessionStartMs) / 1000;

    // Update header with end time
    try
    {
        auto content = headerFile.loadFileAsString();
        auto root    = nlohmann::json::parse(content.toStdString());
        root["ended_at"]         = humanTime(endMs).toStdString();
        root["ended_at_ms"]      = endMs;
        root["duration_seconds"] = durSec;
        headerFile.replaceWithText(juce::String(root.dump(2).data()));
    }
    catch (...) {}

    // Write summary.json
    nlohmann::json summary;
    summary["session_id"]       = sessionId.toStdString();
    summary["duration_seconds"] = durSec;
    summary["total_events"]     = eventCount.load();
    summary["transport_changes"]= countTransport.load();
    summary["osc_events"]       = countOsc.load();
    summary["daw_commands"]     = countDawCommand.load();
    summary["ai_prompts"]       = countAiPrompt.load();
    summary["ai_responses"]     = countAiResponse.load();
    summary["errors"]           = countError.load();
    summaryFile.replaceWithText(juce::String(summary.dump(2).data()));

    // Clear current.json
    nlohmann::json closed;
    closed["session_id"]   = sessionId.toStdString();
    closed["status"]       = "closed";
    closed["ended_at"]     = humanTime(endMs).toStdString();
    currentFile.replaceWithText(juce::String(closed.dump(2).data()));
}

//==============================================================================
// Long-term memory update
//==============================================================================
void SessionManager::updateMemory()
{
    auto memFile = getMemoryFile();
    nlohmann::json mem;

    if (memFile.existsAsFile())
    {
        try { mem = nlohmann::json::parse(memFile.loadFileAsString().toStdString()); }
        catch (...) {}
    }

    // Bootstrap structure
    if (!mem.contains("version"))          mem["version"]          = "2.0";
    if (!mem.contains("total_sessions"))   mem["total_sessions"]   = 0;
    if (!mem.contains("total_events"))     mem["total_events"]     = 0;
    if (!mem.contains("total_active_sec")) mem["total_active_sec"] = 0;
    if (!mem.contains("total_ai_prompts")) mem["total_ai_prompts"] = 0;
    if (!mem.contains("sessions"))         mem["sessions"]         = nlohmann::json::array();

    int64_t now = juce::Time::currentTimeMillis();

    mem["total_sessions"]   = mem["total_sessions"].get<int>() + 1;
    mem["total_events"]     = mem["total_events"].get<int>()   + eventCount.load();
    mem["total_active_sec"] = mem["total_active_sec"].get<int64_t>() +
                              (now - sessionStartMs) / 1000;
    mem["total_ai_prompts"] = mem["total_ai_prompts"].get<int>() + countAiPrompt.load();
    mem["last_session"]     = sessionId.toStdString();
    mem["last_updated"]     = humanTime(now).toStdString();
    mem["last_updated_ms"]  = now;

    // Append session index entry
    nlohmann::json entry;
    entry["id"]              = sessionId.toStdString();
    entry["started_at"]      = humanTime(sessionStartMs).toStdString();
    entry["duration_seconds"]= (now - sessionStartMs) / 1000;
    entry["total_events"]    = eventCount.load();
    entry["transport_changes"]= countTransport.load();
    entry["ai_prompts"]      = countAiPrompt.load();
    entry["daw_commands"]    = countDawCommand.load();
    entry["errors"]          = countError.load();
    mem["sessions"].push_back(entry);

    // Keep only last 500 sessions in index
    if ((int)mem["sessions"].size() > 500)
        mem["sessions"].erase(mem["sessions"].begin());

    getAppDataDirectory().createDirectory();
    memFile.replaceWithText(juce::String(mem.dump(2).data()));
}

//==============================================================================
// Core append — O(1) append to JSONL file
//==============================================================================
nlohmann::json SessionManager::makeEventBase(const std::string& type)
{
    int64_t now = juce::Time::currentTimeMillis();
    nlohmann::json ev;
    ev["ms"]         = now;
    ev["elapsed_ms"] = now - sessionStartMs;
    ev["time"]       = humanTime(now).toStdString();
    ev["type"]       = type;
    return ev;
}

void SessionManager::appendEvent(const nlohmann::json& event)
{
    if (!sessionActive.load() && event["type"] != "session_close")
        return;

    juce::ScopedLock sl(lock);
    eventCount.fetch_add(1);

    // Append one JSON line to events.jsonl
    juce::String line(event.dump().data());
    eventsFile.appendText(line + "\n");

    // Update current.json with live session snapshot
    nlohmann::json current;
    current["session_id"]    = sessionId.toStdString();
    current["status"]        = "active";
    current["started_at"]    = humanTime(sessionStartMs).toStdString();
    current["elapsed_sec"]   = (juce::Time::currentTimeMillis() - sessionStartMs) / 1000;
    current["total_events"]  = eventCount.load();
    current["last_event"]    = event;
    current["transport"] = {
        {"isPlaying",   lastIsPlaying},
        {"isRecording", lastIsRecording},
        {"bpm",         lastBpm},
        {"position",    lastPosition}
    };
    current["counts"] = {
        {"transport",  countTransport.load()},
        {"osc",        countOsc.load()},
        {"daw_cmd",    countDawCommand.load()},
        {"ai_prompts", countAiPrompt.load()},
        {"errors",     countError.load()}
    };
    currentFile.replaceWithText(juce::String(current.dump(2).data()));
}

//==============================================================================
// Event loggers
//==============================================================================

void SessionManager::logOscEvent(const juce::String& address, float value)
{
    if (!sessionActive.load()) return;

    int64_t now = juce::Time::currentTimeMillis();

    // Rate-limit meter ticks and position updates
    bool isMeter    = address.contains("meter") || address.contains("level");
    bool isPosition = address.contains("song_time") || address.contains("position");

    if (isMeter && (now - lastMeterLogMs < METER_LOG_INTERVAL_MS)) return;
    if (isPosition && (now - lastPositionLogMs < POSITION_LOG_INTERVAL_MS)) return;

    if (isMeter)    lastMeterLogMs    = now;
    if (isPosition) lastPositionLogMs = now;

    countOsc.fetch_add(1);
    auto ev = makeEventBase("osc");
    ev["address"] = address.toStdString();
    ev["value"]   = value;
    appendEvent(ev);
}

void SessionManager::logTransport(bool isPlaying, bool isRecording, float bpm, float positionSeconds)
{
    if (!sessionActive.load()) return;

    lastIsPlaying   = isPlaying;
    lastIsRecording = isRecording;
    lastBpm         = bpm;
    lastPosition    = positionSeconds;

    countTransport.fetch_add(1);
    auto ev = makeEventBase("transport");
    ev["isPlaying"]        = isPlaying;
    ev["isRecording"]      = isRecording;
    ev["bpm"]              = bpm;
    ev["position_seconds"] = positionSeconds;

    // Human-readable state description
    juce::String desc = isPlaying ? (isRecording ? "REC" : "PLAY") : "STOP";
    ev["state"] = desc.toStdString();

    appendEvent(ev);
}

void SessionManager::logDawCommand(const juce::String& command, const juce::String& params)
{
    if (!sessionActive.load()) return;

    countDawCommand.fetch_add(1);
    auto ev = makeEventBase("daw_command");
    ev["command"] = command.toStdString();
    if (params.isNotEmpty())
        ev["params"] = params.toStdString();
    appendEvent(ev);
}

void SessionManager::logParameter(const juce::String& name, float value)
{
    if (!sessionActive.load()) return;

    auto ev = makeEventBase("parameter");
    ev["name"]  = name.toStdString();
    ev["value"] = value;
    appendEvent(ev);
}

void SessionManager::logAiPrompt(const juce::String& prompt, const juce::String& provider,
                                  const juce::String& model)
{
    if (!sessionActive.load()) return;

    countAiPrompt.fetch_add(1);
    auto ev = makeEventBase("ai_prompt");
    ev["provider"]     = provider.toStdString();
    ev["model"]        = model.isNotEmpty() ? model.toStdString() : "unknown";
    ev["prompt"]       = prompt.toStdString();           // full prompt, no truncation
    ev["prompt_chars"] = prompt.length();
    appendEvent(ev);
}

void SessionManager::logAiResponse(const juce::String& response, int durationMs, bool success)
{
    if (!sessionActive.load()) return;

    countAiResponse.fetch_add(1);
    auto ev = makeEventBase("ai_response");
    ev["success"]     = success;
    ev["duration_ms"] = durationMs;
    ev["chars"]       = response.length();
    ev["response"]    = response.toStdString();
    appendEvent(ev);
}

void SessionManager::logAiAction(const juce::String& widgetId, float value,
                                  float previousValue, const juce::String& description)
{
    if (!sessionActive.load()) return;

    auto ev = makeEventBase("ai_action");
    ev["widget_id"]    = widgetId.toStdString();
    ev["value"]        = value;
    ev["previous"]     = previousValue;
    ev["description"]  = description.toStdString();
    appendEvent(ev);
}

void SessionManager::logWsConnect(int clientId, bool connected)
{
    if (!sessionActive.load()) return;

    auto ev = makeEventBase(connected ? "ws_connect" : "ws_disconnect");
    ev["client_id"] = clientId;
    appendEvent(ev);
}

void SessionManager::logError(const juce::String& code, const juce::String& msg)
{
    countError.fetch_add(1);
    auto ev = makeEventBase("error");
    ev["code"]    = code.toStdString();
    ev["message"] = msg.toStdString();
    appendEvent(ev);
}

void SessionManager::logNote(const juce::String& text)
{
    if (!sessionActive.load()) return;

    auto ev = makeEventBase("note");
    ev["text"] = text.toStdString();
    appendEvent(ev);
}
