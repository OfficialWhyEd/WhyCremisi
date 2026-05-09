/*
  ==============================================================================
  SessionManager.h
  WhyCremisi — Persistent Session Memory & Long-Term Knowledge Base

  Philosophy: everything that happens while the plugin is active is logged.
  Every transport change, every OSC message, every AI interaction, every DAW
  command — all stored with millisecond-precision timestamps so the entire
  session history is always reconstructable.

  Storage layout:
    ~/Library/Application Support/WhyCremisi/
      sessions/
        YYYYMMDD_HHMMSS/
          header.json       — session metadata (written once at start)
          events.jsonl      — one JSON object per line, append-only (fast)
          summary.json      — written on session end
      current.json          — live snapshot of the active session (always fresh)
      memory.json           — long-term knowledge base across all sessions
  ==============================================================================
*/
#pragma once

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <atomic>

class SessionManager
{
public:
    SessionManager();
    ~SessionManager();

    //==========================================================================
    void startSession  (const juce::String& dawHint = "Unknown DAW");
    void endSession    ();
    bool isActive      () const { return sessionActive.load(); }
    juce::String getSessionId() const { return sessionId; }

    //==========================================================================
    // Event loggers — call from any thread, thread-safe
    void logOscEvent    (const juce::String& address, float value);
    void logTransport   (bool isPlaying, bool isRecording, float bpm, float positionSeconds = 0.0f);
    void logDawCommand  (const juce::String& command, const juce::String& params = "");
    void logParameter   (const juce::String& name, float value);
    void logAiPrompt    (const juce::String& prompt, const juce::String& provider, const juce::String& model = "");
    void logAiResponse  (const juce::String& response, int durationMs, bool success = true);
    void logWsConnect   (int clientId, bool connected);
    void logError       (const juce::String& code, const juce::String& msg);
    void logNote        (const juce::String& text);  // free-form note

    //==========================================================================
    juce::File getAppDataDirectory () const;
    juce::File getSessionsDirectory() const;
    juce::File getMemoryFile       () const;
    juce::File getCurrentSessionFile() const;
    juce::File getActiveSessionDir () const;

    //==========================================================================
    int  getEventCount () const { return eventCount.load(); }

private:
    //==========================================================================
    // Core append — writes one JSON line to events.jsonl and updates current.json
    void appendEvent (const nlohmann::json& event);

    // Build event base with timestamp fields already filled
    nlohmann::json makeEventBase (const std::string& type);

    // Finalize session: build summary.json + update memory.json
    void closeSession ();
    void updateMemory ();

    // Generate human-readable timestamp string from ms epoch
    static juce::String humanTime (int64_t ms);

    //==========================================================================
    juce::String  sessionId;
    juce::File    sessionDir;         // sessions/YYYYMMDD_HHMMSS/
    juce::File    eventsFile;         // sessions/YYYYMMDD_HHMMSS/events.jsonl
    juce::File    headerFile;         // sessions/YYYYMMDD_HHMMSS/header.json
    juce::File    summaryFile;        // sessions/YYYYMMDD_HHMMSS/summary.json
    juce::File    currentFile;        // current.json  (always fresh)

    std::atomic<bool>  sessionActive { false };
    std::atomic<int>   eventCount    { 0 };
    int64_t            sessionStartMs { 0 };

    // Per-type counters for summary
    std::atomic<int> countTransport   { 0 };
    std::atomic<int> countOsc         { 0 };
    std::atomic<int> countDawCommand  { 0 };
    std::atomic<int> countAiPrompt    { 0 };
    std::atomic<int> countAiResponse  { 0 };
    std::atomic<int> countError       { 0 };

    // OSC rate-limit: meter/position ticks only every 500ms
    int64_t lastMeterLogMs   { 0 };
    int64_t lastPositionLogMs{ 0 };
    static constexpr int METER_LOG_INTERVAL_MS    = 500;
    static constexpr int POSITION_LOG_INTERVAL_MS = 1000;

    // Last known state (for current.json snapshot)
    bool   lastIsPlaying   { false };
    bool   lastIsRecording { false };
    float  lastBpm         { 120.0f };
    float  lastPosition    { 0.0f };

    juce::CriticalSection lock;
};
