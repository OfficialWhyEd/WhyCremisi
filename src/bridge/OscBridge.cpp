/*
  ==============================================================================
  OscBridge.cpp
  WhyCremisi VST Plugin - Bidirectional OSC-WebSocket Bridge Implementation

  Bridges OSC (UDP) from DAW to WebSocket (TCP) for React UI.
  ==============================================================================
*/

#include "OscBridge.h"
#include "AiEngine.h"
#include "SessionManager.h"
#include "MidiHandler.h"
#include "ParameterMapper.h"
#include "PluginChain.h"
#include <chrono>
#include <random>

//==============================================================================
OscBridge::OscBridge(int oscReceivePort, int wsListenPort)
    : oscPort(oscReceivePort), wsPort(wsListenPort)
{
    oscHandler = std::make_unique<OscHandler>(oscPort);
    wsServer = std::make_unique<WebSocketServer>(wsPort);

    // OSC → WebSocket: set callback for incoming OSC from DAW
    oscHandler->setCallback([this](const juce::String& address, float value) {
        onOscReceived(address, value);
    });

    // WebSocket → OSC: set callback for incoming messages from UI
    wsServer->setMessageCallback([this](const nlohmann::json& msg) {
        handleWebSocketMessage(msg);
    });

    // WebSocket connection handling
    wsServer->setConnectionCallback([this](int clientId, bool connected) {
        handleClientConnection(clientId, connected);
    });
}

OscBridge::~OscBridge()
{
    stop();
}

//==============================================================================
bool OscBridge::start()
{
    // Start OSC listener (receives from DAW).
    // start() launches a thread; give it a moment to bind the socket before
    // checking isRunning(), which reads the 'connected' atomic set by that thread.
    oscHandler->start();
    juce::Thread::sleep(50);
    if (!oscHandler->isRunning())
    {
        lastError = "Failed to start OSC listener on port " + juce::String(oscPort);
        log("[ERROR] " + lastError);
        return false;
    }
    log("[OSC] Listening on port " + juce::String(oscPort));

    // Start WebSocket server (accepts connections from UI)
    if (!wsServer->start())
    {
        lastError = "Failed to start WebSocket server on port " + juce::String(wsPort);
        log("[ERROR] " + lastError);
        oscHandler->stop();
        return false;
    }
    log("[WebSocket] Listening on port " + juce::String(wsPort));

    log("[OscBridge] Started successfully");
    log("[OscBridge] DAW target: " + oscHandler->getSendHost() + ":" + juce::String(oscHandler->getSendPort()));

    // 33ms timer: broadcasts position ticker + meters to UI
    startTimer(33);

    return true;
}

void OscBridge::stop()
{
    stopTimer();
    if (wsServer)
        wsServer->stop();
    if (oscHandler)
        oscHandler->stop();
    
    if (aiThread && aiThread->joinable())
    {
        log("[OscBridge] Waiting for AI thread to join...");
        aiThread->join();
    }
    
    log("[OscBridge] Stopped");
}

