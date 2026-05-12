/*
  ==============================================================================
  OscHandler.h
  WhyCremisi VST Plugin - OSC Communication Handler
  
  Bidirectional OSC: receives from DAW and sends back.
  Receive: UDP listener on configurable port (default 9000)
  Send: UDP to target host:port (default localhost:9001)
  
  Sections:
    Lifecycle        - constructor, destructor, start/stop
    Transport Cmds   - DAW transport: play, stop, record, etc.
    Mixer Cmds       - Track mixer: gain, pan, mute, solo
    Sending          - sendMessage overloads + sendTarget
    Query            - learn mode, port, connection status
    Callbacks        - incoming message routing
    OSC Parsing      - binary packet -> address + values
    OSC Encoding     - address/type/arg -> binary packet
    Logging          - thread-safe message log
  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <thread>
#include <functional>
#include <atomic>
#include <map>

class OscHandler
{
public:
    //==============================================================================
    // Lifecycle
    //==============================================================================
    OscHandler(int port = 9000);
    ~OscHandler();

    void start();
    void stop();
    bool isRunning() const { return running.load() && connected.load(); }
    void setPort(int newPort);
    int getPort() const { return port; }

    //==============================================================================
    // DAW Transport Commands
    //==============================================================================
    void sendPlay();
    void sendStop();
    void sendRecord();
    void sendTransportAction(const juce::String& action);
    void sendTempo(float bpm);
    void sendPosition(double seconds);

    //==============================================================================
    // DAW Mixer Commands
    //==============================================================================
    void sendGainChange(int trackIndex, float gainDb);
    void sendPanChange(int trackIndex, float pan);
    void sendMuteToggle(int trackIndex);
    void sendSoloToggle(int trackIndex);
    void sendMixerCommand(const juce::String& command, float value);

    //==============================================================================
    // Sending
    //==============================================================================
    void sendMessage(const juce::String& address, float value);
    void sendMessage(const juce::String& address, const juce::String& value);
    void sendMessage(const juce::String& address, int value);
    void setSendTarget(const juce::String& host, int sendPort);
    juce::String getSendHost() const { return sendHost; }
    int getSendPort() const { return sendPortNum; }

    //==============================================================================
    // Query / Status
    //==============================================================================
    void enableLearnMode(bool enable) { learnMode.store(enable); }
    bool isInLearnMode() const { return learnMode.load(); }
    juce::String getLearnedAddress() const { return learnedAddress; }
    bool isConnected() const { return connected.load(); }
    int getMessagesReceived() const { return messagesReceived.load(); }

    //==============================================================================
    // Callbacks (incoming message routing)
    //==============================================================================
    using OscCallback = std::function<void(const juce::String& address, float value)>;
    using OscStringCallback = std::function<void(const juce::String& address, const juce::String& value)>;
    using MessageCallback = std::function<void(const juce::String&, const juce::var&)>;

    void setCallback(OscCallback cb);
    void setStringCallback(OscStringCallback cb);
    void setMessageCallback(MessageCallback cb);

    //==============================================================================
    // Logging
    //==============================================================================
    using LogCallback = std::function<void(const juce::String& line)>;
    void setLogCallback(LogCallback cb) { logCallback = std::move(cb); }
    const juce::StringArray& getMessageLog() const { return messageLog; }
    void clearLog() { messageLog.clear(); }
    int getMessagesSent() const { return messagesSent.load(); }

private:
    //==============================================================================
    // Internal State
    //==============================================================================
    int port;
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> learnMode{false};
    std::atomic<int> messagesReceived{0};
    std::atomic<int> messagesSent{0};
    juce::String learnedAddress;

    //==============================================================================
    // Listener Thread
    //==============================================================================
    std::unique_ptr<std::thread> listenerThread;
    void listenerLoop();

    //==============================================================================
    // OSC Packet Parsing
    //==============================================================================
    void handleOscPacket(const char* data, int size);
    void handleOscMessage(const juce::String& address, const juce::String& typeTag,
                          const char* arguments, int argSize);

    //==============================================================================
    // OSC Binary Encoding
    //==============================================================================
    void writeOscAddress(juce::MemoryBlock& dest, const juce::String& address);
    void writeOscTypeTag(juce::MemoryBlock& dest, const juce::String& typeTag);
    void writeOscFloatArg(juce::MemoryBlock& dest, float value);
    void writeOscIntArg(juce::MemoryBlock& dest, int32_t value);
    void writeOscStringArg(juce::MemoryBlock& dest, const juce::String& value);
    void padToFourBytes(juce::MemoryBlock& dest);

    //==============================================================================
    // Callback Storage
    //==============================================================================
    OscCallback callback;
    OscStringCallback stringCallback;
    MessageCallback messageCallback;

    //==============================================================================
    // Logging
    //==============================================================================
    juce::StringArray messageLog;
    juce::CriticalSection logLock;
    LogCallback logCallback;
    void addToLog(const juce::String& msg);

    //==============================================================================
    // UDP Sockets
    //==============================================================================
    std::unique_ptr<juce::DatagramSocket> socket;
    std::unique_ptr<juce::DatagramSocket> sendSocket;
    juce::String sendHost = "127.0.0.1";
    int sendPortNum = 9001;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscHandler)
};
