/*
  ==============================================================================
  SessionManager.h
  WhyCremisi — Persistent session logging and long-term memory

  Creates a JSON session file per plugin open/close cycle.
  Saves to: ~/Library/Application Support/WhyCremisi/sessions/
  Long-term memory: ~/Library/Application Support/WhyCremisi/memory.json
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
    void startSession(const juce::String& dawHint = "");
    void endSession();
    bool isSessionActive() const { return sessionActive.load(); }
    juce::String getCurrentSessionId() const { return sessionId; }

    //==========================================================================
    // Log event types
    void logOscEvent   (const juce::String& address, float value);
    void logAiPrompt   (const juce::String& prompt, const juce::String& provider);
    void logAiResponse (const juce::String& response, int durationMs);
    void logTransport  (bool isPlaying, bool isRecording, float bpm);
    void logDawCommand (const juce::String& command);
    void logError      (const juce::String& code, const juce::String& msg);

    //==========================================================================
    juce::File getSessionsDirectory() const;
    juce::File getLongTermMemoryFile() const;
    juce::File getAppDataDirectory() const;

    //==========================================================================
    int getEventCount() const { return eventCount.load(); }

private:
    juce::String    sessionId;
    juce::File      currentSessionFile;
    std::atomic<bool> sessionActive { false };
    std::atomic<int>  eventCount    { 0 };
    int64_t         sessionStartMs  { 0 };

    // OSC rate-limit: don't write every single meter tick
    int64_t lastOscLogMs { 0 };
    static constexpr int OSC_LOG_INTERVAL_MS = 2000;

    juce::CriticalSection lock;

    void        appendEvent (const nlohmann::json& event);
    void        writeSessionHeader();
    void        closeSessionFile();
    void        updateLongTermMemory();
    juce::String generateSessionId() const;
};
