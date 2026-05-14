/*
  ==============================================================================
  OscBridge.cpp
  WhyCremisi VST Plugin - Bidirectional OSC-WebSocket Bridge Implementation

  Bridges OSC (UDP) from DAW to WebSocket (TCP) for React UI.
  ==============================================================================
*/

#include "OscBridge.h"
#include "AiEngine.h"
#include "ToolRegistry.h"
#include "SessionManager.h"
#include "MidiHandler.h"
#include "ParameterMapper.h"
#include "PluginChain.h"
#include "PluginProcessor.h"
#include "IDawHandler.h"
#include "ReaperDawHandler.h"
#include "AbletonDawHandler.h"
#include "DawDetector.h"
#include <chrono>
#include <random>

// ── Message validation helper ─────────────────────────────────
struct MessageSchema {
    std::vector<const char*> requiredFields;
    std::vector<const char*> optionalFields;
};

static bool validateMessage(const nlohmann::json& msg, const MessageSchema& schema, juce::String& error)
{
    if (!msg.is_object()) {
        error = "Message must be a JSON object";
        return false;
    }
    if (!msg.contains("type") || !msg["type"].is_string()) {
        error = "Message missing required field: type";
        return false;
    }
    for (auto& field : schema.requiredFields) {
        if (!msg.contains(field)) {
            error = "Message missing required field: " + juce::String(field);
            return false;
        }
    }
    return true;
}

static MessageSchema getSchemaForType(const juce::String& type)
{
    if (type == "config.set")       return {{"payload"}, {}};
    if (type == "config.get")       return {{"payload"}, {}};
    if (type == "ai.prompt")        return {{"payload"}, {}};
    if (type == "ai.personalityStyle") return {{"payload"}, {}};
    if (type == "daw.command")      return {{"payload"}, {}};
    if (type == "daw.request")      return {{"payload"}, {}};
    if (type == "midi.learn.start") return {{"payload"}, {}};
    if (type == "widget.update")    return {{"payload"}, {}};
    if (type == "chain.get")        return {{}, {}};
    if (type == "chain.set")        return {{"payload"}, {}};
    if (type == "osc.send")         return {{"payload"}, {}};
    if (type == "ping")             return {{}, {}};
    return {{}, {}}; // unknown types pass through
}

