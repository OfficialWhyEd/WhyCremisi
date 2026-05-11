/*
  ==============================================================================
  OscBridge.h
  WhyCremisi VST Plugin - Bidirectional OSC-WebSocket Bridge

  Connects:
  - OscHandler (UDP OSC from/to DAW)
  - WebSocketServer (TCP WebSocket to React UI)

  Handles translation between OSC binary protocol and JSON messages
  according to protocol-json-v1.md.

  Flow:
  DAW OSC (UDP 9000) → OscHandler → OscBridge → WebSocketServer → React UI
  React UI → WebSocketServer → OscBridge → OscHandler → DAW OSC (UDP 9001)
  ==============================================================================
*/

#pragma once

#include "OscHandler.h"
#include "WebSocketServer.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <nlohmann/json.hpp>
#include <thread>

// Forward declarations
class AiEngine;
class SessionManager;
class MidiHandler;
class ParameterMapper;
class PluginChain;

class OscBridge : private juce::Timer
{
public:
    //==============================================================================
    OscBridge(int oscReceivePort = 9000, int wsPort = 8080);
    ~OscBridge();

    //==============================================================================
    /** Start both OSC listener and WebSocket server */
    bool start();

    /** Stop everything */
    void stop();

    /** Check if running */
    bool isRunning() const;

    //==============================================================================
    /** Get status info */
    int getOscMessagesReceived() const;
    int getOscMessagesSent() const;
    int getWebSocketClientsConnected() const;
    juce::String getLastError() const { return lastError; }

    //==============================================================================
    /** Send OSC message to DAW (UI → DAW direction) */
    void sendOscToDaw(const juce::String& address, float value);
    void sendOscToDaw(const juce::String& address, const juce::String& value);
    void sendOscToDaw(const juce::String& address, int value);

    //==============================================================================
    /** Broadcast DAW state to all WebSocket clients (DAW → UI direction) */
    void broadcastTransport(bool isPlaying, bool isRecording, float bpm, float positionSeconds);
    void broadcastTrackUpdate(int trackId, const juce::String& name, float volumeDb, float pan,
                             bool isMuted = false, bool isSoloed = false);
    void broadcastMeter(int trackId, float leftDb, float rightDb, float peakLeftDb = -96.0f, float peakRightDb = -96.0f);
    void broadcastAiResponse(const juce::String& requestId, const juce::String& content,
                            const juce::String& provider = "ollama", bool isComplete = true);
    void broadcastAiStream(const juce::String& requestId, const juce::String& chunk, bool isDone);
    void broadcastWidgetCreate(const juce::String& widgetId, const juce::String& widgetType,
                               const juce::String& title, const nlohmann::json& config);
    void broadcastWidgetUpdate(const juce::String& widgetId, const nlohmann::json& values);
    void broadcastWidgetRemove(const juce::String& widgetId);
    void broadcastError(const juce::String& code, const juce::String& message,
                       const juce::String& severity = "error");

    //==============================================================================
    /** Forward raw OSC message to WebSocket clients (for debugging) */
    void forwardOscToUI(const juce::String& address, float value);

    /** Broadcast raw JSON to all WebSocket clients */
    void broadcastJson(const nlohmann::json& msg);

    //==============================================================================
    /** Set the DAW OSC target (plugin sends to this address) */
    void setDawTarget(const juce::String& host, int sendPort);

    //==============================================================================
    /** Configuration */
    int getOscPort() const { return oscPort; }
    int getWebSocketPort() const { return wsPort; }

    //==============================================================================
    /** Set AI Engine for processing prompts */
    void setAiEngine(AiEngine* engine);

    /** Set MIDI Handler for MIDI Learn */
    void setMidiHandler(MidiHandler* mh);

    /** Set Parameter Mapper for action execution */
    void setParameterMapper(ParameterMapper* pm);

    /** Set Plugin Chain for chain management */
    void setPluginChain(PluginChain* pc);

    /** Set Session Manager for event logging */
    void setSessionManager(SessionManager* sm);

    /** Get current AI response status */
    bool isAiProcessing() const { return aiProcessing.load(); }

