/*
  ==============================================================================
  OscBridge.cpp
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  Bidirectional OSC-WebSocket Bridge Implementation

  Bridges OSC (UDP) from DAW to WebSocket (TCP) for React UI.
  ==============================================================================
*/

#include "OscBridge.h"
#include "AiEngine.h"
#include "AIProvider.h"
#include "ToolRegistry.h"
#include "SessionManager.h"
#include "MidiHandler.h"
#include "ParameterMapper.h"
#include "PluginChain.h"
#include "PluginProcessor.h"
#include "IDawHandler.h"
#include "DSPEngine.h"
#include "../debug/DebugLog.h"
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

    // Messaggi con piu' argomenti: e' la forma con cui AbletonOSC risponde
    // sulle tracce, e senza questa non ne veniva letto nessuno.
    oscHandler->setMultiCallback([this](const juce::String& address, const std::vector<float>& args) {
        handleAbletonMultiArg(address, args);
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
        ana["payload"]["rms"] = lastRms.load();
        ana["payload"]["gainReduction"] = lastGainReduction.load();
        ana["payload"]["bpm"] = lastBpm.load();
        ana["payload"]["bpmConfidence"] = lastBpmConfidence.load();
        {
            const juce::ScopedLock sl(spectrumLock);
            if (lastKey.isNotEmpty())
            {
                ana["payload"]["key"] = lastKey.toStdString();
                ana["payload"]["keyConfidence"] = lastKeyConfidence.load();
            }
        }

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

            // Punti del vectorscope, arrotondati a tre decimali: oltre non
            // si vedrebbe la differenza a schermo e il messaggio pesa meno.
            nlohmann::json scopeArr = nlohmann::json::array();
            for (float v : lastScopePoints)
                scopeArr.push_back (std::round (v * 1000.0f) / 1000.0f);
            ana["payload"]["scope"] = scopeArr;
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
//==============================================================================
// Catena DSP: accesso e stato
//==============================================================================

DSPEngine* OscBridge::getDspEngine() const
{
    return pluginProcessor ? pluginProcessor->getDspEngine() : nullptr;
}

void OscBridge::applyDspBypass (int module, bool bypassed)
{
    auto* dsp = getDspEngine();
    if (! dsp) return;

    dsp->setBypass (module, bypassed);
    log (juce::String ("[DSP] modulo ") + juce::String (module)
         + (bypassed ? " bypassato" : " attivo"));
    broadcastDspState();
}

void OscBridge::broadcastDspState()
{
    auto* dsp = getDspEngine();
    if (! dsp || ! wsServer) return;

    nlohmann::json msg;
    msg["type"] = "dsp.state";
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["eqBypassed"]    = dsp->isBypassed (DSPEngine::EqModule);
    msg["payload"]["compBypassed"]  = dsp->isBypassed (DSPEngine::CompModule);
    msg["payload"]["limitBypassed"] = dsp->isBypassed (DSPEngine::LimitModule);

    if (dsp->compressor)
    {
        msg["payload"]["comp"]["threshold"] = dsp->compressor->getThreshold();
        msg["payload"]["comp"]["ratio"]     = dsp->compressor->getRatio();
        msg["payload"]["comp"]["attack"]    = dsp->compressor->getAttack();
        msg["payload"]["comp"]["release"]   = dsp->compressor->getRelease();
        msg["payload"]["comp"]["makeup"]    = dsp->compressor->getMakeup();
    }
    if (dsp->limiter)
    {
        msg["payload"]["limiter"]["threshold"] = dsp->limiter->getThreshold();
        msg["payload"]["limiter"]["release"]   = dsp->limiter->getRelease();
    }

    wsServer->broadcast (msg);
}

void OscBridge::handleAbletonMultiArg(const juce::String& address, const std::vector<float>& args)
{
    // AbletonOSC risponde con l'indice della traccia come primo argomento:
    //     /live/track/get/volume  [2, 0.85]
    // Il vecchio percorso lo cercava dentro l'indirizzo, non lo trovava e
    // scartava il messaggio, quindi nessuna risposta di Live arrivava mai.
    if (! address.startsWith("/live/track/") || args.size() < 2)
        return;

    const int trackId = (int) args[0];
    const float value = args[1];
    if (trackId < 0) return;

    abletonDetected = true;
    auto& track = abletonTracks[trackId];

    if (address.endsWith("/volume"))       track.volume = value;
    else if (address.endsWith("/panning")) track.pan = value;
    else if (address.endsWith("/mute"))    track.mute = (value > 0.5f);
    else if (address.endsWith("/solo"))    track.solo = (value > 0.5f);
    else return;

    broadcastAbletonTrack(trackId);
}

void OscBridge::handleAbletonTrackData(const juce::String& address, float value)
{
    // Percorso per il vecchio LiveOSC, dove l'indice sta nell'indirizzo:
    //     /live/track/2/get/volume  [0.85]
    // Tenuto per chi usa ancora quel Remote Script; il formato attuale
    // passa invece da handleAbletonMultiArg.
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
        // La richiesta del numero di tracce non prende argomenti.
        oscHandler->sendMessage("/live/song/get/num_tracks", std::vector<float>{}, {});
        return;
    }

    log("[Ableton] Discovering " + juce::String(abletonTrackCount) + " tracks...");

    // In AbletonOSC l'indice della traccia e' un argomento del messaggio,
    // non parte dell'indirizzo, e la numerazione parte da zero. Con la
    // forma precedente (/live/track/1/get/name) Live non rispondeva.
    for (int i = 0; i < abletonTrackCount; ++i)
    {
        const std::vector<float> arg { (float) i };
        const std::vector<bool>  asInt { true };
        oscHandler->sendMessage("/live/track/get/name",    arg, asInt);
        oscHandler->sendMessage("/live/track/get/volume",  arg, asInt);
        oscHandler->sendMessage("/live/track/get/panning", arg, asInt);
        oscHandler->sendMessage("/live/track/get/mute",    arg, asInt);
        oscHandler->sendMessage("/live/track/get/solo",    arg, asInt);
    }

    // Broadcast list of discovered track IDs to UI
    nlohmann::json msg;
    msg["type"] = "daw.trackList";
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["count"] = abletonTrackCount;
    msg["payload"]["source"] = "ableton";
    nlohmann::json ids = nlohmann::json::array();
    // Stessa numerazione da zero usata nelle richieste qui sopra: se la UI
    // ricevesse indici da 1 rimanderebbe comandi sulla traccia sbagliata.
    for (int i = 0; i < abletonTrackCount; ++i)
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

        // Canale multi-argomento: senza questo Ableton non riceve i comandi
        // sul mixer, perche' li' l'indice della traccia e' un argomento.
        dawHandler->setSendMultiCallback([this](const juce::String& addr,
                                                const std::vector<float>& vals,
                                                const std::vector<bool>& intMask) {
            oscHandler->sendMessage(addr, vals, intMask);
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
    // ── Catena DSP interna ──────────────────────────────────────────────
    //
    //  Questi comandi agiscono sui moduli dentro il plugin. Prima venivano
    //  spediti come OSC verso la DAW ("/whycremisi/comp_ratio"), che non li
    //  conosce: uscivano dal plugin e non li riceveva nessuno, mentre il
    //  compressore restava com'era.
    else if (command == "limiter")
    {
        // Senza "enabled" il comando accende il limiter, che e' quello che
        // ci si aspetta premendo un pulsante chiamato LIMIT.
        const bool on = payload.contains("enabled") ? payload["enabled"].get<bool>() : true;
        applyDspBypass (DSPEngine::LimitModule, ! on);
    }
    else if (command == "ceiling")
    {
        if (payload.contains("value"))
            if (auto* dsp = getDspEngine())
                if (dsp->limiter)
                    dsp->limiter->setThreshold (payload["value"].get<float>());
    }
    else if (command == "compThreshold")
    {
        if (payload.contains("value"))
            if (auto* dsp = getDspEngine())
                if (dsp->compressor)
                    dsp->compressor->setThreshold (payload["value"].get<float>());
    }
    else if (command == "compRatio")
    {
        if (payload.contains("value"))
            if (auto* dsp = getDspEngine())
                if (dsp->compressor)
                    dsp->compressor->setRatio (payload["value"].get<float>());
    }
    else if (command == "compAttack")
    {
        if (payload.contains("value"))
            if (auto* dsp = getDspEngine())
                if (dsp->compressor)
                    dsp->compressor->setAttack (payload["value"].get<float>());
    }
    else if (command == "compRelease")
    {
        if (payload.contains("value"))
            if (auto* dsp = getDspEngine())
                if (dsp->compressor)
                    dsp->compressor->setRelease (payload["value"].get<float>());
    }
    else if (command == "compMakeup")
    {
        if (payload.contains("value"))
            if (auto* dsp = getDspEngine())
                if (dsp->compressor)
                    dsp->compressor->setMakeup (payload["value"].get<float>());
    }
    else if (command == "compBypass")
    {
        // "enabled" qui significa "bypass attivo", come lo manda la UI.
        const bool bypassed = payload.contains("enabled") ? payload["enabled"].get<bool>() : true;
        applyDspBypass (DSPEngine::CompModule, bypassed);
    }
    else if (command == "eqBypass")
    {
        const bool bypassed = payload.contains("enabled") ? payload["enabled"].get<bool>() : true;
        applyDspBypass (DSPEngine::EqModule, bypassed);
    }
    else if (command == "dspBypassAll")
    {
        const bool bypassed = payload.contains("enabled") ? payload["enabled"].get<bool>() : true;
        applyDspBypass (DSPEngine::EqModule, bypassed);
        applyDspBypass (DSPEngine::CompModule, bypassed);
        applyDspBypass (DSPEngine::LimitModule, bypassed);
    }
    else if (command == "compAuto")
    {
        // Impostazione di partenza ragionevole per un master: soglia sotto
        // il livello di lavoro tipico e rapporto contenuto, cosi' interviene
        // sui picchi senza schiacciare il pezzo.
        if (auto* dsp = getDspEngine())
        {
            if (dsp->compressor)
            {
                dsp->compressor->setThreshold (-18.0f);
                dsp->compressor->setRatio (2.5f);
                dsp->compressor->setAttack (10.0f);
                dsp->compressor->setRelease (120.0f);
                dsp->compressor->setMakeup (0.0f);
            }
            applyDspBypass (DSPEngine::CompModule, false);
        }
    }
    else if (command == "applyEQ")
    {
        // Prima questo comando spediva "/whycremisi/eq_apply" al DAW con un
        // 1.0 fisso, buttando via frequenza e guadagno: il pulsante del box
        // dei suggerimenti diceva "applicato" e non toccava niente.
        // Ora scrive davvero su una banda dell'EQ interno.
        auto* dsp = getDspEngine();
        if (! dsp || dsp->eqBands.empty())
            return;

        const float gain = payload.contains("gain") && payload["gain"].is_number()
                         ? payload["gain"].get<float>() : 0.0f;

        // La frequenza puo' arrivare come numero oppure come intervallo
        // testuale tipo "200Hz-400Hz": in quel caso si prende il centro
        // geometrico, che e' il modo giusto di stare in mezzo fra due
        // frequenze — la media aritmetica sposta il punto verso l'acuto.
        float freq = 1000.0f;
        if (payload.contains("freq"))
        {
            if (payload["freq"].is_number())
            {
                freq = payload["freq"].get<float>();
            }
            else if (payload["freq"].is_string())
            {
                const juce::String testo (payload["freq"].get<std::string>());
                const float lo = testo.upToFirstOccurrenceOf ("-", false, true)
                                     .retainCharacters ("0123456789.").getFloatValue();
                const float hi = testo.fromLastOccurrenceOf ("-", false, true)
                                     .retainCharacters ("0123456789.").getFloatValue();
                if (lo > 0.0f && hi > lo) freq = std::sqrt (lo * hi);
                else if (lo > 0.0f)       freq = lo;
            }
        }

        const float q = payload.contains("q") && payload["q"].is_number()
                      ? payload["q"].get<float>() : 1.4f;

        // Si sceglie una banda libera, altrimenti la prima: cosi' due
        // suggerimenti su frequenze diverse non si sovrascrivono.
        EQBand* banda = nullptr;
        for (auto& b : dsp->eqBands)
            if (b && std::abs (b->getGain()) < 0.01f) { banda = b.get(); break; }
        if (! banda) banda = dsp->eqBands[0].get();
        if (! banda) return;

        banda->setType (EQBand::Type::Peak);
        banda->setFrequency (freq);
        banda->setQ (q);
        banda->setGain (gain);
        banda->setEnabled (true);
        applyDspBypass (DSPEngine::EqModule, false);

        log ("[DSP] EQ: " + juce::String (freq, 0) + " Hz, "
             + juce::String (gain, 1) + " dB, Q " + juce::String (q, 2));

        nlohmann::json rp;
        rp["type"] = "dsp.eqApplied";
        rp["payload"]["freq"] = freq;
        rp["payload"]["gain"] = gain;
        rp["payload"]["q"] = q;
        broadcastJson (rp);
        broadcastDspState();
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
        // Una richiesta risulta ancora in corso. Prima bastava questo per
        // rifiutare, e se la precedente moriva senza rilasciare il flag il
        // plugin restava muto per il resto della sessione. Ora si guarda da
        // quanto dura: oltre il limite la si considera persa e si riparte.
        const auto da = juce::Time::currentTimeMillis() - aiStartedAtMs.load();

        if (da < aiStalloMs)
        {
            nlohmann::json busy;
            busy["type"] = "ai.response";
            busy["id"] = activeReqId.toStdString();
            busy["timestamp"] = juce::Time::currentTimeMillis();
            busy["payload"]["status"] = "error";
            busy["payload"]["content"] = (juce::String ("Una richiesta e' gia' in corso da ")
                                       + juce::String (da / 1000)
                                       + "s. Attendi la risposta oppure premi Annulla.").toStdString();
            wsServer->broadcast(busy);
            return;
        }

        log("[AI] La richiesta precedente e' ferma da " + juce::String(da / 1000)
            + "s: la abbandono e proseguo");
        aiEngine->abortRequest();
        aiProcessing.store(false);
    }

    aiProcessing.store(true);
    aiStartedAtMs.store(juce::Time::currentTimeMillis());

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
      // Rete di sicurezza: qualunque cosa succeda qui dentro — eccezione,
      // ritorno anticipato, callback che non arriva — il flag deve tornare
      // basso, altrimenti il prossimo prompt trova la porta chiusa.
      struct RilasciaAllaFine
      {
          std::atomic<bool>& flag;
          ~RilasciaAllaFine() { flag.store(false); }
      } rilascia { aiProcessing };

      try
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

                        // Se non e' arrivato niente, il motivo lo sa il
                        // fornitore. Prima finiva qui: la UI riceveva
                        // status "error" con contenuto vuoto e mostrava una
                        // risposta bianca, senza dire che la chiave era
                        // sbagliata o il servizio spento.
                        const juce::String motivo = aiEngine->getProviderLastError();
                        if (!success)
                        {
                            structured.text = motivo.isNotEmpty()
                                ? motivo
                                : juce::String ("Nessuna risposta dal fornitore, e nessun motivo riportato.");
                        }

                        aiEngine->finalizeStreamingResponse(capturedPrompt, structured.text);

                        if (sessionManager)
                            sessionManager->logAiResponse(structured.text, durationMs);

                        broadcastSessionEvent("ai_response",
                            {{"preview", structured.text.substring(0, 80).toStdString()},
                             {"duration_ms", durationMs},
                             {"success", success}});

                        // Anche il percorso in streaming lascia una traccia:
                        // prima scriveva nel log solo quello non-streaming, e
                        // siccome lo streaming e' il predefinito, ogni
                        // diagnosi partiva da un log che si fermava a
                        // "Prompt received".
                        log(juce::String("[AI] ") + (success ? "Risposta" : "FALLITA")
                            + " in " + juce::String(durationMs) + "ms"
                            + (success ? juce::String(": ") + structured.text.substring(0, 60)
                                       : juce::String(" — ") + structured.text.substring(0, 160)));

                        nlohmann::json response;
                        response["type"] = "ai.response";
                        response["id"] = capturedReqId.toStdString();
                        response["timestamp"] = juce::Time::currentTimeMillis();
                        response["payload"]["status"] = success ? "success" : "error";
                        response["payload"]["provider"] = aiEngine->getProviderName().toStdString();
                        response["payload"]["model"]    = aiEngine->getModelName().toStdString();
                        response["payload"]["content"]  = structured.text.toStdString();
                        response["payload"]["durationMs"] = durationMs;
                        if (!success)
                            response["payload"]["error"] = structured.text.toStdString();

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
      }
      catch (const std::exception& e)
      {
          // Un'eccezione qui dentro faceva terminare il thread in silenzio:
          // la UI restava a "sta pensando" per sempre e il flag non tornava
          // mai basso. Ora si dice cos'e' successo e si sblocca.
          const juce::String motivo (e.what());
          log("[AI] Eccezione durante la richiesta: " + motivo);

          nlohmann::json err;
          err["type"] = "ai.response";
          err["id"] = capturedReqId.toStdString();
          err["timestamp"] = juce::Time::currentTimeMillis();
          err["payload"]["status"] = "error";
          err["payload"]["content"] = ("Errore interno durante la richiesta: " + motivo).toStdString();
          err["payload"]["error"] = motivo.toStdString();
          if (wsServer) wsServer->broadcast(err);
      }
      catch (...)
      {
          log("[AI] Errore sconosciuto durante la richiesta");

          nlohmann::json err;
          err["type"] = "ai.response";
          err["id"] = capturedReqId.toStdString();
          err["timestamp"] = juce::Time::currentTimeMillis();
          err["payload"]["status"] = "error";
          err["payload"]["content"] = "Errore interno durante la richiesta.";
          if (wsServer) wsServer->broadcast(err);
      }
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

        // Ogni impostazione accettata finisce subito sul disco: se il DAW
        // si chiude male, o si apre un altro progetto, la scelta resta.
        if (pluginProcessor && responsePayload.contains("status")
            && responsePayload["status"] == "ok")
            pluginProcessor->saveUserSettings();
    };

    // ── Read path (config.get) ──────────────────────────────────
    if (isRead)
    {
        nlohmann::json rp;
        rp["key"] = key;

        if (configKey == "ai.provider")
        {
            // Il nome canonico ("anthropic"), non quello da mostrare
            // ("Anthropic Claude"): la UI confronta questo valore con gli id
            // dei suoi pulsanti, e con il nome esteso nessuno corrispondeva,
            // quindi all'apertura la scelta salvata non risultava mai
            // selezionata.
            rp["value"] = aiEngine
                ? AiEngine::providerToId (aiEngine->getConfig().provider).toStdString()
                : std::string ("none");
            rp["label"] = aiEngine ? aiEngine->getProviderName().toStdString() : "none";
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
        else if (configKey == "ai.status")
        {
            // Quadro completo dell'accesso, in un colpo solo.
            //
            // Serve perche' il plugin e' l'unico a sapere davvero come sta
            // messo: le credenziali le tiene lui su disco. Prima la UI
            // ripeteva al plugin quello che aveva nel proprio archivio locale
            // del browser, e quella copia poteva essere vecchia: bastava un
            // vecchio apiKey rimasto li' per cancellare, alla riconnessione,
            // l'abbonamento Claude appena collegato.
            if (aiEngine)
            {
                const auto& cfg = aiEngine->getConfig();
                const bool haChiave  = cfg.apiKey.isNotEmpty();
                const bool haAbbon   = cfg.authToken.isNotEmpty();
                const bool inLocale  = cfg.provider == AiEngine::Provider::Ollama;

                rp["provider"]        = AiEngine::providerToId (cfg.provider).toStdString();
                rp["label"]           = aiEngine->getProviderName().toStdString();
                rp["model"]           = cfg.model.toStdString();
                rp["effort"]          = cfg.effort.toStdString();
                rp["hasKey"]          = haChiave;
                rp["hasSubscription"] = haAbbon;
                // "pronto" significa: c'e' una credenziale utilizzabile,
                // oppure il fornitore non ne richiede nessuna.
                rp["ready"]           = inLocale || haChiave || haAbbon;
            }
            else
            {
                rp["ready"] = false;
            }
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
            // updateConfig e non configure: configure sostituisce l'intera
            // configurazione, quindi cambiare provider azzererebbe chiave,
            // modello e tutto il resto impostato prima.
            aiEngine->updateConfig([&provider](AiEngine::Config& cfg)
            {
                cfg.provider = AiEngine::providerFromId (juce::String (provider), cfg.provider);

                // Ogni fornitore ha bisogno di un modello che gli appartenga:
                // tenere quello del fornitore precedente produce un 404 al
                // primo prompt, con l'utente convinto di aver scelto bene.
                switch (cfg.provider)
                {
                    case AiEngine::Provider::Ollama:
                        if (cfg.baseUrl.isEmpty()) cfg.baseUrl = "http://localhost:11434";
                        if (cfg.model.isEmpty())   cfg.model = "llama3.2";
                        break;

                    case AiEngine::Provider::Anthropic:
                        if (cfg.model.isEmpty() || ! cfg.model.startsWith ("claude-"))
                            cfg.model = AnthropicProvider::defaultModel();
                        break;

                    case AiEngine::Provider::OpenRouter:
                        // Solo modelli gratuiti: chi arriva qui non deve
                        // ritrovarsi a pagare senza averlo chiesto.
                        if (cfg.model.isEmpty() || ! OpenRouterProvider::isFreeModel (cfg.model))
                            cfg.model = OpenRouterProvider::freeModels()[0];
                        break;

                    case AiEngine::Provider::Gemini:
                        if (cfg.model.isEmpty() || ! cfg.model.startsWith ("gemini"))
                            cfg.model = "gemini-2.0-flash";
                        break;

                    case AiEngine::Provider::OpenAI:
                        if (cfg.model.isEmpty() || ! cfg.model.startsWith ("gpt"))
                            cfg.model = "gpt-4o";
                        break;

                    case AiEngine::Provider::Groq:
                        if (cfg.model.isEmpty())
                            cfg.model = "llama-3.3-70b-versatile";
                        break;
                }
            });
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
            aiEngine->updateConfig([&model](AiEngine::Config& cfg)
            {
                cfg.model = juce::String(model.data());
            });
        }

        nlohmann::json rp;
        rp["key"] = "ai.model";
        rp["value"] = model;
        rp["status"] = "ok";
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.apiKey")
    {
        // Il campo "provider" e' facoltativo. Prima era obbligatorio, e ogni
        // messaggio che non lo portava cadeva nel nulla: la chiave sembrava
        // accettata dalla UI ma il motore non la riceveva mai.
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string provider = (payload.contains("provider") && payload["provider"].is_string())
                             ? payload["provider"].get<std::string>()
                             : AiEngine::providerToId (aiEngine ? aiEngine->getConfig().provider
                                                                : AiEngine::Provider::Ollama).toStdString();
        std::string apiKey = payload["value"].get<std::string>();
        log("[CONFIG] API key set for: " + juce::String(provider.data()));
        
        if (aiEngine)
        {
            aiEngine->updateConfig([&apiKey](AiEngine::Config& cfg)
            {
                cfg.apiKey = juce::String(apiKey.data());
                // Chiave API e abbonamento sono alternativi: impostarne una
                // deve annullare l'altro, altrimenti resterebbero entrambi
                // e il token vincerebbe silenziosamente sulla chiave.
                cfg.authToken = {};
            });
        }

        nlohmann::json rp;
        rp["key"] = "ai.apiKey";
        rp["provider"] = provider;
        rp["status"] = "ok";
        rp["masked"] = true;
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.authToken")
    {
        // Login con un abbonamento Claude (Pro / Max / Claude Code): al posto
        // di una chiave API si passa il token OAuth, che il provider manda
        // come Bearer. La UI lo ottiene dal flusso di login e non lo rilegge
        // mai indietro.
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string token = payload["value"].get<std::string>();

        // Un valore vuoto non cancella niente, a meno che non lo si chieda
        // apposta. Bastava un messaggio con il campo vuoto — e la UI ne
        // mandava uno a ogni conferma quando l'abbonamento era gia' stato
        // rilevato sulla macchina — per buttare via il token appena
        // collegato, e la verifica subito dopo diceva "credenziali
        // rifiutate" senza che nessuno avesse toccato l'accesso.
        const bool cancella = payload.contains("clear") && payload["clear"].is_boolean()
                           && payload["clear"].get<bool>();

        if (token.empty() && !cancella)
        {
            log("[CONFIG] Token abbonamento: messaggio vuoto ignorato, quello esistente resta");
            nlohmann::json rp;
            rp["key"] = "ai.authToken";
            rp["status"] = "ok";
            rp["connected"] = aiEngine && aiEngine->getConfig().authToken.isNotEmpty();
            sendConfigResponse(rp);
            return;
        }

        log("[CONFIG] Claude subscription token " + juce::String(token.empty() ? "cleared" : "set"));

        if (aiEngine)
        {
            aiEngine->updateConfig([&token](AiEngine::Config& cfg)
            {
                cfg.authToken = juce::String(token.data());
                if (! token.empty())
                {
                    cfg.apiKey = {};
                    cfg.provider = AiEngine::Provider::Anthropic;
                    if (cfg.model.isEmpty() || ! cfg.model.startsWith ("claude-"))
                        cfg.model = AnthropicProvider::defaultModel();
                }
            });
        }

        nlohmann::json rp;
        rp["key"] = "ai.authToken";
        rp["status"] = "ok";
        rp["masked"] = true;
        rp["connected"] = ! token.empty();
        sendConfigResponse(rp);
    }
    else if (configKey == "daw.preset")
    {
        // Le porte OSC non sono le stesse fra le DAW, e sbagliarle e' il
        // modo piu' comune di ritrovarsi con un plugin che "non fa niente"
        // senza alcun messaggio d'errore. Qui si sceglie la DAW e le porte
        // si impostano da sole.
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        const juce::String preset (payload["value"].get<std::string>().data());

        int sendPort = 0, receivePort = 0;
        if (preset == "ableton")
        {
            // AbletonOSC ascolta sulla 11000 e risponde sulla 11001.
            sendPort = 11000; receivePort = 11001;
        }
        else if (preset == "reaper")
        {
            // In Reaper le porte si scelgono in Preferenze; queste sono
            // quelle suggerite nella guida del progetto.
            sendPort = 8000; receivePort = 9000;
        }
        else
        {
            nlohmann::json err;
            err["key"] = "daw.preset";
            err["status"] = "error";
            err["message"] = "Preset ammessi: ableton, reaper";
            sendConfigResponse(err);
            return;
        }

        setDawTarget ("127.0.0.1", sendPort);
        log("[CONFIG] Preset " + preset + ": invio su " + juce::String(sendPort)
            + ", ascolto su " + juce::String(receivePort));

        nlohmann::json rp;
        rp["key"] = "daw.preset";
        rp["value"] = preset.toStdString();
        rp["status"] = "ok";
        rp["sendPort"] = sendPort;
        rp["receivePort"] = receivePort;
        // La porta di ascolto si applica al riavvio del listener: la UI
        // deve dirlo, altrimenti sembra che il preset non abbia funzionato.
        rp["receivePortNeedsRestart"] = (receivePort != oscPort);
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.openLogin")
    {
        // Apre nel browser la pagina dove l'utente accede davvero con la
        // propria email e trova la chiave da riportare qui. Non e' ancora
        // il ritorno automatico di un OAuth completo — quello richiede un
        // client registrato presso il fornitore e una pagina di rientro —
        // ma e' il percorso piu' breve che funziona: un clic, il login
        // vero, e si torna.
        if (!payload.contains("provider") || !payload["provider"].is_string()) return;
        const juce::String provider (payload["provider"].get<std::string>().data());

        juce::String url;
        if (provider == "anthropic")       url = "https://console.anthropic.com/settings/keys";
        else if (provider == "openai")     url = "https://platform.openai.com/api-keys";
        else if (provider == "gemini")     url = "https://aistudio.google.com/app/apikey";
        else if (provider == "openrouter") url = "https://openrouter.ai/keys";
        else if (provider == "groq")       url = "https://console.groq.com/keys";

        if (url.isEmpty())
        {
            nlohmann::json err;
            err["key"] = "ai.openLogin";
            err["status"] = "error";
            err["message"] = "Nessuna pagina di accesso per questo provider";
            sendConfigResponse(err);
            return;
        }

        const bool aperto = juce::URL(url).launchInDefaultBrowser();
        log("[AUTH] Pagina di accesso " + provider + ": " + (aperto ? "aperta" : "NON aperta"));

        nlohmann::json rp;
        rp["key"] = "ai.openLogin";
        rp["provider"] = provider.toStdString();
        rp["url"] = url.toStdString();
        // Non "ok": non deve far scattare il salvataggio delle impostazioni,
        // qui non e' cambiato niente da salvare.
        rp["status"] = aperto ? "opened" : "failed";
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.detectSubscription")
    {
        // La UI chiede se sulla macchina c'e' gia' un abbonamento Claude
        // utilizzabile, per poter offrire l'accesso senza chiedere nulla.
        auto token = AnthropicProvider::detectSubscriptionToken();

        if (token.isNotEmpty() && aiEngine)
        {
            aiEngine->updateConfig([&token](AiEngine::Config& cfg)
            {
                cfg.authToken = token;
                cfg.apiKey = {};
                cfg.provider = AiEngine::Provider::Anthropic;
                if (cfg.model.isEmpty() || ! cfg.model.startsWith ("claude-"))
                    cfg.model = AnthropicProvider::defaultModel();
            });
        }

        nlohmann::json rp;
        rp["key"] = "ai.detectSubscription";
        rp["status"] = "ok";
        rp["found"] = token.isNotEmpty();
        // Se non c'e' un token ma Claude Code e' installato, si puo' ancora
        // offrire il collegamento con un clic invece di un campo vuoto.
        rp["claudeCode"] = AnthropicProvider::findClaudeCodeExecutable().existsAsFile();
        // Il token non torna mai indietro: alla UI basta sapere che c'e'.
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.linkClaude")
    {
        // Un clic solo: Claude Code apre il browser, l'utente approva, il
        // token torna qui gia' autorizzato per l'uso da programmi terzi.
        // Non si chiede all'utente di incollare niente.
        auto esito = AnthropicProvider::linkAccountViaClaudeCode();

        if (esito.ok && aiEngine)
        {
            aiEngine->updateConfig([&esito](AiEngine::Config& cfg)
            {
                cfg.authToken = esito.token;
                cfg.apiKey = {};
                cfg.provider = AiEngine::Provider::Anthropic;
                if (cfg.model.isEmpty() || ! cfg.model.startsWith ("claude-"))
                    cfg.model = AnthropicProvider::defaultModel();
            });

            // Conservato a parte, fuori dalle impostazioni: cosi' resta
            // valido fra un avvio e l'altro senza doverlo riemettere.
            auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("WhyCremisi").getChildFile ("accesso-claude.txt");
            f.getParentDirectory().createDirectory();
            f.replaceWithText (esito.token);
        }

        log("[AUTH] Collegamento Claude Code: " + juce::String(esito.ok ? "riuscito" : "fallito")
            + " - " + esito.message);

        nlohmann::json rp;
        rp["key"] = "ai.linkClaude";
        rp["status"] = esito.ok ? "ok" : "error";
        rp["linked"] = esito.ok;
        rp["message"] = esito.message.toStdString();
        // Anche qui il token resta dentro: la UI non deve mai vederlo.
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.effort")
    {
        // Profondita' di ragionamento su Claude. Piu' alta significa risposte
        // migliori sui compiti difficili e piu' token spesi.
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string effort = payload["value"].get<std::string>();

        static const juce::StringArray validEffort { "low", "medium", "high", "xhigh", "max" };
        if (! validEffort.contains (juce::String (effort.data())))
        {
            nlohmann::json err;
            err["key"] = "ai.effort";
            err["status"] = "error";
            err["message"] = "Valori ammessi: low, medium, high, xhigh, max";
            sendConfigResponse(err);
            return;
        }

        log("[CONFIG] Claude effort set to: " + juce::String(effort.data()));

        if (aiEngine)
        {
            aiEngine->updateConfig([&effort](AiEngine::Config& cfg)
            {
                cfg.effort = juce::String(effort.data());
            });
        }

        nlohmann::json rp;
        rp["key"] = "ai.effort";
        rp["value"] = effort;
        rp["status"] = "ok";
        sendConfigResponse(rp);
    }
    else if (configKey == "ai.ollamaUrl")
    {
        if (!payload.contains("value") || !payload["value"].is_string()) return;
        std::string url = payload["value"].get<std::string>();
        log("[CONFIG] Ollama URL set to: " + juce::String(url.data()));
        
        if (aiEngine)
        {
            aiEngine->updateConfig([&url](AiEngine::Config& cfg)
            {
                cfg.baseUrl = juce::String(url.data());
                cfg.provider = AiEngine::Provider::Ollama;
            });
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

void OscBridge::updateScope(float rmsDb, const std::vector<float>& scopePoints)
{
    lastRms.store(rmsDb);
    const juce::ScopedLock sl(spectrumLock);
    lastScopePoints = scopePoints;
}

void OscBridge::updateTempoAndKey(float bpm, float bpmConfidence,
                                   const juce::String& key, float keyConfidence)
{
    lastBpm.store(bpm);
    lastBpmConfidence.store(bpmConfidence);
    lastKeyConfidence.store(keyConfidence);
    const juce::ScopedLock sl(spectrumLock);
    lastKey = key;
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

    // Consumo: a ogni risposta la UI riceve i token della singola richiesta
    // e il totale di sessione, cosi' l'utente vede quanto sta spendendo.
    aiEngine->onUsageUpdate = [this](const AIProvider::Usage& last,
                                     const AiEngine::SessionUsage& session)
    {
        nlohmann::json msg;
        msg["type"] = "ai.usage";
        msg["payload"]["last"]["input"]      = last.inputTokens;
        msg["payload"]["last"]["output"]     = last.outputTokens;
        msg["payload"]["last"]["cacheRead"]  = last.cacheReadTokens;
        msg["payload"]["last"]["cacheWrite"] = last.cacheWriteTokens;
        msg["payload"]["session"]["input"]      = session.inputTokens;
        msg["payload"]["session"]["output"]     = session.outputTokens;
        msg["payload"]["session"]["cacheRead"]  = session.cacheReadTokens;
        msg["payload"]["session"]["cacheWrite"] = session.cacheWriteTokens;
        msg["payload"]["session"]["requests"]   = session.requests;
        msg["payload"]["session"]["total"]      = session.total();
        broadcastJson(msg);
    };

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
        // ── Catena DSP interna ──────────────────────────────────────
        else if (call.name == "dsp.compressor.set") {
            auto* dsp = getDspEngine();
            if (! dsp || ! dsp->compressor) {
                result.success = false;
                result.output = "Catena DSP non disponibile";
            } else {
                juce::StringArray applicati;
                auto leggi = [&](const char* chiave, auto setter, const char* etichetta) {
                    if (call.arguments.contains(chiave) && call.arguments[chiave].is_number()) {
                        const float v = call.arguments[chiave].get<float>();
                        setter(v);
                        applicati.add (juce::String(etichetta) + " " + juce::String(v, 1));
                    }
                };
                leggi("threshold", [&](float v){ dsp->compressor->setThreshold(v); }, "soglia");
                leggi("ratio",     [&](float v){ dsp->compressor->setRatio(v); },     "rapporto");
                leggi("attack",    [&](float v){ dsp->compressor->setAttack(v); },    "attacco");
                leggi("release",   [&](float v){ dsp->compressor->setRelease(v); },   "rilascio");
                leggi("makeup",    [&](float v){ dsp->compressor->setMakeup(v); },    "recupero");

                // Impostare valori senza attivare il modulo non avrebbe
                // effetto udibile, e sembrerebbe che il comando sia stato
                // ignorato.
                applyDspBypass (DSPEngine::CompModule, false);

                result.success = true;
                result.output = applicati.isEmpty()
                    ? "Compressore attivato con i valori correnti"
                    : "Compressore attivo: " + applicati.joinIntoString(", ").toStdString();
            }
        }
        else if (call.name == "dsp.limiter.set") {
            auto* dsp = getDspEngine();
            if (! dsp || ! dsp->limiter) {
                result.success = false;
                result.output = "Catena DSP non disponibile";
            } else {
                if (call.arguments.contains("ceiling") && call.arguments["ceiling"].is_number())
                    dsp->limiter->setThreshold (call.arguments["ceiling"].get<float>());
                if (call.arguments.contains("release") && call.arguments["release"].is_number())
                    dsp->limiter->setRelease (call.arguments["release"].get<float>());

                applyDspBypass (DSPEngine::LimitModule, false);
                result.success = true;
                result.output = "Limiter attivo a "
                    + juce::String (dsp->limiter->getThreshold(), 1).toStdString() + " dB";
            }
        }
        else if (call.name == "dsp.bypass") {
            auto* dsp = getDspEngine();
            const std::string modulo = call.arguments.contains("module")
                ? call.arguments["module"].get<std::string>() : "";
            const bool bypassed = call.arguments.contains("bypassed")
                ? call.arguments["bypassed"].get<bool>() : true;

            if (! dsp) {
                result.success = false;
                result.output = "Catena DSP non disponibile";
            } else if (modulo == "eq") {
                applyDspBypass (DSPEngine::EqModule, bypassed);
                result.success = true;
            } else if (modulo == "compressor") {
                applyDspBypass (DSPEngine::CompModule, bypassed);
                result.success = true;
            } else if (modulo == "limiter") {
                applyDspBypass (DSPEngine::LimitModule, bypassed);
                result.success = true;
            } else if (modulo == "all") {
                applyDspBypass (DSPEngine::EqModule, bypassed);
                applyDspBypass (DSPEngine::CompModule, bypassed);
                applyDspBypass (DSPEngine::LimitModule, bypassed);
                result.success = true;
            } else {
                result.success = false;
                result.output = "Modulo sconosciuto: " + modulo + " (usa eq, compressor, limiter o all)";
            }

            if (result.success)
                result.output = modulo + (bypassed ? " bypassato" : " attivo");
        }
        else if (call.name == "mix.analyze") {
            auto* dsp = getDspEngine();
            if (! dsp || ! dsp->analyzer) {
                result.success = false;
                result.output = "Analyzer non disponibile";
            } else {
                const auto& d = dsp->analyzer->getData();

                nlohmann::json m;
                m["loudness"]["integrated"] = d.integratedLoudness;
                m["loudness"]["shortTerm"]  = d.shortTermLoudness;
                m["loudness"]["momentary"]  = d.momentaryLoudness;
                m["loudness"]["range"]      = d.loudnessRange;
                m["truePeak"]      = d.truePeak;
                m["rms"]           = d.rms;
                m["crestFactor"]   = d.truePeak - d.rms;
                m["correlation"]   = d.corrMono;
                m["clippingCount"] = d.clippingCount;
                m["spectralCentroid"] = d.spectralCentroid;
                m["spectralRolloff"]  = d.spectralRolloff;

                // Tempo e tonalita' con la loro affidabilita': su materiale
                // senza pulsazione chiara o senza tonalita' definita i
                // numeri escono comunque, ed e' la confidenza a dire se
                // vanno presi sul serio.
                if (d.bpm > 0.0f)
                {
                    m["bpm"] = d.bpm;
                    m["bpmConfidence"] = d.bpmConfidence;
                }
                if (d.key.isNotEmpty())
                {
                    m["key"] = d.key.toStdString();
                    m["keyConfidence"] = d.keyConfidence;

                    // La relativa condivide le stesse note, e le stime di
                    // tonalita' scivolano spesso fra le due: darla insieme
                    // alla principale evita di far passare per certa una
                    // scelta che il segnale non permette di fare.
                    static const char* nomi[12] =
                        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                    const bool minore = d.key.endsWith ("min");
                    const juce::String tonica = d.key.upToFirstOccurrenceOf (" ", false, true);
                    for (int i = 0; i < 12; ++i)
                    {
                        if (tonica != nomi[i]) continue;
                        // Relativa minore: tre semitoni sotto. Maggiore: tre sopra.
                        const int rel = minore ? (i + 3) % 12 : (i + 9) % 12;
                        m["keyRelative"] = juce::String (nomi[rel]).toStdString()
                                         + (minore ? " maj" : " min");
                        break;
                    }
                }
                m["riferimenti"]["confidenza"] = "sotto 0.3 il valore e' poco affidabile, non citarlo come certo";
                m["riferimenti"]["tonalita"] = "la stima puo' cadere sulla relativa o sulla dominante: se la confidenza non e' alta, dilla come ipotesi";

                nlohmann::json bande = nlohmann::json::array();
                for (int b = 0; b < Analyzer::FFTData::numBands; ++b)
                    bande.push_back (d.bandEnergy[(size_t) b]);
                m["bandEnergyPercent"] = bande;

                // I riferimenti viaggiano insieme alle misure: senza, il
                // modello dovrebbe ricordarsi a memoria cosa sia un buon
                // valore, e su questo si sbaglia facilmente.
                m["riferimenti"]["lufsStreaming"] = "-14 Spotify, -16 Apple; sotto -8 e' guerra del volume";
                m["riferimenti"]["truePeak"]      = "<= -1 dBTP";
                m["riferimenti"]["loudnessRange"] = "5-9 LU musica moderna; sotto 4 e' schiacciato";
                m["riferimenti"]["crestFactor"]   = "14-20 dB non compresso; sotto 9 e' limitato a morte";
                m["riferimenti"]["correlazione"]  = "> 0; sotto zero collassa in mono";
                m["riferimenti"]["bande"]         = "dieci bande log da 20 Hz a 20 kHz, in percentuale di energia";

                result.success = true;
                result.output = m.dump();
            }
        }
        else if (call.name == "dsp.getState") {
            auto* dsp = getDspEngine();
            if (! dsp) {
                result.success = false;
                result.output = "Catena DSP non disponibile";
            } else {
                nlohmann::json stato;
                stato["eq"]["bypassed"]        = dsp->isBypassed (DSPEngine::EqModule);
                stato["compressor"]["bypassed"] = dsp->isBypassed (DSPEngine::CompModule);
                stato["limiter"]["bypassed"]    = dsp->isBypassed (DSPEngine::LimitModule);
                if (dsp->compressor) {
                    stato["compressor"]["threshold"] = dsp->compressor->getThreshold();
                    stato["compressor"]["ratio"]     = dsp->compressor->getRatio();
                    stato["compressor"]["attack"]    = dsp->compressor->getAttack();
                    stato["compressor"]["release"]   = dsp->compressor->getRelease();
                    stato["compressor"]["makeup"]    = dsp->compressor->getMakeup();
                    stato["compressor"]["gainReduction"] = dsp->compressor->getCurrentReduction();
                }
                if (dsp->limiter) {
                    stato["limiter"]["ceiling"] = dsp->limiter->getThreshold();
                    stato["limiter"]["release"] = dsp->limiter->getRelease();
                }
                result.success = true;
                result.output = stato.dump();
            }
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
    // Scrive sempre, anche in Release: e' la versione che gira nel DAW, ed
    // e' proprio li' che serve sapere cosa e' successo. Prima queste righe
    // stavano dietro #ifndef NDEBUG e durante una prova vera il file
    // restava vuoto.
    whycremisi::log ("bridge", msg);
}