//==============================================================================
OscBridge::OscBridge(int oscReceivePort, int wsListenPort)
    : oscPort(oscReceivePort), wsPort(wsListenPort)
{
    oscHandler = std::make_unique<OscHandler>(oscPort);
    wsServer = std::make_unique<WebSocketServer>(wsPort);

    // OSC → WebSocket: set callback for incoming OSC from DAW (float values)
    oscHandler->setCallback([this](const juce::String& address, float value) {
        onOscReceived(address, value);
    });

    // OSC → WebSocket: set callback for incoming OSC string values (track names, etc.)
    oscHandler->setStringCallback([this](const juce::String& address, const juce::String& value) {
        onOscStringReceived(address, value);
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
    bool playing = currentIsPlaying.load();
    if (playing)
    {
        if (lastTimerTimeMs != 0)
        {
            float pos = currentPosition.load();
            pos += (now - lastTimerTimeMs) / 1000.0f;
            currentPosition.store(pos);
        }
        // Broadcast transport at ~10Hz (every 3 ticks at 33ms = ~100ms)
        if (++meterTickCounter % 3 == 0)
            broadcastTransport(playing, currentIsRecording.load(), currentBpm.load(), currentPosition.load());
    }
    lastTimerTimeMs = now;

    // Always broadcast meter at ~30fps
    broadcastMeter(-1, lastMeterL.load(), lastMeterR.load(),
                       lastMeterL.load(), lastMeterR.load());

    // Broadcast analyzer data every 10 ticks (~330ms)
    if (meterTickCounter % 10 == 0)
    {
        float corr = lastCorrelation.load();
        float momentary = lastMomentaryLoudness.load();
        float shortTerm = lastShortTermLoudness.load();
        float integrated = lastIntegratedLoudness.load();
        float truePeak = lastTruePeak.load();
        int clipCount = lastClippingCount.load();

        nlohmann::json ana;
        ana["type"] = "daw.analyzer";
        ana["timestamp"] = juce::Time::currentTimeMillis();
        ana["payload"]["correlation"] = corr;
        ana["payload"]["loudnessMomentary"] = momentary;
        ana["payload"]["loudnessShortTerm"] = shortTerm;
        ana["payload"]["loudnessIntegrated"] = integrated;
        ana["payload"]["truePeak"] = truePeak;
        ana["payload"]["clippingCount"] = clipCount;

        // Include device stats
        double bufSize = static_cast<double>(lastBufferSize.load());
        double sampRate = lastSampleRate.load();
        double latencyMs = (bufSize > 0.0 && sampRate > 0.0)
                           ? (bufSize / sampRate) * 1000.0
                           : 0.0;
        ana["payload"]["sampleRate"] = sampRate;
        ana["payload"]["bufferSize"] = lastBufferSize.load();
        ana["payload"]["latencyMs"] = latencyMs;

        {
            const juce::ScopedLock sl(spectrumLock);
            nlohmann::json specArr = nlohmann::json::array();
            int numBins = juce::jmin(256, (int)lastSpectrum.size());
            for (int i = 0; i < numBins; ++i)
                specArr.push_back(lastSpectrum[i]);
            ana["payload"]["spectrum"] = specArr;
        }

        wsServer->broadcast(ana);

        // Broadcast CPU usage at ~3Hz
        nlohmann::json cpu;
        cpu["type"] = "plugin.cpu";
        cpu["id"]   = nullptr;
        cpu["timestamp"] = juce::Time::currentTimeMillis();
        cpu["payload"]["cpuPercent"] = lastCpuPct.load();
        cpu["payload"]["peakTimeUs"] = lastPeakTimeUs.load();
        wsServer->broadcast(cpu);
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
    if (!juce::MessageManager::existsAndIsCurrentThread())
    {
        juce::MessageManager::callAsync([this, address, value] { onOscReceived(address, value); });
        return;
    }

    log("[OSC->WS] " + address + " = " + juce::String(value, 3));

    // Auto-detect DAW type from first incoming OSC address
    if (!dawHandler)
    {
        auto type = detectDawType(address);
        if (type != DawType::Unknown)
            ensureDawHandler(type);
    }

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
        broadcastSessionEvent("transport", {{"is_playing", currentIsPlaying.load()}, {"bpm", currentBpm.load()}});
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
    // ── Ableton Live: Track parameters & discovery ──────────────────────────
    else if (address == "/live/song/get/num_tracks")
    {
        abletonDetected = true;
        abletonTrackCount = (int)value;
        log("[Ableton] Detected! " + juce::String(abletonTrackCount) + " tracks");
        discoverAbletonTracks();
    }
    else if (address.startsWith("/live/track/") && address.endsWith("/get/volume"))
    {
        handleAbletonTrackData(address, value);
    }
    else if (address.startsWith("/live/track/") && address.endsWith("/get/panning"))
    {
        handleAbletonTrackData(address, value);
    }
    else if (address.startsWith("/live/track/") && address.endsWith("/get/mute"))
    {
        handleAbletonTrackData(address, value);
    }
    else if (address.startsWith("/live/track/") && address.endsWith("/get/solo"))
    {
        handleAbletonTrackData(address, value);
    }
    else if (address.startsWith("/live/track/") && address.endsWith("/get/sends"))
    {
        // Sends come back as individual float values per send slot
        handleAbletonTrackData(address, value);
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
        // REAPER-specific track parameter handling + Ableton Live feedback
        auto extractTrackId = [&](const juce::String& suffix) -> int {
            juce::String idStr = address.fromFirstOccurrenceOf("/track/", false, true);
            idStr = idStr.upToLastOccurrenceOf(suffix, false, true);
            return idStr.getIntValue();
        };

        auto extractAbletonTrackId = [&](const juce::String& suffix) -> int {
            juce::String idStr = address.fromFirstOccurrenceOf("/live/track/", false, true);
            idStr = idStr.upToLastOccurrenceOf(suffix, false, true);
            return idStr.getIntValue();
        };

        int trackId = 0;
        bool handled = true;

        // Ableton set operation feedback (e.g. /live/track/N/set/volume)
        if (address.startsWith("/live/track/") && address.contains("/set/"))
        {
            if (address.endsWith("/volume"))
                trackId = extractAbletonTrackId("/set/volume");
            else if (address.endsWith("/panning"))
                trackId = extractAbletonTrackId("/set/panning");
            else if (address.endsWith("/mute"))
                trackId = extractAbletonTrackId("/set/mute");
            else if (address.endsWith("/solo"))
                trackId = extractAbletonTrackId("/set/solo");
            else
                handled = false;

            if (handled)
                handleAbletonTrackData(address, value);
        }
        // REAPER-specific
        else if (address.startsWith("/track/") && address.endsWith("/volume"))
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
// OSC string callback — handles track names, device names, etc.
//==============================================================================
void OscBridge::onOscStringReceived(const juce::String& address, const juce::String& value)
{
    if (!juce::MessageManager::existsAndIsCurrentThread())
    {
        juce::MessageManager::callAsync([this, address, value] { onOscStringReceived(address, value); });
        return;
    }

    log("[OSC->WS][STR] " + address + " = \"" + value + "\"");

    if (sessionManager)
        sessionManager->logOscEvent(address, 0.0f);

    // Ableton Live: track name
    if (address.startsWith("/live/track/") && address.endsWith("/get/name"))
    {
        handleAbletonTrackString(address, value);
    }
    else
    {
        forwardOscToUI(address, 0.0f);
    }
}

//==============================================================================
// Ableton Live: handle track parameter data
//==============================================================================
void OscBridge::handleAbletonTrackData(const juce::String& address, float value)
{
    // Parse track ID from /live/track/N/... address
    juce::String remainder = address.fromFirstOccurrenceOf("/live/track/", false, true);
    int trackId = remainder.upToFirstOccurrenceOf("/", false, true).getIntValue();
    if (trackId < 1) return;

    abletonDetected = true;
    auto& track = abletonTracks[trackId];

    if (address.endsWith("/volume") || address.endsWith("/get/volume"))
        track.volume = value;
    else if (address.endsWith("/panning") || address.endsWith("/get/panning"))
        track.pan = value;
    else if (address.endsWith("/mute") || address.endsWith("/get/mute"))
        track.mute = (value > 0.5f);
    else if (address.endsWith("/solo") || address.endsWith("/get/solo"))
        track.solo = (value > 0.5f);
    else if (address.contains("/send"))
    {
        // Extract send index: /live/track/N/sends/M/value
        auto sendStr = remainder.fromFirstOccurrenceOf("/sends/", false, true);
        int sendIdx = sendStr.upToFirstOccurrenceOf("/", false, true).getIntValue();
        if (sendIdx >= (int)track.sends.size())
            track.sends.resize(sendIdx + 1, 0.0f);
        track.sends[sendIdx] = value;
    }

    broadcastAbletonTrack(trackId);
}

void OscBridge::handleAbletonTrackString(const juce::String& address, const juce::String& value)
{
    // Parse track ID from /live/track/N/get/name
    juce::String remainder = address.fromFirstOccurrenceOf("/live/track/", false, true);
    int trackId = remainder.upToFirstOccurrenceOf("/", false, true).getIntValue();
    if (trackId < 1) return;

    abletonDetected = true;
    auto& track = abletonTracks[trackId];
    track.name = value;

    broadcastAbletonTrack(trackId);
}

//==============================================================================
// Ableton Live: broadcast track info to UI
//==============================================================================
void OscBridge::broadcastAbletonTrack(int trackId)
{
    auto it = abletonTracks.find(trackId);
    if (it == abletonTracks.end()) return;
    const auto& track = it->second;

    nlohmann::json msg;
    msg["type"] = "daw.track";
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["trackId"] = trackId;
    msg["payload"]["name"] = track.name.toStdString();
    msg["payload"]["volume"] = track.volume;
    msg["payload"]["pan"] = track.pan;
    msg["payload"]["isMuted"] = track.mute;
    msg["payload"]["isSoloed"] = track.solo;
    msg["payload"]["source"] = "ableton";

    nlohmann::json sendArray = nlohmann::json::array();
    for (auto s : track.sends)
        sendArray.push_back(s);
    msg["payload"]["sends"] = sendArray;
    msg["payload"]["numDevices"] = track.numDevices;

    if (wsServer)
        wsServer->broadcast(msg);
}

//==============================================================================
// Ableton Live: auto-discover all tracks on connection
//==============================================================================
void OscBridge::discoverAbletonTracks()
{
    if (!abletonDetected || abletonTrackCount == 0)
    {
        // Send a query to learn the track count
        sendOscToDaw("/live/song/get/num_tracks", 0.0f);
        return;
    }

    log("[Ableton] Discovering " + juce::String(abletonTrackCount) + " tracks...");

    // Enumerate all tracks
    for (int i = 1; i <= abletonTrackCount; ++i)
    {
        juce::String prefix = "/live/track/" + juce::String(i);
        sendOscToDaw(prefix + "/get/name", 0.0f);
        sendOscToDaw(prefix + "/get/volume", 0.0f);
        sendOscToDaw(prefix + "/get/panning", 0.0f);
        sendOscToDaw(prefix + "/get/mute", 0.0f);
        sendOscToDaw(prefix + "/get/solo", 0.0f);
    }

    // Broadcast list of discovered track IDs to UI
    nlohmann::json msg;
    msg["type"] = "daw.trackList";
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["count"] = abletonTrackCount;
    msg["payload"]["source"] = "ableton";
    nlohmann::json ids = nlohmann::json::array();
    for (int i = 1; i <= abletonTrackCount; ++i)
        ids.push_back(i);
    msg["payload"]["trackIds"] = ids;
    if (wsServer)
        wsServer->broadcast(msg);
}

//==============================================================================
// DAW detection & handler management
//==============================================================================
DawType OscBridge::detectDawType(const juce::String& address) const
{
    return DawDetector::detectFromOscAddress(address);
}

void OscBridge::ensureDawHandler(DawType type)
{
    if (type == static_cast<DawType>(0) || type == DawType::Unknown)
        return;
    if (dawHandler && detectedDawType == type)
        return;

    detectedDawType = type;
    dawHandler = nullptr; // reset

    switch (type)
    {
        case DawType::Reaper:
            dawHandler = std::make_unique<ReaperDawHandler>();
            break;
        case DawType::Ableton:
            dawHandler = std::make_unique<AbletonDawHandler>();
            break;
        default:
            break;
    }

    if (dawHandler)
    {
        // Wire up the send callback so handler can emit OSC
        dawHandler->setSendCallback([this](const juce::String& addr, float val) {
            sendOscToDaw(addr, val);
        });

        log("[DAW] Handler created: " + dawHandler->getName());
        broadcastDawInfo();
    }
}

IDawHandler* OscBridge::getOrCreateDawHandler(DawType type)
{
    if (!dawHandler)
        ensureDawHandler(type);
    return dawHandler.get();
}

void OscBridge::broadcastDawInfo()
{
    if (!wsServer || !wsServer->isRunning())
        return;

    nlohmann::json msg;
    msg["type"] = "daw.info";
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["detected"] = dawHandler ? dawHandler->getName().toStdString()
                                            : DawDetector::getName(detectedDawType).toStdString();
    msg["payload"]["oscHelp"] = DawDetector::getOscHelp(detectedDawType).toStdString();
    msg["payload"]["abletonDetected"] = abletonDetected;

    wsServer->broadcast(msg);
}

//==============================================================================
// WebSocket message handler - receives from UI
//==============================================================================
void OscBridge::handleWebSocketMessage(const nlohmann::json& message)
{
    if (!juce::MessageManager::existsAndIsCurrentThread())
    {
        juce::MessageManager::callAsync([this, message] { handleWebSocketMessage(message); });
        return;
    }

    juce::String validationError;
    juce::String msgType = message.contains("type") ? juce::String(message["type"].get<std::string>()) : "";
    auto schema = getSchemaForType(msgType);
    if (!validateMessage(message, schema, validationError)) {
        log("[WS] Validation failed: " + validationError + " (type: " + msgType + ")");
        nlohmann::json err;
        err["type"] = "error";
        err["payload"]["code"] = "VALIDATION_ERROR";
        err["payload"]["message"] = validationError.toStdString();
        broadcastJson(err);
        return;
    }

    juce::String reqId = message.contains("id") && !message["id"].is_null()
                         ? juce::String(message["id"].get<std::string>().data())
                         : "";

    log("[WS->OSC] " + msgType + (reqId.isNotEmpty() ? " (id=" + reqId + ")" : ""));

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
    else if (msgType == "ai.abort")
    {
        log("[AI] Abort requested by user");
        if (aiEngine) aiEngine->abortRequest();
        aiProcessing.store(false);
    }
    else if (msgType == "ai.personalityStyle")
    {
        if (aiEngine && message.contains("payload") && message["payload"].contains("style") && message["payload"]["style"].is_string())
        {
            std::string style = message["payload"]["style"].get<std::string>();
            if (style == "analytical")
                aiEngine->setPersonalityStyle(AiPersonalityStyle::Analytical);
            else if (style == "consultative")
                aiEngine->setPersonalityStyle(AiPersonalityStyle::Consultative);
            else if (style == "direct")
                aiEngine->setPersonalityStyle(AiPersonalityStyle::Direct);
            else if (style == "creative")
                aiEngine->setPersonalityStyle(AiPersonalityStyle::Creative);
            else if (style == "warm")
                aiEngine->setPersonalityStyle(AiPersonalityStyle::Warm);

            log("[AI] Personality style set to: " + juce::String(style.data()));
        }
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
    if (!juce::MessageManager::existsAndIsCurrentThread())
    {
        juce::MessageManager::callAsync([this, clientId, connected] { handleClientConnection(clientId, connected); });
        return;
    }

    if (connected)
    {
        log("[WS] Client " + juce::String(clientId) + " connected");

        if (sessionManager) sessionManager->logWsConnect(clientId, true);

        // Push current state to the newly connected client
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);

        // Push real audio device stats (SR, buffer, latency) to new client
        {
            double bufSize = static_cast<double>(lastBufferSize.load());
            double sampRate = lastSampleRate.load();
            double latencyMs = (bufSize > 0.0 && sampRate > 0.0)
                               ? (bufSize / sampRate) * 1000.0
                               : 0.0;
            nlohmann::json msg;
            msg["type"] = "plugin.stats";
            msg["timestamp"] = juce::Time::currentTimeMillis();
            msg["payload"]["sampleRate"] = sampRate;
            msg["payload"]["bufferSize"] = lastBufferSize.load();
            msg["payload"]["latencyMs"] = latencyMs;
            wsServer->broadcast(msg);
        }

        // Request fresh transport + track state from Ableton
        sendOscToDaw("/live/song/get/is_playing", 0.0f);
        sendOscToDaw("/live/song/get/tempo", 0.0f);
        sendOscToDaw("/live/song/get/current_song_time", 0.0f);
        sendOscToDaw("/live/song/get/num_tracks", 0.0f);

        // Broadcast current program state to newly connected client
        if (pluginProcessor)
            pluginProcessor->broadcastCurrentProgram();

        // Broadcast DAW detection info
        broadcastDawInfo();
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

    // Use DAW handler if detected, otherwise fall back to dual-send
    auto sendViaHandler = [&](auto&& handlerFn, auto&& fallbackFn) {
        if (dawHandler)
            handlerFn();
        else
            fallbackFn();
    };

    if (command == "play")
    {
        currentIsPlaying = true;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendViaHandler(
            [&]() { dawHandler->play(); },
            [&]() {
                sendOscToDaw("/live/song/start_playing", 1.0f);
                sendOscToDaw("/play", 1.0f);
            }
        );
    }
    else if (command == "stop")
    {
        currentIsPlaying = false;
        currentPosition  = 0.0f;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendViaHandler(
            [&]() { dawHandler->stop(); },
            [&]() {
                sendOscToDaw("/live/song/stop_playing", 1.0f);
                sendOscToDaw("/stop", 1.0f);
            }
        );
    }
    else if (command == "record")
    {
        currentIsRecording = !currentIsRecording;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendViaHandler(
            [&]() { dawHandler->record(); },
            [&]() {
                sendOscToDaw("/live/song/record", 1.0f);
                sendOscToDaw("/record", 1.0f);
            }
        );
    }
    else if (command == "pause")
    {
        currentIsPlaying = false;
        broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
        sendViaHandler(
            [&]() { dawHandler->pause(); },
            [&]() {
                sendOscToDaw("/live/song/continue_playing", 1.0f);
                sendOscToDaw("/pause", 1.0f);
            }
        );
    }
    else if (command == "setTempo")
    {
        if (payload.contains("bpm"))
        {
            float bpm = payload["bpm"].get<float>();
            currentBpm = bpm;
            broadcastTransport(currentIsPlaying, currentIsRecording, currentBpm, currentPosition);
            sendViaHandler(
                [&]() { dawHandler->setTempo(bpm); },
                [&]() {
                    sendOscToDaw("/live/song/set/tempo", bpm);
                    sendOscToDaw("/tempo", bpm);
                }
            );
        }
    }
    else if (command == "setVolume")
    {
        if (payload.contains("trackId") && payload.contains("valueDb"))
        {
            int trackId = payload["trackId"].get<int>();
            float db = payload["valueDb"].get<float>();
            float linear = juce::Decibels::decibelsToGain(db);
            sendViaHandler(
                [&]() { dawHandler->setVolume(trackId, linear); },
                [&]() {
                    sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/volume", linear);
                    sendOscToDaw("/track/" + juce::String(trackId) + "/volume", db);
                }
            );
        }
    }
    else if (command == "setPan")
    {
        if (payload.contains("trackId") && payload.contains("value"))
        {
            int trackId = payload["trackId"].get<int>();
            float pan = payload["value"].get<float>();
            sendViaHandler(
                [&]() { dawHandler->setPan(trackId, pan); },
                [&]() {
                    sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/panning", pan);
                    sendOscToDaw("/track/" + juce::String(trackId) + "/pan", pan);
                }
            );
        }
    }
    else if (command == "muteTrack")
    {
        if (payload.contains("trackId") && payload.contains("muted"))
        {
            int trackId = payload["trackId"].get<int>();
            bool muted = payload["muted"].get<bool>();
            sendViaHandler(
                [&]() { dawHandler->muteTrack(trackId, muted); },
                [&]() {
                    sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/mute", muted ? 1.0f : 0.0f);
                    sendOscToDaw("/track/" + juce::String(trackId) + "/mute", muted ? 1.0f : 0.0f);
                }
            );
        }
    }
    else if (command == "soloTrack")
    {
        if (payload.contains("trackId") && payload.contains("soloed"))
        {
            int trackId = payload["trackId"].get<int>();
            bool soloed = payload["soloed"].get<bool>();
            sendViaHandler(
                [&]() { dawHandler->soloTrack(trackId, soloed); },
                [&]() {
                    sendOscToDaw("/live/track/" + juce::String(trackId) + "/set/solo", soloed ? 1.0f : 0.0f);
                    sendOscToDaw("/track/" + juce::String(trackId) + "/solo", soloed ? 1.0f : 0.0f);
                }
            );
        }
    }
    else if (command == "requestTransport")
    {
        sendViaHandler(
            [&]() {
                // Ableton: poll for current state
                if (dawHandler->isAbleton())
                {
                    sendOscToDaw("/live/song/get/is_playing", 0.0f);
                    sendOscToDaw("/live/song/get/tempo", 0.0f);
                    sendOscToDaw("/live/song/get/current_song_time", 0.0f);
                }
            },
            [&]() {
                sendOscToDaw("/live/song/get/is_playing", 0.0f);
                sendOscToDaw("/live/song/get/tempo", 0.0f);
                sendOscToDaw("/live/song/get/current_song_time", 0.0f);
            }
        );
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
    else if (command == "midSide")
    {
        if (payload.contains("enabled"))
            sendOscToDaw("/whycremisi/midside", payload["enabled"].get<bool>() ? 1.0f : 0.0f);
    }
    else if (command == "phaseInvert")
    {
        if (payload.contains("enabled"))
            sendOscToDaw("/whycremisi/phaseinvert", payload["enabled"].get<bool>() ? 1.0f : 0.0f);
    }
    else if (command == "mono")
    {
        sendOscToDaw("/whycremisi/mono", 1.0f);
    }
    else if (command == "narrow")
    {
        sendOscToDaw("/whycremisi/width", 0.0f);
    }
    else if (command == "widen")
    {
        sendOscToDaw("/whycremisi/width", 1.0f);
    }
    else if (command == "targetLoudness")
    {
        if (payload.contains("target"))
            sendOscToDaw("/whycremisi/loudness_target", payload["target"].get<float>());
    }
    else if (command == "limiter")
    {
        sendOscToDaw("/whycremisi/limiter", 1.0f);
    }
    else if (command == "softClip")
    {
        if (payload.contains("enabled"))
            sendOscToDaw("/whycremisi/clip_mode", payload["enabled"].get<bool>() ? 0.0f : 1.0f);
    }
    else if (command == "hardClip")
    {
        if (payload.contains("enabled"))
            sendOscToDaw("/whycremisi/clip_mode", payload["enabled"].get<bool>() ? 1.0f : 0.0f);
    }
    else if (command == "ceiling")
    {
        if (payload.contains("value"))
            sendOscToDaw("/whycremisi/ceiling", payload["value"].get<float>());
    }
    else if (command == "compThreshold")
    {
        if (payload.contains("value"))
            sendOscToDaw("/whycremisi/comp_threshold", payload["value"].get<float>());
    }
    else if (command == "compRatio")
    {
        if (payload.contains("value"))
            sendOscToDaw("/whycremisi/comp_ratio", payload["value"].get<float>());
    }
    else if (command == "compBypass")
    {
        if (payload.contains("enabled"))
            sendOscToDaw("/whycremisi/comp_bypass", payload["enabled"].get<bool>() ? 1.0f : 0.0f);
    }
    else if (command == "compAuto")
    {
        sendOscToDaw("/whycremisi/comp_auto", 1.0f);
    }
    else if (command == "applyEQ")
    {
        if (payload.contains("freq") && payload.contains("gain"))
            sendOscToDaw("/whycremisi/eq_apply", 1.0f);
    }
    else if (command == "eqAnalyze")
    {
        sendOscToDaw("/whycremisi/eq_analyze", 1.0f);
    }
    else if (command == "eqMatch")
    {
        sendOscToDaw("/whycremisi/eq_match", 1.0f);
    }
    else if (command == "spectralAnalyze")
    {
        sendOscToDaw("/whycremisi/spectral_analyze", 1.0f);
    }
    else if (command == "gotoMarker")
    {
        int idx = payload.contains("index") ? payload["index"].get<int>() : 0;
        sendViaHandler(
            [&]() { dawHandler->gotoMarker(idx); },
            [&]() {
                sendOscToDaw("/live/song/goto/marker", (float)idx);
                sendOscToDaw("/marker/goto", (float)idx);
            }
        );
    }
    else if (command == "setMarker")
    {
        sendViaHandler(
            [&]() { dawHandler->setMarker(); },
            [&]() {
                sendOscToDaw("/live/song/set/marker", 1.0f);
                sendOscToDaw("/marker/insert", 1.0f);
            }
        );
    }
    else if (command == "prevMarker")
    {
        sendViaHandler(
            [&]() { dawHandler->prevMarker(); },
            [&]() {
                sendOscToDaw("/live/song/prev/marker", 1.0f);
                sendOscToDaw("/marker/prev", 1.0f);
            }
        );
    }
    else if (command == "nextMarker")
    {
        sendViaHandler(
            [&]() { dawHandler->nextMarker(); },
            [&]() {
                sendOscToDaw("/live/song/next/marker", 1.0f);
                sendOscToDaw("/marker/next", 1.0f);
            }
        );
    }
    else if (command == "selectTrack")
    {
        if (payload.contains("index"))
        {
            int idx = payload["index"].get<int>();
            sendViaHandler(
                [&]() { dawHandler->selectTrack(idx); },
                [&]() {
                    sendOscToDaw("/live/track/" + juce::String(idx) + "/select", 1.0f);
                }
            );
        }
    }
    else if (command == "fxParam")
    {
        if (payload.contains("trackId") && payload.contains("fxId") && payload.contains("paramId") && payload.contains("value"))
        {
            int t = payload["trackId"].get<int>();
            int f = payload["fxId"].get<int>();
            int p = payload["paramId"].get<int>();
            float v = payload["value"].get<float>();
            sendViaHandler(
                [&]() { dawHandler->setFxParam(t, f, p, v); },
                [&]() {
                    sendOscToDaw("/live/track/" + juce::String(t) + "/fx/" + juce::String(f) + "/param/" + juce::String(p), v);
                }
            );
        }
    }
    else if (command == "programSelect")
    {
        if (payload.contains("index") && pluginProcessor)
        {
            int idx = payload["index"].get<int>();
            pluginProcessor->setCurrentProgram(idx);
        }
    }
    else if (command == "programList")
    {
        if (pluginProcessor)
        {
            nlohmann::json msg;
            msg["type"] = "plugin.program";
            msg["timestamp"] = juce::Time::currentTimeMillis();
            msg["payload"]["currentIndex"] = pluginProcessor->getCurrentProgram();
            msg["payload"]["currentName"] = pluginProcessor->getProgramName(
                pluginProcessor->getCurrentProgram()).toStdString();
            nlohmann::json names = nlohmann::json::array();
            auto progs = pluginProcessor->getProgramNames();
            for (int i = 0; i < progs.size(); ++i)
                names.push_back(progs[i].toStdString());
            msg["payload"]["programs"] = names;
            wsServer->broadcast(msg);
        }
    }
    else if (command == "programRename")
    {
        if (payload.contains("index") && payload.contains("name") && pluginProcessor)
        {
            int idx = payload["index"].get<int>();
            std::string name = payload["name"];
            pluginProcessor->changeProgramName(idx, juce::String(name.data()));
        }
    }
    else if (command == "presetSave")
    {
        if (payload.contains("path") && pluginProcessor)
        {
            std::string path = payload["path"];
            pluginProcessor->savePreset(juce::File(juce::String(path.data())));
        }
        else if (pluginProcessor)
        {
            // Save to default location
            auto defaultDir = juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory).getChildFile("WhyCremisi/Presets");
            defaultDir.createDirectory();
            auto file = defaultDir.getChildFile(
                pluginProcessor->getProgramName(pluginProcessor->getCurrentProgram()) + ".whycremisi");
            pluginProcessor->savePreset(file);
            log("[CMD] Preset saved: " + file.getFullPathName());
        }
    }
    else if (command == "presetLoad")
    {
        if (payload.contains("path") && pluginProcessor)
        {
            std::string path = payload["path"];
            pluginProcessor->loadPreset(juce::File(juce::String(path.data())));
            log("[CMD] Preset loaded: " + juce::String(path.data()));
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
        response["payload"]["isPlaying"] = currentIsPlaying.load();
        response["payload"]["isRecording"] = currentIsRecording.load();
        response["payload"]["bpm"] = currentBpm.load();
        response["payload"]["positionSeconds"] = currentPosition.load();
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

    // Check if streaming is requested (default: true)
    bool useStreaming = !payload.contains("stream") || payload["stream"].get<bool>();

    aiThread = std::make_unique<std::thread>([this, capturedPrompt, capturedReqId, startMs, useStreaming]()
    {
        if (useStreaming)
        {
            // Streaming path — send chunks as they arrive
            juce::String accumulated;

            aiEngine->sendPromptStreaming(capturedPrompt,
                [this, capturedReqId, capturedPrompt, &accumulated, startMs](const juce::String& chunk, bool isDone)
                {
                    if (!chunk.isEmpty())
                    {
                        accumulated += chunk;
                        broadcastAiStream(capturedReqId, chunk, false);
                    }

                    if (isDone)
                    {
                        aiProcessing.store(false);
                        int durationMs = static_cast<int>(juce::Time::currentTimeMillis() - startMs);

                        // Parse accumulated streaming text — no second API call
                        auto structured = aiEngine->parseStructuredResponse(accumulated);
                        bool success = structured.success || !accumulated.isEmpty();
                        aiEngine->finalizeStreamingResponse(capturedPrompt, structured.text);

                        if (sessionManager)
                            sessionManager->logAiResponse(structured.text, durationMs);

                        broadcastSessionEvent("ai_response",
                            {{"preview", structured.text.substring(0, 80).toStdString()},
                             {"duration_ms", durationMs},
                             {"success", success}});

                        nlohmann::json response;
                        response["type"] = "ai.response";
                        response["id"] = capturedReqId.toStdString();
                        response["timestamp"] = juce::Time::currentTimeMillis();
                        response["payload"]["status"] = success ? "success" : "error";
                        response["payload"]["provider"] = aiEngine->getProviderName().toStdString();
                        response["payload"]["model"]    = aiEngine->getModelName().toStdString();
                        response["payload"]["content"]  = structured.text.toStdString();
                        response["payload"]["durationMs"] = durationMs;

                        nlohmann::json actionsArray = nlohmann::json::array();
                        for (const auto& a : structured.actions)
                        {
                            nlohmann::json act;
                            act["widgetId"] = a.widgetId.toStdString();
                            act["value"] = a.value;
                            act["previousValue"] = a.previousValue;
                            act["description"] = a.description.toStdString();
                            actionsArray.push_back(act);

                            if (sessionManager)
                                sessionManager->logAiAction(a.widgetId, a.value, a.previousValue, a.description);

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
                    }
                });
        }
        else
        {
            // Non-streaming path (original behavior)
            auto structured = aiEngine->sendPromptStructured(capturedPrompt);
            bool success = structured.success;
            int durationMs = static_cast<int>(juce::Time::currentTimeMillis() - startMs);

            aiProcessing.store(false);

            if (sessionManager)
                sessionManager->logAiResponse(structured.text, durationMs);

        broadcastSessionEvent("ai_response",
            {{"preview", structured.text.substring(0, 80).toStdString()},
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
        } // end non-streaming else
    }); // end lambda
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
        response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : std::string();
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
        response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : std::string();
        response["timestamp"] = juce::Time::currentTimeMillis();
        response["payload"]["status"] = "cancelled";
        wsServer->broadcast(response);

        log("[MIDI] Learn stopped");
    }
}

//==============================================================================
void OscBridge::dispatchConfig(const nlohmann::json& payload, const juce::String& reqId)
{
    if (!payload.contains("key") || !payload["key"].is_string())
        return;

    std::string key = payload["key"].get<std::string>();
    juce::String configKey(key.data(), key.size());
    bool isRead = !payload.contains("value");

    // Helper to send a config.response
    auto sendConfigResponse = [&](nlohmann::json responsePayload) {
        nlohmann::json response = nlohmann::json::object();
        response["type"] = "config.response";
        if (reqId.isNotEmpty())
            response["id"] = reqId.toStdString();
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
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string provider = payload["value"].get<std::string>();
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
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string model = payload["value"].get<std::string>();
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
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        if (!payload["provider"].is_string()) return;
        std::string provider = payload["provider"].get<std::string>();
        std::string apiKey = payload["value"].get<std::string>();
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
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string url = payload["value"].get<std::string>();
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
    // Store for timer-based broadcast (safe from audio thread)
    lastSampleRate.store(sampleRate);
    lastBufferSize.store(bufferSize);
}

void OscBridge::setCpuUsage(double cpuPct, double peakTimeUs)
{
    lastCpuPct.store(cpuPct);
    lastPeakTimeUs.store(peakTimeUs);
}

void OscBridge::updateAnalyzer(float correlation, float momentaryLoudness, float shortTermLoudness, float integratedLoudness, float truePeak, const std::vector<float>& spectrum, int clippingCount)
{
    lastCorrelation.store(correlation);
    lastMomentaryLoudness.store(momentaryLoudness);
    lastShortTermLoudness.store(shortTermLoudness);
    lastIntegratedLoudness.store(integratedLoudness);
    lastTruePeak.store(truePeak);
    lastClippingCount.store(clippingCount);
    const juce::ScopedLock sl(spectrumLock);
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
    msg["payload"]["isPlaying"] = currentIsPlaying.load();
    msg["payload"]["isRecording"] = currentIsRecording.load();
    msg["payload"]["bpm"] = currentBpm.load();
    msg["payload"]["positionSeconds"] = currentPosition.load();
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
    msg["id"] = requestId.isNotEmpty() ? requestId.toStdString() : std::string();
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
    msg["id"] = requestId.isNotEmpty() ? requestId.toStdString() : std::string();
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
    msg["id"] = widgetId.isNotEmpty() ? widgetId.toStdString() : std::string();
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
    log("[OSC<-WS] SENT: " + address + " = " + juce::String(value, 3));
}
void OscBridge::sendOscToDaw(const juce::String& address, const juce::String& value)
{
    oscHandler->sendMessage(address, value);
    log("[OSC<-WS] SENT: " + address + " = \"" + value + "\"");
}
void OscBridge::sendOscToDaw(const juce::String& address, int value)
{
    oscHandler->sendMessage(address, value);
    log("[OSC<-WS] SENT: " + address + " = " + juce::String(value));
}

void OscBridge::setDawTarget(const juce::String& host, int sendPort)
{
    oscHandler->setSendTarget(host, sendPort);
    log("[CONFIG] DAW target set to " + host + ":" + juce::String(sendPort));
}

void OscBridge::setAiEngine(AiEngine* engine)
{
    aiEngine = engine;
    if (!aiEngine) return;

    // Wire tool executor: maps tool calls → DAW commands / widget control
    aiEngine->setToolExecutor([this](const ToolCall& call) -> ToolResult {
        ToolResult result;
        result.toolCallId = call.id;
        result.name = call.name;

        auto sendDawCmd = [&](const std::string& action, const nlohmann::json& params = {}) {
            nlohmann::json msg;
            msg["type"] = "daw.command";
            msg["payload"]["command"] = action;
            for (auto& [k, v] : params.items())
                msg["payload"][k] = v;
            wsServer->broadcast(msg);
        };

        // Transport
        if (call.name == "daw.transport.play") {
            sendDawCmd("play");
            result.success = true;
            result.output = "Playback started";
        }
        else if (call.name == "daw.transport.stop") {
            sendDawCmd("stop");
            result.success = true;
            result.output = "Playback stopped";
        }
        else if (call.name == "daw.transport.record") {
            bool arm = call.arguments.contains("arm") && call.arguments["arm"].get<bool>();
            sendDawCmd("record", {{"arm", arm}});
            result.success = true;
            result.output = arm ? "Recording armed" : "Recording stopped";
        }
        else if (call.name == "daw.transport.setTempo") {
            float bpm = call.arguments["bpm"].get<float>();
            sendDawCmd("setTempo", {{"bpm", bpm}});
            result.success = true;
            result.output = "Tempo set to " + std::to_string(bpm) + " BPM";
        }
        // Track controls
        else if (call.name == "daw.track.setVolume") {
            int track = call.arguments["track"].get<int>();
            float vol = call.arguments["volume"].get<float>();
            sendDawCmd("setVolume", {{"track", track}, {"volume", vol}});
            result.success = true;
            result.output = "Track " + std::to_string(track) + " volume set to " + std::to_string(vol) + "dB";
        }
        else if (call.name == "daw.track.setPan") {
            int track = call.arguments["track"].get<int>();
            float pan = call.arguments["pan"].get<float>();
            sendDawCmd("setPan", {{"track", track}, {"pan", pan}});
            result.success = true;
            result.output = "Track " + std::to_string(track) + " pan set to " + std::to_string(pan);
        }
        else if (call.name == "daw.track.mute") {
            int track = call.arguments["track"].get<int>();
            bool mute = call.arguments["mute"].get<bool>();
            sendDawCmd("mute", {{"track", track}, {"mute", mute}});
            result.success = true;
            result.output = "Track " + std::to_string(track) + (mute ? " muted" : " unmuted");
        }
        else if (call.name == "daw.track.solo") {
            int track = call.arguments["track"].get<int>();
            bool solo = call.arguments["solo"].get<bool>();
            sendDawCmd("solo", {{"track", track}, {"solo", solo}});
            result.success = true;
            result.output = "Track " + std::to_string(track) + (solo ? " soloed" : " unsoloed");
        }
        // Plugin controls
        else if (call.name == "daw.plugin.setParam") {
            int track = call.arguments["track"].get<int>();
            std::string plugin = call.arguments["plugin"].get<std::string>();
            std::string param = call.arguments["param"].get<std::string>();
            float value = call.arguments["value"].get<float>();
            sendDawCmd("setPluginParam", {{"track", track}, {"plugin", plugin}, {"param", param}, {"value", value}});
            result.success = true;
            result.output = plugin + " " + param + " set to " + std::to_string(value);
        }
        else if (call.name == "daw.plugin.bypass") {
            int track = call.arguments["track"].get<int>();
            std::string plugin = call.arguments["plugin"].get<std::string>();
            bool bypass = call.arguments["bypass"].get<bool>();
            sendDawCmd("bypassPlugin", {{"track", track}, {"plugin", plugin}, {"bypass", bypass}});
            result.success = true;
            result.output = plugin + (bypass ? " bypassed" : " enabled");
        }
        // Markers
        else if (call.name == "daw.marker.goto") {
            std::string marker = call.arguments["marker"].get<std::string>();
            sendDawCmd("gotoMarker", {{"marker", marker}});
            result.success = true;
            result.output = "Navigated to marker " + marker;
        }
        else if (call.name == "daw.marker.set") {
            std::string name = call.arguments.contains("name") ? call.arguments["name"].get<std::string>() : "";
            sendDawCmd("setMarker", {{"name", name}});
            result.success = true;
            result.output = name.empty() ? "Marker set" : "Marker '" + name + "' set";
        }
        // Widget controls (dynamic)
        else if (call.name.startsWith("widget.set_")) {
            juce::String widgetId = call.name.fromFirstOccurrenceOf("widget.set_", false, false);
            float value = call.arguments["value"].get<float>();
            nlohmann::json msg;
            msg["type"] = "widget.update";
            msg["payload"]["widgetId"] = widgetId.toStdString();
            msg["payload"]["value"] = value;
            wsServer->broadcast(msg);
            result.success = true;
            result.output = "Widget " + widgetId.toStdString() + " set to " + std::to_string(value);
        }
        else {
            result.success = false;
            result.output = "Unknown tool: " + call.name.toStdString();
        }

        return result;
    });
}

void OscBridge::setMidiHandler(MidiHandler* mh) { midiHandler = mh; }

void OscBridge::setParameterMapper(ParameterMapper* pm) { paramMapper = pm; }

void OscBridge::setPluginChain(PluginChain* pc) { pluginChain = pc; }

void OscBridge::broadcastJson(const nlohmann::json& msg) { wsServer->broadcast(msg); }

void OscBridge::dispatchChainGet(const juce::String& reqId)
{
    nlohmann::json response;
    response["type"] = "chain.response";
    response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : std::string();
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
    response["id"] = reqId.isNotEmpty() ? reqId.toStdString() : std::string();
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