    /** Callback: widget value changed from UI → ParameterMapper */
    std::function<void(const juce::String&, float)> widgetChangeCallback;

    //==============================================================================
    /** Called from prepareToPlay with real audio device info */
    void broadcastPluginStats(double sampleRate, int bufferSize);

    /** Called from processBlock to update analyzer data (thread-safe) */
    void updateAnalyzer(float correlation, float loudness, const std::vector<float>& spectrum);

    /** Called from processBlock to update meter levels (thread-safe) */
    void updateMeter(float leftDb, float rightDb)
    {
        lastMeterL.store(leftDb);
        lastMeterR.store(rightDb);
    }

    /** Called from processBlock to update position/bpm from getPlayHead() */
    void setPosition(float positionSeconds, float bpm)
    {
        currentPosition = positionSeconds;
        currentBpm      = bpm;
    }

private:
    // juce::Timer callback — broadcasts position + meters at 30ms intervals
    void timerCallback() override;

    //==============================================================================
    // Called when OSC message is received from DAW
    void onOscReceived(const juce::String& address, float value);

    // WebSocket message handler (receives from UI)
    void handleWebSocketMessage(const nlohmann::json& message);

    // Connection handler
    void handleClientConnection(int clientId, bool connected);

    // DAW state (for broadcasting)
    bool currentIsPlaying = false;
    bool currentIsRecording = false;
    float currentBpm = 120.0f;
    float currentPosition = 0.0f;
    juce::uint32 lastTimerTimeMs = 0;

    // OSC Handler (receives from DAW)
    std::unique_ptr<OscHandler> oscHandler;

    // WebSocket Server (sends to UI)
    std::unique_ptr<WebSocketServer> wsServer;

    // Ports
    int oscPort;
    int wsPort;

    // Error tracking
    juce::String lastError;

    // Message dispatch helpers
    void dispatchDawCommand(const nlohmann::json& payload);
    void dispatchDawRequest(const nlohmann::json& payload, const juce::String& reqId);
    void dispatchAiPrompt(const nlohmann::json& payload, const juce::String& reqId);
    void dispatchWidgetChange(const nlohmann::json& payload);
    void dispatchMidiLearn(const juce::String& msgType, const nlohmann::json& payload,
                           const juce::String& reqId);
    void dispatchConfig(const nlohmann::json& payload, const juce::String& reqId);
    void dispatchOscSend(const nlohmann::json& payload);
    void dispatchAiAction(const nlohmann::json& payload, const juce::String& reqId);
    void dispatchChainGet(const juce::String& reqId);
    void dispatchChainSet(const nlohmann::json& payload);

    // AI Engine reference (for ai.prompt messages)
    AiEngine*       aiEngine       = nullptr;
    MidiHandler*    midiHandler    = nullptr;
    ParameterMapper* paramMapper   = nullptr;
    PluginChain*     pluginChain   = nullptr;
    SessionManager* sessionManager = nullptr;
    std::atomic<bool> aiProcessing {false};

    // AI thread (async dispatch)
    std::unique_ptr<std::thread> aiThread;

    // Meter state written from processBlock, read by timerCallback
    std::atomic<float> lastMeterL { -60.0f };
    std::atomic<float> lastMeterR { -60.0f };
    int meterTickCounter { 0 }; // only broadcast meter every N timer ticks

    // Analyzer state (written from processBlock, broadcast periodically)
    std::atomic<float> lastCorrelation { 0.0f };
    std::atomic<float> lastLoudness { -96.0f };
    std::vector<float> lastSpectrum;
    juce::CriticalSection spectrumLock;

    // Audio device info (set from prepareToPlay via broadcastPluginStats)
    double lastSampleRate { 44100.0 };
    int    lastBufferSize  { 512 };

    // JSON message builders
    nlohmann::json makeDawTransport();
    nlohmann::json makeOscMessage(const juce::String& address, float value);
    
    // Generate UUID for message IDs
    juce::String generateUUID();

    // Session protocol
    void dispatchSessionGet(const juce::String& reqId);
    void broadcastSessionEvent(const std::string& eventType, const nlohmann::json& data);

    // Logging
    void log(const juce::String& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscBridge)
};