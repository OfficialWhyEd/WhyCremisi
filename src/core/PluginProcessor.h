/*
  ==============================================================================
  PluginProcessor.h
  WhyCremisi VST Plugin - Main Audio Processor
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <map>

// Forward declarations
class AiEngine;
class OscBridge;
class SessionManager;

//==============================================================================
class WhyCremisiProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    WhyCremisiProcessor();
    ~WhyCremisiProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    #endif

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "WhyCremisi VST Plugin"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }

    //==============================================================================
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // WhyCremisi specific methods
    
    // AI Engine
    void sendAiPrompt(const juce::String& prompt);
    juce::String getLastAiResponse() const { return lastAiResponse; }
    
    // AI Configuration
    void updateAiEngineConfig();
    juce::String getAiProvider() const;
    juce::String getAiModel() const;
    void setAiApiKey(const juce::String& provider, const juce::String& apiKey);
    juce::String getAiApiKey(const juce::String& provider) const;
    
    // Parameters
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    
    // OSC
    int getOscPort() const { return oscPort; }
    void setOscPort(int port);

    // OscBridge (OSC + WebSocket for React UI)
    OscBridge* getOscBridge() const { return oscBridge.get(); }
    bool isOscBridgeRunning() const;
    int getOscBridgeWsPort() const;

private:
    //==============================================================================
    // Parameters
    juce::AudioProcessorValueTreeState parameters;
    
    // Parameter pointers
    std::atomic<float>* gainParam1 = nullptr;
    std::atomic<float>* gainParam2 = nullptr;
    std::atomic<float>* aiEnabled = nullptr;
    std::atomic<float>* aiProvider = nullptr;
    std::atomic<float>* aiModelIndex = nullptr;
    std::atomic<float>* oscPortParam = nullptr;
    std::atomic<float>* dawOscPortParam = nullptr;
    std::atomic<float>* wsPortParam = nullptr;
    
    // AI Engine
    std::unique_ptr<AiEngine> aiEngine;
    juce::String lastAiResponse;
    
    // API Keys storage (not exposed as parameters for security)
    mutable juce::CriticalSection apiKeyLock;
    std::map<juce::String, juce::String> apiKeys; // provider -> key
    
    // OscBridge (OSC + WebSocket for React UI)
    std::unique_ptr<OscBridge> oscBridge;
    int oscPort = 9000;
    juce::String dawIp = "127.0.0.1";
    int dawOscPort = 9001;
    int wsPort = 8080;
    juce::CriticalSection dawIpLock; // Protects dawIp
    
    // Session Manager
    std::unique_ptr<SessionManager> sessionManager;

    // Meter state (computed in processBlock, broadcast periodically)
    std::atomic<float> meterLevelL { -60.0f };
    std::atomic<float> meterLevelR { -60.0f };
    int meterBroadcastCounter { 0 };
    static constexpr int METER_BROADCAST_EVERY = 512; // blocks

    // Last known transport state (change detection in processBlock)
    bool  lastIsPlaying   { false };
    bool  lastIsRecording { false };
    float lastBpm         { 120.0f };

    // Audio device info (set in prepareToPlay, broadcast to UI)
    double currentSampleRate  { 44100.0 };
    int    currentBufferSize  { 512 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhyCremisiProcessor)
};