/*
  ==============================================================================
  OscHandler.h
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  OSC Communication Handler
  
  Bidirectional OSC: receives from DAW and sends back.
  Receive: UDP listener on configurable port (default 9000)
  Send: UDP to target host:port (default localhost:9001)
  
  Phase 1 COMPLETE: Receive + Send
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
    /** Callback type for incoming OSC messages */
    using OscCallback = std::function<void(const juce::String& address, float value)>;
    using OscStringCallback = std::function<void(const juce::String& address, const juce::String& value)>;
    
    // Legacy callback type for backward compatibility
    using MessageCallback = std::function<void(const juce::String&, const juce::var&)>;
    
    //==============================================================================
    OscHandler(int port = 9000);
    ~OscHandler();
    
    //==============================================================================
    /** Start/stop the OSC listener */
    void start();
    void stop();
    bool isRunning() const { return running.load() && connected.load(); }
    
    //==============================================================================
    /** Send OSC messages to DAW */
    void sendMessage(const juce::String& address, float value);
    void sendMessage(const juce::String& address, const juce::String& value);
    void sendMessage(const juce::String& address, int value);
    
    //==============================================================================
    /** Set send target (DAW OSC address) */
    void setSendTarget(const juce::String& host, int sendPort);
    juce::String getSendHost() const { return sendHost; }
    int getSendPort() const { return sendPortNum; }
    
    //==============================================================================
    /** Set callbacks for incoming messages */
    void setCallback(OscCallback callback);
    void setStringCallback(OscStringCallback callback);
    void setMessageCallback(MessageCallback callback);  // Legacy support
    
    //==============================================================================
    /** Set the OSC port (restarts listener if running) */
    void setPort(int newPort);
    int getPort() const { return port; }
    
    //==============================================================================
    /** Get the OSC message log (for UI/debug) */
    const juce::StringArray& getMessageLog() const { return messageLog; }
    void clearLog() { messageLog.clear(); }
    
    //==============================================================================
    /** OSC learn mode - captures next incoming address */
    void enableLearnMode(bool enable) { learnMode.store(enable); }
    bool isInLearnMode() const { return learnMode.load(); }
    juce::String getLearnedAddress() const { return learnedAddress; }
    
    //==============================================================================
    /** Connection status */
    bool isConnected() const { return connected.load(); }
    int getMessagesReceived() const { return messagesReceived.load(); }

private:
    //==============================================================================
    int port;
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> learnMode{false};
    std::atomic<int> messagesReceived{0};
    std::atomic<int> messagesSent{0};
    
    juce::String learnedAddress;
    
    //==============================================================================
    /** Listener thread */
    std::unique_ptr<std::thread> listenerThread;
    void listenerLoop();
    
    //==============================================================================
    /** Message parsing */
    void handleOscPacket(const char* data, int size);
    void handleOscMessage(const juce::String& address, const juce::String& typeTag, 
                          const char* arguments, int argSize);
    
    //==============================================================================
    /** Callbacks */
    OscCallback callback;
    OscStringCallback stringCallback;
    MessageCallback messageCallback;  // Legacy
    
    //==============================================================================
    /** Message log (thread-safe via lock) */
    juce::StringArray messageLog;
    juce::CriticalSection logLock;
    void addToLog(const juce::String& msg);
    
    //==============================================================================
    /** UDP sockets */
    std::unique_ptr<juce::DatagramSocket> socket;        // Receive socket
    std::unique_ptr<juce::DatagramSocket> sendSocket;     // Send socket
    
    //==============================================================================
    /** Send target */
    juce::String sendHost = "127.0.0.1";
    int sendPortNum = 9001;
    
    //==============================================================================
    /** OSC binary encoding helpers */
    void writeOscAddress(juce::MemoryBlock& dest, const juce::String& address);
    void writeOscTypeTag(juce::MemoryBlock& dest, const juce::String& typeTag);
    void writeOscFloatArg(juce::MemoryBlock& dest, float value);
    void writeOscIntArg(juce::MemoryBlock& dest, int32_t value);
    void writeOscStringArg(juce::MemoryBlock& dest, const juce::String& value);
    void padToFourBytes(juce::MemoryBlock& dest);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscHandler)
};