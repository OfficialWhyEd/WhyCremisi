/*
  ==============================================================================
  OscBridge.h
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  Bidirectional OSC-WebSocket Bridge

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
#include <mutex>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

// Forward declarations
class AiEngine;
class SessionManager;
class MidiHandler;
class ParameterMapper;
class PluginChain;
class WhyCremisiProcessor;
class IDawHandler;
class DSPEngine;
class SampleLibrary;
struct ToolCall;
struct ToolResult;
enum class DawType;

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

    /** Set Plugin Processor for program/preset management */
    void setPluginProcessor(WhyCremisiProcessor* proc) { pluginProcessor = proc; }

    /** Set Session Manager for event logging */
    void setSessionManager(SessionManager* sm);

    /** Get current AI response status */
    bool isAiProcessing() const { return aiProcessing.load(); }

    /** Callback: widget value changed from UI → ParameterMapper */
    std::function<void(const juce::String&, float)> widgetChangeCallback;

    //==============================================================================
    /** Called from prepareToPlay with real audio device info */
    void broadcastPluginStats(double sampleRate, int bufferSize);

    /** Called from processBlock to report CPU usage */
    void setCpuUsage(double cpuPct, double peakTimeUs);

    /** Called from processBlock to update analyzer data (thread-safe) */
    void updateAnalyzer(float correlation, float momentaryLoudness, float shortTermLoudness, float integratedLoudness, float truePeak, const std::vector<float>& spectrum, int clippingCount);

    /** RMS reale e nuvola di punti del vectorscope, dal processBlock. */
    void updateScope(float rmsDb, const std::vector<float>& scopePoints);

    /** Tempo e tonalita' stimati, con la rispettiva affidabilita'. */
    void updateTempoAndKey(float bpm, float bpmConfidence,
                           const juce::String& key, float keyConfidence);

    /** Misure di mix: gamma di loudness, baricentro, rolloff, energia per banda.
        L'analizzatore le calcola gia' tutte, ma finora restavano dentro il
        plugin e uscivano solo quando l'AI chiedeva mix.analyze. Il cruscotto
        dell'interfaccia, intanto, si scriveva da solo dei numeri inventati a
        partire dal livello dei meter. */
    void updateMixMetrics (float loudnessRange, float spectralCentroid,
                           float spectralRolloff, const float* bandEnergy, int numBands);

    /** Riduzione di guadagno applicata dal compressore, in dB. */
    void updateGainReduction(float gainReductionDb) { lastGainReduction.store(gainReductionDb); }

    /** Called from processBlock to update meter levels (thread-safe) */
    void updateMeter(float leftDb, float rightDb)
    {
        lastMeterL.store(leftDb);
        lastMeterR.store(rightDb);
    }

    /** Called from processBlock to update position/bpm from getPlayHead() */
    void setPosition(float positionSeconds, float bpm)
    {
        currentPosition.store(positionSeconds);
        currentBpm.store(bpm);
    }