//==============================================================================
void OscBridge::timerCallback()
{
    if (!wsServer || !wsServer->isRunning() || wsServer->getConnectedClientsCount() == 0)
        return;

    // Advance position using real elapsed time to prevent drift
    auto now = juce::Time::getMillisecondCounter();
    if (currentIsPlaying)
    {
        if (lastTimerTimeMs != 0)
            currentPosition += (now - lastTimerTimeMs) / 1000.0f;
        // Broadcast transport every ~1s (every 30 ticks)
        if (++meterTickCounter % 30 == 0)
            broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    lastTimerTimeMs = now;

    // Always broadcast meter at ~30fps
    broadcastMeter(-1, lastMeterL.load(), lastMeterR.load(),
                       lastMeterL.load(), lastMeterR.load());

    // Broadcast analyzer data every 10 ticks (~330ms)
    if (meterTickCounter % 10 == 0)
    {
        float corr = lastCorrelation.load();
        float loud = lastLoudness.load();

        nlohmann::json ana;
        ana["type"] = "daw.analyzer";
        ana["timestamp"] = juce::Time::currentTimeMillis();
        ana["payload"]["correlation"] = corr;
        ana["payload"]["loudness"] = loud;

        // Send first 256 bins of spectrum
        nlohmann::json specArr = nlohmann::json::array();
        int numBins = juce::jmin(256, (int)lastSpectrum.size());
        for (int i = 0; i < numBins; ++i)
            specArr.push_back(lastSpectrum[i]);
        ana["payload"]["spectrum"] = specArr;

        wsServer->broadcast(ana);
    }
}

bool OscBridge::isRunning() const
{
    return oscHandler && oscHandler->isRunning() &&
           wsServer && wsServer->isRunning();
}

//==============================================================================
// OscHandler::OscCallback - receives OSC from DAW
//==============================================================================
void OscBridge::onOscReceived(const juce::String& address, float value)
{
    log("[OSC→WS] " + address + " = " + juce::String(value, 3));

    // Rate-limited session logging (SessionManager handles the interval itself)
    if (sessionManager)
        sessionManager->logOscEvent(address, value);

    // ── AbletonOSC exact addresses ──────────────────────────────────────────
    // Ableton sends these after /live/song/get/* queries or live.add_listener

    if (address == "/live/song/get/is_playing" || address == "/live/song/is_playing")
    {
        currentIsPlaying = (value > 0.5f);
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        broadcastSessionEvent("transport", {{"is_playing", currentIsPlaying}, {"bpm", currentBpm}});
    }
    else if (address == "/live/song/get/is_recording" || address == "/live/song/is_recording")
    {
        currentIsRecording = (value > 0.5f);
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address == "/live/song/get/tempo" || address == "/live/song/tempo")
    {
        currentBpm = value;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address == "/live/song/get/current_song_time" || address == "/live/song/current_song_time")
    {
        currentPosition = value;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        // position logged by SessionManager's rate limiter
        if (sessionManager) sessionManager->logOscEvent(address, value);
        return; // already logged as OSC above, skip generic OSC log below
    }
    else if (address == "/live/song/start_playing")
    {
        currentIsPlaying = true;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(true, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address == "/live/song/stop_playing")
    {
        currentIsPlaying = false;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(false, currentIsRecording, currentBpm, currentPosition);
    }
    // ── REAPER / generic OSC transport fallback ─────────────────────────────
    else if (address == "/play" || (address.contains("play") && !address.contains("back")))
    {
        currentIsPlaying = (value > 0.5f);
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address == "/stop")
    {
        currentIsPlaying = false;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(false, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address == "/record" || address.contains("record"))
    {
        currentIsRecording = (value > 0.5f);
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address.contains("tempo") || address.contains("bpm"))
    {
        currentBpm = value;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        if (sessionManager) sessionManager->logTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    else if (address.contains("song_time") || address.contains("position"))
    {
        currentPosition = value;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
    }
    // ── Track data → forward to UI ──────────────────────────────────────────
    else
    {
        // REAPER-specific track parameter handling
        auto extractTrackId = [&](const juce::String& suffix) -> int {
            juce::String idStr = address.fromFirstOccurrenceOf("/track/", false, true);
            idStr = idStr.upToLastOccurrenceOf(suffix, false, true);
            return idStr.getIntValue();
        };

        int trackId = 0;
        bool handled = true;

        if (address.startsWith("/track/") && address.endsWith("/volume"))
            trackId = extractTrackId("/volume");
        else if (address.startsWith("/track/") && address.endsWith("/pan"))
            trackId = extractTrackId("/pan");
        else if (address.startsWith("/track/") && address.endsWith("/mute"))
            trackId = extractTrackId("/mute");
        else if (address.startsWith("/track/") && address.endsWith("/solo"))
            trackId = extractTrackId("/solo");
        else
            handled = false;

        if (handled)
        {
            if (trackId < 1)
                log("[OSC] Warning: invalid track ID in address: " + address);
            forwardOscToUI(address, value);
        }
        else
        {
            forwardOscToUI(address, value);
        }
    }
}

//==============================================================================
// WebSocket message handler - receives from UI
//==============================================================================
void OscBridge::handleWebSocketMessage(const nlohmann::json& message)
{
    if (!message.contains("type"))
    {
        log("[ERROR] WebSocket message without 'type'");
        return;
    }

    std::string type = message["type"];
    juce::String msgType(type.data(), type.size());
    juce::String reqId = message.contains("id") && !message["id"].is_null()
                         ? juce::String(message["id"].get<std::string>().data())
                         : "";

    log("[WS→OSC] " + msgType + (reqId.isNotEmpty() ? " (id=" + reqId + ")" : ""));

    // Dispatch based on type
    if (msgType == "plugin.init")
    {
        // Respond with plugin.init FIRST so client knows we're ready
        nlohmann::json init;
        init["type"] = "plugin.init";
        init["id"] = nullptr;
        init["timestamp"] = juce::Time::currentTimeMillis();
        init["payload"]["version"] = "1.0.0";
        init["payload"]["oscPort"] = oscPort;
        init["payload"]["wsPort"] = wsPort;
        init["payload"]["capabilities"] = nlohmann::json::array({"widgets", "ai", "osc", "daw"});
        wsServer->broadcast(init);

        // Then push current DAW state
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);

        log("[WS] Sent plugin.init + transport to client");
    }
    else if (msgType == "daw.command")
    {
        if (message.contains("payload"))
            dispatchDawCommand(message["payload"]);
    }
    else if (msgType == "daw.request")
    {
        if (message.contains("payload"))
            dispatchDawRequest(message["payload"], reqId);
    }
    else if (msgType == "ai.prompt")
    {
        if (message.contains("payload"))
            dispatchAiPrompt(message["payload"], reqId);
    }
    else if (msgType == "widget.valueChange")
    {
        if (message.contains("payload"))
            dispatchWidgetChange(message["payload"]);
    }
    else if (msgType == "config.get" || msgType == "config.set")
    {
        if (message.contains("payload"))
            dispatchConfig(message["payload"], reqId);
    }
    else if (msgType == "osc.send")
    {
        if (message.contains("payload"))
            dispatchOscSend(message["payload"]);
    }
    else if (msgType == "session.get")
    {
        dispatchSessionGet(reqId);
    }
    else if (msgType == "midi.learn.start" || msgType == "midi.learn.stop")
    {
        if (message.contains("payload"))
            dispatchMidiLearn(msgType, message["payload"], reqId);
    }
    else if (msgType == "ai.action")
    {
        if (message.contains("payload"))
            dispatchAiAction(message["payload"], reqId);
    }
    else if (msgType == "chain.get")
    {
        dispatchChainGet(reqId);
    }
    else if (msgType == "chain.set")
    {
        if (message.contains("payload"))
            dispatchChainSet(message["payload"]);
    }
    else
    {
        log("[WARN] Unknown message type: " + msgType);
    }
}

//==============================================================================
void OscBridge::handleClientConnection(int clientId, bool connected)
{
    if (connected)
    {
        log("[WS] Client " + juce::String(clientId) + " connected");

        if (sessionManager) sessionManager->logWsConnect(clientId, true);

        // Push current state to the newly connected client
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);

        // Push real audio device stats (SR, buffer, latency)
        broadcastPluginStats(lastSampleRate, lastBufferSize);

        // Request fresh transport state from Ableton (triggers OSC responses)
        sendOscToDaw("/live/song/get/is_playing", 0.0f);
        sendOscToDaw("/live/song/get/tempo", 0.0f);
        sendOscToDaw("/live/song/get/current_song_time", 0.0f);
    }
    else
    {
        log("[WS] Client " + juce::String(clientId) + " disconnected");
        if (sessionManager) sessionManager->logWsConnect(clientId, false);
    }
}

//==============================================================================
// DAW command dispatcher (UI → DAW)
//==============================================================================
void OscBridge::dispatchDawCommand(const nlohmann::json& payload)
{
    if (!payload.contains("command"))
        return;

    std::string cmd = payload["command"];
    juce::String command(cmd.data(), cmd.size());

    log("[CMD] " + command);

    if (sessionManager)
        sessionManager->logDawCommand(command, juce::String(payload.dump().data()));

    broadcastSessionEvent("daw_command", {{"command", command.toStdString()}});

    // AbletonOSC + REAPER dual-send + optimistic local state update.
    // Optimistic update: change internal state and broadcast immediately so
    // the UI is responsive even before DAW feedback arrives over OSC.
    if (command == "play")
    {
        currentIsPlaying = true;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendOscToDaw("/live/song/start_playing", 1.0f);  // AbletonOSC
        sendOscToDaw("/play", 1.0f);                      // REAPER
    }
    else if (command == "stop")
    {
        currentIsPlaying = false;
        currentPosition  = 0.0f;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendOscToDaw("/live/song/stop_playing", 1.0f);   // AbletonOSC
        sendOscToDaw("/stop", 1.0f);                      // REAPER
    }
    else if (command == "record")
    {
        currentIsRecording = !currentIsRecording;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendOscToDaw("/live/song/record", 1.0f);          // AbletonOSC
        sendOscToDaw("/record", 1.0f);                    // REAPER
    }
    else if (command == "pause")
    {
        currentIsPlaying = false;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendOscToDaw("/live/song/continue_playing", 1.0f);
        sendOscToDaw("/pause", 1.0f);
    }
    else if (command == "setTempo")
    {
        if (payload.contains("bpm"))
        {
            float bpm = payload["bpm"].get<float>();
            currentBpm = bpm;
            broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
            sendOscToDaw("/live/song/set/tempo", bpm);    // AbletonOSC
            sendOscToDaw("/tempo", bpm);                   // REAPER
        }
    }
    else if (command == "setVolume")
    {
        if (payload.contains("trackId") && payload.contains("valueDb"))
        {
            int trackId = payload["trackId"].get<int>();
            float db = payload["valueDb"].get<float>();
            // AbletonOSC uses 0-1 linear; convert from dB
            float linear = juce::Decibels::decibelsToGain(db);
            sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/volume", linear);
            sendOscToDaw("/track/" + juce::String(trackId) + "/volume", db);
        }
    }
    else if (command == "setPan")
    {
        if (payload.contains("trackId") && payload.contains("value"))
        {
            int trackId = payload["trackId"].get<int>();
            float pan = payload["value"].get<float>();
            sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/panning", pan);
            sendOscToDaw("/track/" + juce::String(trackId) + "/pan", pan);
        }
    }
    else if (command == "muteTrack")
    {
        if (payload.contains("trackId") && payload.contains("muted"))
        {
            int trackId = payload["trackId"].get<int>();
            float muted = payload["muted"].get<bool>() ? 1.0f : 0.0f;
            sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/mute", muted);
            sendOscToDaw("/track/" + juce::String(trackId) + "/mute", muted);
        }
    }
    else if (command == "soloTrack")
    {
        if (payload.contains("trackId") && payload.contains("soloed"))
        {
            int trackId = payload["trackId"].get<int>();
            float soloed = payload["soloed"].get<bool>() ? 1.0f : 0.0f;
            sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/solo", soloed);
            sendOscToDaw("/track/" + juce::String(trackId) + "/solo", soloed);
        }
    }
    else if (command == "requestTransport")
    {
        // Poll AbletonOSC for current state
        sendOscToDaw("/live/song/get/is_playing", 0.0f);
        sendOscToDaw("/live/song/get/tempo", 0.0f);
        sendOscToDaw("/live/song/get/current_song_time", 0.0f);
    }
    else if (command == "setGain")
    {
        // Master gain (plugin internal, also send to track 0 in Ableton)
        if (payload.contains("valueDb"))
        {
            float db = payload["valueDb"].get<float>();
            float linear = juce::Decibels::decibelsToGain(db);
            sendOscToDaw("/live/master_track/set/volume", linear);
        }
    }
    else if (command == "setDrive")
    {
        // Generic drive/saturation — forward as custom OSC param
        if (payload.contains("value"))
        {
            float v = payload["value"].get<float>();
            sendOscToDaw("/whycremisi/drive", v);
        }
    }
    else
    {
        log("[WARN] Unknown DAW command: " + command);
    }
}

//==============================================================================
void OscBridge::dispatchDawRequest(const nlohmann::json& payload, const juce::String& reqId)
{
    if (!payload.contains("request"))
        return;

    std::string req = payload["request"];
    juce::String request(req.data(), req.size());

    log("[REQ] " + request);

    nlohmann::json response;
    response["type"] = "daw.response";
    response["id"] = reqId.toStdString();
    response["timestamp"] = juce::Time::currentTimeMillis();

    if (request == "transport")
    {
        response["payload"]["isPlaying"] = currentIsPlaying;
        response["payload"]["isRecording"] = currentIsRecording;
        response["payload"]["bpm"] = currentBpm;
        response["payload"]["positionSeconds"] = currentPosition;
    }
    else if (request == "trackInfo")
    {
        int trackId = payload.contains("trackId") ? payload["trackId"].get<int>() : 1;
        response["payload"]["trackId"] = trackId;
        response["payload"]["name"] = std::string("Track ") + std::to_string(trackId);
        response["payload"]["volumeDb"] = 0.0;
        response["payload"]["pan"] = 0.0;
        response["payload"]["isMuted"] = false;
        response["payload"]["isSoloed"] = false;
    }
    else
    {
        response["payload"]["error"] = "Unknown request: " + request.toStdString();
    }

    wsServer->broadcast(response);
}

//==============================================================================
void OscBridge::dispatchAiPrompt(const nlohmann::json& payload, const juce::String& reqId)
{
    if (!payload.contains("prompt"))
        return;

    std::string promptStr = payload["prompt"];
    juce::String prompt(promptStr.data(), promptStr.size());

    log("[AI] Prompt received (id=" + reqId + "): " + prompt.substring(0, 50));

    if (!aiEngine)
    {
        nlohmann::json response;
        response["type"] = "ai.response";
        response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : generateUUID().toStdString();
        response["timestamp"] = juce::Time::currentTimeMillis();
        response["payload"]["status"] = "error";
        response["payload"]["provider"] = "none";
        response["payload"]["content"] = "AI engine not initialized";
        wsServer->broadcast(response);
        return;
    }

    // Log prompt to session before going async
    if (sessionManager)
        sessionManager->logAiPrompt(prompt, aiEngine->getProviderName());

    broadcastSessionEvent("ai_prompt", {{"prompt", prompt.substring(0, 120).toStdString()},
                                        {"provider", aiEngine->getProviderName().toStdString()}});

    juce::String activeReqId = reqId.isNotEmpty() ? reqId : generateUUID();

    // Broadcast "thinking" immediately so UI can react
    {
        nlohmann::json status;
        status["type"] = "ai.response";
        status["id"] = activeReqId.toStdString();
        status["timestamp"] = juce::Time::currentTimeMillis();
        status["payload"]["status"] = "thinking";
        status["payload"]["provider"] = aiEngine->getProviderName().toStdString();
        status["payload"]["content"] = "";
        wsServer->broadcast(status);
    }

    if (aiProcessing.load())
    {
        // Already processing — queue not supported, reject gracefully
        nlohmann::json busy;
        busy["type"] = "ai.response";
        busy["id"] = activeReqId.toStdString();
        busy["timestamp"] = juce::Time::currentTimeMillis();
        busy["payload"]["status"] = "error";
        busy["payload"]["content"] = "AI is already processing a request. Please wait.";
        wsServer->broadcast(busy);
        return;
    }

    aiProcessing.store(true);

    // Join the previous finished thread if it exists to avoid leaking handles
    if (aiThread && aiThread->joinable())
        aiThread->join();

    // Capture everything needed by value — the thread outlives this stack frame
    juce::String capturedPrompt = prompt;
    juce::String capturedReqId  = activeReqId;
    int64_t      startMs        = juce::Time::currentTimeMillis();

    aiThread = std::make_unique<std::thread>([this, capturedPrompt, capturedReqId, startMs]()
    {
        auto structured = aiEngine->sendPromptStructured(capturedPrompt);
        bool success = structured.success;
        int durationMs = static_cast<int>(juce::Time::currentTimeMillis() - startMs);

        aiProcessing.store(false);

        // Log the response to session
        if (sessionManager)
            sessionManager->logAiResponse(structured.text, durationMs);

        broadcastSessionEvent("ai_response", {{"preview", structured.text.substring(0, 80).toStdString()},
                                              {"duration_ms", durationMs},
                                              {"success", success}});

        log("[AI] Response in " + juce::String(durationMs) + "ms: " +
            structured.text.substring(0, 60) + (structured.text.length() > 60 ? "..." : ""));

        nlohmann::json response;
        response["type"] = "ai.response";
        response["id"] = capturedReqId.toStdString();
        response["timestamp"] = juce::Time::currentTimeMillis();
        response["payload"]["status"] = success ? "success" : "error";
        response["payload"]["provider"] = aiEngine->getProviderName().toStdString();
        response["payload"]["model"]    = aiEngine->getModelName().toStdString();
        response["payload"]["content"]  = structured.text.toStdString();
        response["payload"]["durationMs"] = durationMs;

        // Include actions in response for UI
        nlohmann::json actionsArray = nlohmann::json::array();
        for (const auto& a : structured.actions)
        {
            nlohmann::json act;
            act["widgetId"] = a.widgetId.toStdString();
            act["value"] = a.value;
            act["previousValue"] = a.previousValue;
            act["description"] = a.description.toStdString();
            actionsArray.push_back(act);

            // Log to session
            if (sessionManager)
                sessionManager->logAiAction(a.widgetId, a.value, a.previousValue, a.description);

            // Broadcast each action separately for action log
            nlohmann::json actionLog;
            actionLog["type"] = "ai.action.log";
            actionLog["timestamp"] = juce::Time::currentTimeMillis();
            actionLog["payload"]["widgetId"] = a.widgetId.toStdString();
            actionLog["payload"]["value"] = a.value;
            actionLog["payload"]["previousValue"] = a.previousValue;
            actionLog["payload"]["description"] = a.description.toStdString();
            wsServer->broadcast(actionLog);
        }
        response["payload"]["actions"] = actionsArray;

        wsServer->broadcast(response);
    });
}

//==============================================================================
void OscBridge::dispatchWidgetChange(const nlohmann::json& payload)
{
    if (!payload.contains("widgetId") || !payload.contains("value"))
        return;

    juce::String widgetId = juce::String(payload["widgetId"].get<std::string>().data());
    float value = payload["value"].get<float>();

    log("[WIDGET] " + widgetId + " = " + juce::String(value));

    if (widgetChangeCallback)
        widgetChangeCallback(widgetId, value);
}

//==============================================================================
void OscBridge::dispatchMidiLearn(const juce::String& msgType,
                                   const nlohmann::json& payload,
                                   const juce::String& reqId)
{
    if (msgType == "midi.learn.start")
    {
        if (!payload.contains("widgetId"))
            return;

        juce::String widgetId = juce::String(payload["widgetId"].get<std::string>().data());
        log("[MIDI] Learn start for widget: " + widgetId);

        if (midiHandler)
            midiHandler->startLearn(widgetId);

        nlohmann::json response;
        response["type"] = "midi.learn.status";
        response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : nullptr;
        response["timestamp"] = juce::Time::currentTimeMillis();
        response["payload"]["widgetId"] = widgetId.toStdString();
        response["payload"]["status"] = "listening";
        wsServer->broadcast(response);
    }
    else if (msgType == "midi.learn.stop")
    {
        if (midiHandler)
            midiHandler->stopLearn();

        nlohmann::json response;
        response["type"] = "midi.learn.status";
        response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : nullptr;
        response["timestamp"] = juce::Time::currentTimeMillis();
        response["payload"]["status"] = "cancelled";
        wsServer->broadcast(response);

        log("[MIDI] Learn stopped");
    }
}

//==============================================================================
void OscBridge::dispatchConfig(const nlohmann::json& payload, const juce::String& reqId)
{
    if (!payload.contains("key"))
        return;

    std::string key = payload["key"];
    juce::String configKey(key.data(), key.size());
    bool isRead = !payload.contains("value");

    // Helper to send a config.response
    auto sendConfigResponse = [&](nlohmann::json responsePayload) {
        nlohmann::json response;
        response["type"] = "config.response";
        response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : nullptr;
        response["timestamp"] = juce::Time::currentTimeMillis();
        response["payload"] = responsePayload;
        wsServer->broadcast(response);
    };

    // ── Read path (config.get) ──────────────────────────────────
    if (isRead)
    {
        nlohmann::json rp;
        rp["key"] = key;

        if (configKey == "ai.provider")
        {
            rp["value"] = aiEngine ? aiEngine->getProviderName().toStdString() : "none";
        }
        else if (configKey == "ai.model")
        {
            rp["value"] = aiEngine ? aiEngine->getModelName().toStdString() : "none";
        }
        else if (configKey == "ai.getModels")
        {
            juce::StringArray models = aiEngine ? aiEngine->getAvailableModels() : juce::StringArray{"Not configured"};
            nlohmann::json modelArray = nlohmann::json::array();
            for (const auto& model : models)
                modelArray.push_back(model.toStdString());
            rp["models"] = modelArray;
            rp["provider"] = aiEngine ? aiEngine->getProviderName().toStdString() : "none";
        }
        else
        {
            rp["status"] = "unknown_key";
            rp["value"] = nullptr;
        }

        rp["status"] = "ok";
        sendConfigResponse(rp);
        return;
    }

    // ── Write path (config.set) ─────────────────────────────────
    if (configKey == "ai.provider")
    {
        std::string provider = payload["value"];
        log("[CONFIG] AI provider set to: " + juce::String(provider.data()));
        
        if (aiEngine)
        {
            AiEngine::Config cfg;
            cfg.baseUrl = "http://localhost:11434";
            cfg.model = "llama3.2";
            
            if (provider == "ollama")
                cfg.provider = AiEngine::Provider::Ollama;
            else if (provider == "gemini")
                cfg.provider = AiEngine::Provider::Gemini;
            else if (provider == "anthropic")
                cfg.provider = AiEngine::Provider::Anthropic;
            else if (provider == "openai")
                cfg.provider = AiEngine::Provider::OpenAI;
            else if (provider == "openrouter")
                cfg.provider = AiEngine::Provider::OpenRouter;
            else if (provider == "groq")
                cfg.provider = AiEngine::Provider::Groq;
            
            aiEngine->configure(cfg);
        }
        
        nlohmann::json rp;
        rp["key"] = "ai.provider";
        rp["value"] = provider;
        rp["status"] = "ok";
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.model")
    {
        std::string model = payload["value"];
        log("[CONFIG] AI model set to: " + juce::String(model.data()));
        
        if (aiEngine)
        {
            AiEngine::Config cfg;
            cfg.model = juce::String(model.data());
            aiEngine->configure(cfg);
        }
        
        nlohmann::json rp;
        rp["key"] = "ai.model";
        rp["value"] = model;
        rp["status"] = "ok";
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.apiKey" && payload.contains("provider"))
    {
        std::string provider = payload["provider"];
        std::string apiKey = payload["value"];
        log("[CONFIG] API key set for: " + juce::String(provider.data()));
        
        if (aiEngine)
        {
            AiEngine::Config cfg;
            cfg.apiKey = juce::String(apiKey.data());
            aiEngine->configure(cfg);
        }
        
        nlohmann::json rp;
        rp["key"] = "ai.apiKey";
        rp["provider"] = provider;
        rp["status"] = "ok";
        rp["masked"] = true;
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.ollamaUrl")
    {
        std::string url = payload["value"];
        log("[CONFIG] Ollama URL set to: " + juce::String(url.data()));
        
        if (aiEngine)
        {
            AiEngine::Config cfg;
            cfg.baseUrl = juce::String(url.data());
            cfg.provider = AiEngine::Provider::Ollama;
            aiEngine->configure(cfg);
        }
        
        nlohmann::json rp;
        rp["key"] = "ai.ollamaUrl";
        rp["value"] = url;
        rp["status"] = "ok";
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.testConnection")
    {
        bool connected = aiEngine ? aiEngine->testConnection() : false;
        juce::String errorMsg = aiEngine ? aiEngine->getLastError() : "AI engine not initialized";
        
        nlohmann::json rp;
        rp["key"] = "ai.testConnection";
        rp["connected"] = connected;
        rp["error"] = errorMsg.toStdString();
        sendConfigResponse(rp);
        
        log("[CONFIG] AI test connection: " + juce::String(connected ? "SUCCESS" : "FAILED"));
    }
    else if (configKey == "ai.getModels")
    {
        juce::StringArray models = aiEngine ? aiEngine->getAvailableModels() : juce::StringArray{"Not configured"};
        nlohmann::json modelArray = nlohmann::json::array();
        for (const auto& model : models)
            modelArray.push_back(model.toStdString());
        
        nlohmann::json rp;
        rp["key"] = "ai.getModels";
        rp["models"] = modelArray;
        rp["provider"] = aiEngine ? aiEngine->getProviderName().toStdString() : "none";
        sendConfigResponse(rp);
    }
    else if (configKey == "osc.port")
    {
        int newPort = payload["value"].get<int>();
        log("[CONFIG] Would change OSC port to " + juce::String(newPort));
    }
    else
    {
        log("[CONFIG] Generic key: " + configKey);

        nlohmann::json rp;
        rp["key"] = configKey.toStdString();
        rp["status"] = "ok";
        rp["value"] = payload["value"];
        sendConfigResponse(rp);
    }
}

//==============================================================================
void OscBridge::dispatchOscSend(const nlohmann::json& payload)
{
    if (!payload.contains("address") || !payload.contains("value"))
        return;

    std::string addr = payload["address"];
    juce::String address(addr.data(), addr.size());
    float value = payload["value"].get<float>();

    sendOscToDaw(address, value);
}

//==============================================================================
void OscBridge::dispatchSessionGet(const juce::String& reqId)
{
    if (!sessionManager)
        return;

    nlohmann::json resp;
    resp["type"] = "session.data";
    resp["id"]   = reqId.isNotEmpty() ? reqId.toStdString() : "";
    resp["timestamp"] = juce::Time::currentTimeMillis();

    // Load snapshot from current.json
    auto currentFile = sessionManager->getCurrentSessionFile();
    nlohmann::json payload;

    if (currentFile.existsAsFile())
    {
        try { payload = nlohmann::json::parse(currentFile.loadFileAsString().toStdString()); }
        catch (...) {}
    }

    // Load recent events from events.jsonl (last 200 lines)
    auto eventsFile = sessionManager->getActiveSessionDir().getChildFile("events.jsonl");
    auto eventsArr  = nlohmann::json::array();

    if (eventsFile.existsAsFile())
    {
        juce::StringArray lines;
        lines.addLines(eventsFile.loadFileAsString());

        // Iterate last 200 non-empty lines
        int start = juce::jmax(0, lines.size() - 200);
        for (int i = start; i < lines.size(); ++i)
        {
            auto line = lines[i].trim();
            if (line.isEmpty()) continue;
            try
            {
                eventsArr.push_back(nlohmann::json::parse(line.toStdString()));
            }
            catch (...) {}
        }
    }

    payload["events"]     = eventsArr;
    payload["session_dir"] = sessionManager->getActiveSessionDir().getFullPathName().toStdString();
    payload["started_at_ms"] = 0; // filled from current.json if present

    resp["payload"] = payload;
    wsServer->broadcast(resp);
    log("[SESSION] Sent session.data (" + juce::String((int)eventsArr.size()) + " events)");
}

//==============================================================================
void OscBridge::broadcastPluginStats(double sampleRate, int bufferSize)
{
    // Store for new clients that connect later
    lastSampleRate = sampleRate;
    lastBufferSize = bufferSize;

    if (!wsServer || !wsServer->isRunning()) return;

    double latencyMs = (bufferSize > 0 && sampleRate > 0)
                       ? (bufferSize / sampleRate) * 1000.0
                       : 0.0;

    nlohmann::json msg;
    msg["type"] = "plugin.stats";
    msg["id"]   = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["sampleRate"]  = sampleRate;
    msg["payload"]["bufferSize"]  = bufferSize;
    msg["payload"]["latencyMs"]   = latencyMs;

    wsServer->broadcast(msg);
    log("[STATS] SR=" + juce::String(sampleRate, 0) +
        " BUF=" + juce::String(bufferSize) +
        " LAT=" + juce::String(latencyMs, 2) + "ms");
}

void OscBridge::updateAnalyzer(float correlation, float loudness, const std::vector<float>& spectrum)
{
    lastCorrelation.store(correlation);
    lastLoudness.store(loudness);
    lastSpectrum = spectrum;
}

void OscBridge::broadcastSessionEvent(const std::string& eventType, const nlohmann::json& data)
{
    if (!wsServer || !wsServer->isRunning() || wsServer->getConnectedClientsCount() == 0)
        return;

    nlohmann::json msg;
    msg["type"] = "session.event";
    msg["id"]   = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["event_type"] = eventType;
    msg["payload"]["data"] = data;
    wsServer->broadcast(msg);
}

//==============================================================================
// Broadcast helpers (DAW → UI)
//==============================================================================
nlohmann::json OscBridge::makeDawTransport()
{
    nlohmann::json msg;
    msg["type"] = "daw.transport";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["isPlaying"] = currentIsPlaying;
    msg["payload"]["isRecording"] = currentIsRecording;
    msg["payload"]["bpm"] = currentBpm;
    msg["payload"]["positionSeconds"] = currentPosition;
    msg["payload"]["timeSignature"] = nlohmann::json{{"numerator", 4}, {"denominator", 4}};
    return msg;
}

nlohmann::json OscBridge::makeOscMessage(const juce::String& address, float value)
{
    nlohmann::json msg;
    msg["type"] = "osc.message";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["address"] = address.toStdString();
    msg["payload"]["value"] = value;
    msg["payload"]["valueType"] = "float";
    return msg;
}

void OscBridge::broadcastTransport(bool isPlaying, bool isRecording, float bpm, float positionSeconds)
{
    nlohmann::json msg;
    msg["type"] = "daw.transport";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["isPlaying"] = isPlaying;
    msg["payload"]["isRecording"] = isRecording;
    msg["payload"]["bpm"] = bpm;
    msg["payload"]["positionSeconds"] = positionSeconds;
    msg["payload"]["timeSignature"] = nlohmann::json{{"numerator", 4}, {"denominator", 4}};

    wsServer->broadcast(msg);
}

void OscBridge::broadcastTrackUpdate(int trackId, const juce::String& name, float volumeDb, float pan,
                                    bool isMuted, bool isSoloed)
{
    nlohmann::json msg;
    msg["type"] = "daw.track";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["trackId"] = trackId;
    msg["payload"]["name"] = name.toStdString();
    msg["payload"]["volumeDb"] = volumeDb;
    msg["payload"]["pan"] = pan;
    msg["payload"]["isMuted"] = isMuted;
    msg["payload"]["isSoloed"] = isSoloed;

    wsServer->broadcast(msg);
}

void OscBridge::broadcastMeter(int trackId, float leftDb, float rightDb, float peakLeftDb, float peakRightDb)
{
    nlohmann::json msg;
    msg["type"] = "daw.meter";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["trackId"] = trackId;
    msg["payload"]["leftDb"] = leftDb;
    msg["payload"]["rightDb"] = rightDb;
    msg["payload"]["peakLeftDb"] = peakLeftDb;
    msg["payload"]["peakRightDb"] = peakRightDb;

    wsServer->broadcast(msg);
}

void OscBridge::broadcastAiResponse(const juce::String& requestId, const juce::String& content,
                                   const juce::String& provider, bool isComplete)
{
    nlohmann::json msg;
    msg["type"] = "ai.response";
    msg["id"] = requestId.isNotEmpty() ? requestId.toStdString() : nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["status"] = isComplete ? "success" : "pending";
    msg["payload"]["provider"] = provider.toStdString();
    msg["payload"]["content"] = content.toStdString();

    wsServer->broadcast(msg);
}

void OscBridge::broadcastAiStream(const juce::String& requestId, const juce::String& chunk, bool isDone)
{
    nlohmann::json msg;
    msg["type"] = "ai.stream";
    msg["id"] = requestId.isNotEmpty() ? requestId.toStdString() : nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["chunk"] = chunk.toStdString();
    msg["payload"]["isDone"] = isDone;

    wsServer->broadcast(msg);
}

void OscBridge::broadcastWidgetCreate(const juce::String& widgetId, const juce::String& widgetType,
                                      const juce::String& title, const nlohmann::json& config)
{
    nlohmann::json msg;
    msg["type"] = "ui.widget.create";
    msg["id"] = widgetId.isNotEmpty() ? widgetId.toStdString() : nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["widgetType"] = widgetType.toStdString();
    msg["payload"]["title"] = title.toStdString();

    for (auto& [key, val] : config.items())
        msg["payload"][key] = val;

    wsServer->broadcast(msg);
}

void OscBridge::broadcastWidgetUpdate(const juce::String& widgetId, const nlohmann::json& values)
{
    nlohmann::json msg;
    msg["type"] = "ui.widget.update";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["widgetId"] = widgetId.toStdString();

    for (auto& [key, val] : values.items())
        msg["payload"][key] = val;

    wsServer->broadcast(msg);
}

void OscBridge::broadcastWidgetRemove(const juce::String& widgetId)
{
    nlohmann::json msg;
    msg["type"] = "ui.widget.remove";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["widgetId"] = widgetId.toStdString();

    wsServer->broadcast(msg);
}

void OscBridge::broadcastError(const juce::String& code, const juce::String& message,
                               const juce::String& severity)
{
    nlohmann::json msg;
    msg["type"] = "plugin.error";
    msg["id"] = nullptr;
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["code"] = code.toStdString();
    msg["payload"]["message"] = message.toStdString();
    msg["payload"]["severity"] = severity.toStdString();

    wsServer->broadcast(msg);
}

void OscBridge::forwardOscToUI(const juce::String& address, float value)
{
    wsServer->broadcast(makeOscMessage(address, value));
}

//==============================================================================
// OSC send to DAW
//==============================================================================
void OscBridge::sendOscToDaw(const juce::String& address, float value)
{
    oscHandler->sendMessage(address, value);
    log("[OSC←WS] SENT: " + address + " = " + juce::String(value, 3));
}

void OscBridge::sendOscToDaw(const juce::String& address, const juce::String& value)
{
    oscHandler->sendMessage(address, value);
    log("[OSC←WS] SENT: " + address + " = \"" + value + "\"");
}

void OscBridge::sendOscToDaw(const juce::String& address, int value)
{
    oscHandler->sendMessage(address, value);
    log("[OSC←WS] SENT: " + address + " = " + juce::String(value));
}

void OscBridge::setDawTarget(const juce::String& host, int sendPort)
{
    oscHandler->setSendTarget(host, sendPort);
    log("[CONFIG] DAW target set to " + host + ":" + juce::String(sendPort));
}

void OscBridge::setAiEngine(AiEngine* engine)
{
    aiEngine = engine;
}

void OscBridge::setMidiHandler(MidiHandler* mh) { midiHandler = mh; }

void OscBridge::setParameterMapper(ParameterMapper* pm) { paramMapper = pm; }

void OscBridge::setPluginChain(PluginChain* pc) { pluginChain = pc; }

void OscBridge::broadcastJson(const nlohmann::json& msg) { wsServer->broadcast(msg); }

void OscBridge::dispatchChainGet(const juce::String& reqId)
{
    nlohmann::json response;
    response["type"] = "chain.response";
    response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : nullptr;
    response["timestamp"] = juce::Time::currentTimeMillis();

    if (pluginChain)
    {
        nlohmann::json pluginsArray = nlohmann::json::array();
        for (const auto& p : pluginChain->getPlugins())
        {
            nlohmann::json jp;
            jp["id"] = p.id.toStdString();
            jp["name"] = p.name.toStdString();
            jp["manufacturer"] = p.manufacturer.toStdString();
            jp["format"] = p.format.toStdString();
            jp["slot"] = p.slot;
            jp["enabled"] = p.enabled;
            pluginsArray.push_back(jp);
        }
        response["payload"]["plugins"] = pluginsArray;
        response["payload"]["status"] = "ok";
    }
    else
    {
        response["payload"]["plugins"] = nlohmann::json::array();
        response["payload"]["status"] = "ok";
    }

    wsServer->broadcast(response);
}

void OscBridge::dispatchChainSet(const nlohmann::json& payload)
{
    if (!pluginChain || !payload.contains("plugins") || !payload["plugins"].is_array())
        return;

    std::vector<PluginInfo> plugins;
    for (const auto& jp : payload["plugins"])
    {
        PluginInfo p;
        p.id           = juce::String(jp.value("id", "").data());
        p.name         = juce::String(jp.value("name", "").data());
        p.manufacturer = juce::String(jp.value("manufacturer", "").data());
        p.format       = juce::String(jp.value("format", "VST3").data());
        p.slot         = jp.value("slot", 0);
        p.enabled      = jp.value("enabled", true);
        if (p.name.isNotEmpty())
            plugins.push_back(p);
    }

    pluginChain->setPlugins(plugins);
    log("[CHAIN] Updated: " + juce::String(plugins.size()) + " plugins");

    // Broadcast confirmation
    dispatchChainGet({});
}

void OscBridge::dispatchAiAction(const nlohmann::json& payload, const juce::String& reqId)
{
    if (!payload.contains("widgetId") || !payload.contains("value"))
        return;

    juce::String widgetId = juce::String(payload["widgetId"].get<std::string>().data());
    float value = payload["value"].get<float>();
    juce::String description;
    if (payload.contains("description"))
        description = juce::String(payload["description"].get<std::string>().data());

    log("[AI ACTION] " + widgetId + " = " + juce::String(value) + (description.isNotEmpty() ? " (" + description + ")" : ""));

    // Execute via ParameterMapper
    if (paramMapper)
        paramMapper->setValue(widgetId, value);

    // Log to session
    if (sessionManager)
        sessionManager->logAiAction(widgetId, value, 0.0f, description);

    // Broadcast action to UI
    nlohmann::json response;
    response["type"] = "ai.action.log";
    response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : nullptr;
    response["timestamp"] = juce::Time::currentTimeMillis();
    response["payload"]["widgetId"] = widgetId.toStdString();
    response["payload"]["value"] = value;
    response["payload"]["description"] = description.toStdString();
    wsServer->broadcast(response);
}

void OscBridge::setSessionManager(SessionManager* sm)
{
    sessionManager = sm;
    log("[OscBridge] Session manager " + juce::String(sm ? "connected" : "disconnected"));
}

//==============================================================================
// Status
//==============================================================================
int OscBridge::getOscMessagesReceived() const
{
    return oscHandler ? oscHandler->getMessagesReceived() : 0;
}

int OscBridge::getOscMessagesSent() const
{
    // OscHandler doesn't track sent messages directly, would need to add
    return 0;
}

int OscBridge::getWebSocketClientsConnected() const
{
    return wsServer ? wsServer->getConnectedClientsCount() : 0;
}

//==============================================================================
// Utilities
//==============================================================================
juce::String OscBridge::generateUUID()
{
    // Simple UUID v4 generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    juce::String uuid;
    const char* hex = "0123456789abcdef";

    for (int i = 0; i < 36; i++)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            uuid += "-";
        else if (i == 14)
            uuid += "4"; // Version 4
        else if (i == 19)
            uuid += juce::String(hex[dis(gen) & 3 | 8]); // Variant
        else
            uuid += juce::String(hex[dis(gen)]);
    }

    return uuid;
}

void OscBridge::log(const juce::String& msg)
{
    DBG("[OscBridge] " + msg);
#ifndef NDEBUG
    juce::File logFile("/tmp/whycremisi-debug.log");
    juce::String timestamp = juce::Time::getCurrentTime().toString(true, true, true, true);
    logFile.appendText("[" + timestamp + "] " + msg + "\n");
#endif
}