private:
    // juce::Timer callback — broadcasts position + meters at 30ms intervals
    void timerCallback() override;

    //==============================================================================
    // Called when OSC message is received from DAW
    void onOscReceived(const juce::String& address, float value);
    void onOscStringReceived(const juce::String& address, const juce::String& value);

    // WebSocket message handler (receives from UI)
    void handleWebSocketMessage(const nlohmann::json& message);

    // Connection handler
    void handleClientConnection(int clientId, bool connected);

    // DAW state (for broadcasting) — atomics for cross-thread safety
    std::atomic<bool> currentIsPlaying {false};
    std::atomic<bool> currentIsRecording {false};
    std::atomic<float> currentBpm {120.0f};
    std::atomic<float> currentPosition {0.0f};
    juce::uint32 lastTimerTimeMs = 0;

    // Ableton Live track cache (trackId -> track info)
    struct AbletonTrackInfo {
        juce::String name;
        float volume = 0.0f;
        float pan = 0.0f;
        bool mute = false;
        bool solo = false;
        std::vector<float> sends;
        int numDevices = 0;
    };
    std::map<int, AbletonTrackInfo> abletonTracks;
    int abletonTrackCount = 0;
    bool abletonDetected = false;

    /** Catena DSP del plugin, se il processor e' collegato. */
    DSPEngine* getDspEngine() const;

    /** Attiva o disattiva un modulo della catena e comunica alla UI il
        nuovo stato, cosi' i pulsanti restano allineati alla realta'. */
    void applyDspBypass (int module, bool bypassed);

    /** Manda alla UI l'assetto corrente della catena DSP. */
    void broadcastDspState();

    void handleAbletonTrackData(const juce::String& address, float value);
    /** Risposte di AbletonOSC, dove l'indice della traccia e' il primo
        argomento del messaggio invece che parte dell'indirizzo. */
    void handleAbletonMultiArg(const juce::String& address, const std::vector<float>& args);
    void handleAbletonTrackString(const juce::String& address, const juce::String& value);
    void discoverAbletonTracks();
    void broadcastAbletonTrack(int trackId);

    // DAW detection & handler
    DawType detectDawType(const juce::String& address) const;
    void ensureDawHandler(DawType type);
    IDawHandler* getOrCreateDawHandler(DawType type);
    void broadcastDawInfo();

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
    // Ritorna false se il payload e' malformato o il comando sconosciuto:
    // in quel caso non parte nulla verso la DAW. true significa solo
    // "consegnato al gestore/OSC", mai "eseguito dalla DAW".
    bool dispatchDawCommand(const nlohmann::json& payload);
    bool dispatchDawCommandValidated(const nlohmann::json& payload, const std::string& cmd);
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

    // Strumenti AI daw.* -> dispatchDawCommand. Unico percorso, usato sia
    // dall'esecutore dell'AiEngine sia dai test. Ritorna false se il nome
    // dello strumento non e' un comando DAW gestito qui.
    bool tryExecuteDawTool(const ToolCall& call, ToolResult& result);

    /** samples.scan, samples.search, beat.create: indice dei suoni e costruzione del beat. */
    void eseguiToolSample(const ToolCall& call, ToolResult& result);

    /** device.list, device.params, device.setParam: plugin di una traccia di Live, letti e impostati con conferma. */
    void eseguiToolDevice(const ToolCall& call, ToolResult& result);

    /** Registra una risposta di Live con testi e numeri mescolati (liste di device e parametri). */
    void registraRispostaPiena (const juce::String& address, const std::vector<OscHandler::OscArg>& args);

    /** Interroga Live e ritorna gli argomenti della risposta (indici compresi). Blocca: thread AI. */
    bool leggiBlocco (const juce::String& indirizzo, const std::vector<float>& argomenti,
                      std::vector<OscHandler::OscArg>& out, int tentativi, int attesaMs);

    /** mix.levels, mix.snapshot.save, mix.snapshot.recall: mixer di Live riletto traccia per traccia. */
    void eseguiToolMix(const ToolCall& call, ToolResult& result);

    // I test di regressione iniettano un IDawHandler che registra le
    // chiamate e invocano il percorso produttivo qui sopra.
    friend class OscBridgeTest;

    // AI Engine reference (for ai.prompt messages)
    AiEngine*       aiEngine       = nullptr;
    MidiHandler*    midiHandler    = nullptr;
    ParameterMapper* paramMapper   = nullptr;
    PluginChain*     pluginChain   = nullptr;
    SessionManager* sessionManager = nullptr;
    WhyCremisiProcessor* pluginProcessor = nullptr;
    std::unique_ptr<IDawHandler> dawHandler;
    std::unique_ptr<SampleLibrary> sampleLibrary;   // creata al primo uso dei tool samples.* / beat.create
    DawType detectedDawType = static_cast<DawType>(0);
    std::atomic<bool> aiProcessing {false};

    // Quando e' iniziata la richiesta in corso. Serve a distinguere "sta
    // ancora lavorando" da "e' rimasto incastrato": se il flag e' alto da piu'
    // del tempo massimo concesso a una risposta, quella richiesta e' morta e
    // va sbloccata. Senza questo, un solo fallimento non gestito rendeva il
    // plugin muto per il resto della sessione, rispondendo a ogni prompt
    // "AI is already processing a request".
    std::atomic<juce::int64> aiStartedAtMs {0};
    static constexpr juce::int64 aiStalloMs = 120000;   // 2 minuti

    // Ultima volta che la DAW ci ha parlato. OSC viaggia su UDP e non
    // conferma niente: l'unico modo onesto di sapere se dall'altra parte
    // c'e' qualcuno e' se qualcosa e' mai arrivato indietro. Serve a non
    // rispondere "fatto" all'AI quando il comando e' partito verso il nulla.
    std::atomic<juce::int64> lastOscFromDawMs {0};
    static constexpr juce::int64 dawSilenzioMs = 30000;

public:
    /** Vero se la DAW ha inviato qualcosa di recente. Falso non significa
        che il comando fallira', ma che non abbiamo alcuna prova che qualcuno
        stia ascoltando — e va detto invece di dare per scontato. */
    bool dawHaRisposto() const
    {
        const auto t = lastOscFromDawMs.load();
        return t > 0 && (juce::Time::currentTimeMillis() - t) < dawSilenzioMs;
    }

    //==========================================================================
    // Conferma reale dei comandi (Live / AbletonOSC)
    //
    //  OSC su UDP non conferma. L'unica prova e' rileggere: dopo un comando si
    //  chiede a Live il valore e lo si confronta con quello atteso. Ogni
    //  risposta di Live finisce qui, con un numero progressivo, cosi' chi
    //  aspetta sa distinguere una risposta nuova da una vecchia.
    //==========================================================================
    struct RispostaDaw
    {
        double          value = 0.0;
        juce::int64     seq   = 0;     // 0 = mai arrivata
    };

    /** Chiave di una risposta: indirizzo, e per le tracce "#indice". */
    static std::string chiaveRisposta (const juce::String& address, int trackId = -1)
    {
        std::string k = address.toStdString();
        if (trackId >= 0) k += "#" + std::to_string (trackId);
        return k;
    }

    /** Registra una risposta ricevuta dalla DAW. Chiamata dai callback OSC. */
    void registraRispostaDaw (const juce::String& address, const std::vector<float>& args);

    /** Ultima risposta registrata per la chiave (seq == 0 se non c'e'). */
    RispostaDaw ultimaRispostaDaw (const std::string& chiave) const;

    /** Esito della verifica di un comando rispetto al valore atteso. */
    struct EsitoLettura
    {
        enum class Stato { Confermato, Diverso, NessunaRisposta, NonVerificabile };
        Stato  stato  = Stato::NonVerificabile;
        double letto  = 0.0;     // valido se stato e' Confermato o Diverso
    };

    /** Interroga la DAW con `indirizzoGet` (+ `argomenti` interi) e attende la
        risposta sulla chiave. Confronta con `atteso` entro `tolleranza`.
        Ritenta fino a `tentativi` volte: Live applica i comandi al giro
        successivo, la prima lettura puo' essere ancora quella vecchia.
        Blocca il thread che chiama: va usata dal thread AI, mai dalla UI. */
    EsitoLettura verificaConLettura (const juce::String& indirizzoGet,
                                     const std::vector<float>& argomenti,
                                     int trackId, double atteso, double tolleranza,
                                     int tentativi, int attesaMs);

    /** Per i test: sostituisce l'invio OSC della domanda a Live. */
    std::function<void (const juce::String&, const std::vector<float>&, const std::vector<bool>&)> inviaDomandaDaw;

private:
    mutable std::mutex          rispostePerChiaveMutex;
    std::map<std::string, RispostaDaw> rispostePerChiave;
    std::atomic<juce::int64>    rispostaSeq {0};
    struct RispostaPiena { std::vector<OscHandler::OscArg> args; juce::int64 seq = 0; };
    std::map<std::string, RispostaPiena> rispostePiene;
public:

private:

    // AI thread (async dispatch)
    std::unique_ptr<std::thread> aiThread;

    // Meter state written from processBlock, read by timerCallback
    std::atomic<float> lastMeterL { -60.0f };
    std::atomic<float> lastMeterR { -60.0f };
    int meterTickCounter { 0 }; // only broadcast meter every N timer ticks

    // Analyzer state (written from processBlock, broadcast periodically)
    std::atomic<float> lastCorrelation { 0.0f };
    std::atomic<float> lastMomentaryLoudness { -96.0f };
    std::atomic<float> lastShortTermLoudness { -96.0f };
    std::atomic<float> lastIntegratedLoudness { -96.0f };
    std::atomic<float> lastTruePeak { -96.0f };
    std::atomic<int> lastClippingCount { 0 };
    std::atomic<float> lastRms { -96.0f };
    std::atomic<float> lastGainReduction { 0.0f };
    std::atomic<float> lastBpm { 0.0f };
    std::atomic<float> lastBpmConfidence { 0.0f };
    std::atomic<float> lastKeyConfidence { 0.0f };
    std::atomic<float> lastLoudnessRange { 0.0f };
    std::atomic<float> lastSpectralCentroid { 0.0f };
    std::atomic<float> lastSpectralRolloff { 0.0f };
    juce::String lastKey;   // protetta da spectrumLock
    std::vector<float> lastSpectrum;
    std::vector<float> lastBandEnergy;   // protetta da spectrumLock
    std::vector<float> lastScopePoints;
    juce::CriticalSection spectrumLock;

    // CPU usage (set from processBlock via setCpuUsage, broadcast from timer)
    std::atomic<double> lastCpuPct {0.0};
    std::atomic<double> lastPeakTimeUs {0.0};

    // Audio device info (set from prepareToPlay via broadcastPluginStats)
    std::atomic<double> lastSampleRate { 44100.0 };
    std::atomic<int>    lastBufferSize  { 512 };

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