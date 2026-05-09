/*
  ==============================================================================
  PluginProcessor.cpp
  WhyCremisi VST Plugin - Main Audio Processor Implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AiEngine.h"
#include "OscBridge.h"

//==============================================================================
WhyCremisiProcessor::WhyCremisiProcessor()
    : parameters(*this, nullptr, "Parameters", []() -> juce::AudioProcessorValueTreeState::ParameterLayout
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        
        // Gain parameters (8 tracks)
        for (int i = 1; i <= 8; ++i)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                "gain" + juce::String(i), 
                "Gain " + juce::String(i), 
                -60.0f, 12.0f, 0.0f));
        }
        
        // AI Enable
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "aiEnabled", "AI Enabled", true));
        
        // AI Provider
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "aiProvider", "AI Provider",
            juce::StringArray{"Ollama", "OpenAI", "Gemini", "Anthropic", "OpenRouter", "Groq"},
            0));
        
        // AI Model
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "aiModelIndex", "AI Model Index", 0.0f, 100.0f, 0.0f));
        
        // OSC Port
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "oscPort", "OSC Port", 1024, 65535, 9000));
        
        return { params.begin(), params.end() };
    }())
{
    // Get parameter pointers
    gainParam1 = parameters.getRawParameterValue("gain1");
    gainParam2 = parameters.getRawParameterValue("gain2");
    aiEnabled = parameters.getRawParameterValue("aiEnabled");
    aiProvider = parameters.getRawParameterValue("aiProvider");
    aiModelIndex = parameters.getRawParameterValue("aiModelIndex");
    oscPortParam = parameters.getRawParameterValue("oscPort");
    
    // Initialize AI Engine
    aiEngine = std::make_unique<AiEngine>();
    updateAiEngineConfig();
    
    // Initialize OscBridge (single owner of OSC + WebSocket)
    oscBridge = std::make_unique<OscBridge>(oscPort, 8080);
    oscBridge->setAiEngine(aiEngine.get());
    oscBridge->setDawTarget("127.0.0.1", 8000);
}

WhyCremisiProcessor::~WhyCremisiProcessor()
{
    if (oscBridge)
        oscBridge->stop();
}

//==============================================================================
void WhyCremisiProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    
    auto logToFile = [](const juce::String& msg) {
        juce::File logFile("/tmp/whycremisi-debug.log");
        juce::String timestamp = juce::Time::getCurrentTime().toString(true, true, true, true);
        logFile.appendText("[" + timestamp + "] " + msg + "\n");
        DBG(msg);
    };
    
    logToFile("[WhyCremisi] prepareToPlay called");
    
    if (oscBridge)
    {
        bool running = oscBridge->isRunning();
        logToFile("[WhyCremisi] OscBridge running? " + juce::String(running ? "YES" : "NO"));
        if (!running)
        {
            bool started = oscBridge->start();
            logToFile("[WhyCremisi] OscBridge start: " + juce::String(started ? "OK" : "FAILED"));
            if (!started)
                logToFile("[WhyCremisi] OscBridge error: " + oscBridge->getLastError());
        }
    }
    else
    {
        logToFile("[WhyCremisi] OscBridge is NULL!");
    }
}

void WhyCremisiProcessor::releaseResources()
{
    if (oscBridge)
    {
        oscBridge->stop();
        DBG("[WhyCremisi] OscBridge stopped");
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool WhyCremisiProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono())
        return true;
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
        return true;
    return false;
}
#endif

void WhyCremisiProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    if (gainParam1 != nullptr)
    {
        float gain = juce::Decibels::decibelsToGain(gainParam1->load());
        buffer.applyGain(gain);
    }
    
    for (const auto midiMessage : midiMessages)
    {
        auto message = midiMessage.getMessage();
        if (message.isController())
        {
            juce::ignoreUnused(message.getControllerNumber(), message.getControllerValue());
        }
    }
}

//==============================================================================
void WhyCremisiProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String WhyCremisiProcessor::getProgramName(int index) { juce::ignoreUnused(index); return "Default"; }
void WhyCremisiProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

//==============================================================================
void WhyCremisiProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void WhyCremisiProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(parameters.state.getType()))
    {
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

//==============================================================================
// WhyCremisi specific methods

void WhyCremisiProcessor::updateAiEngineConfig()
{
    if (!aiEngine) return;
    
    AiEngine::Config config;
    
    int providerIdx = aiProvider ? static_cast<int>(aiProvider->load()) : 0;
    switch (providerIdx)
    {
        case 0: config.provider = AiEngine::Provider::Ollama; break;
        case 1: config.provider = AiEngine::Provider::OpenAI; break;
        case 2: config.provider = AiEngine::Provider::Gemini; break;
        case 3: config.provider = AiEngine::Provider::Anthropic; break;
        case 4: config.provider = AiEngine::Provider::OpenRouter; break;
        case 5: config.provider = AiEngine::Provider::Groq; break;
        default: config.provider = AiEngine::Provider::Ollama; break;
    }
    
    config.baseUrl = "http://localhost:11434";
    config.model = "llama3.2";
    config.timeoutMs = 30000;
    
    {
        juce::ScopedLock lock(apiKeyLock);
        auto providerName = getAiProvider();
        auto it = apiKeys.find(providerName);
        if (it != apiKeys.end())
            config.apiKey = it->second.toStdString();
    }
    
    aiEngine->configure(config);
}

juce::String WhyCremisiProcessor::getAiProvider() const
{
    if (!aiProvider) return "Ollama";
    int idx = static_cast<int>(aiProvider->load());
    juce::StringArray providers{"Ollama", "OpenAI", "Gemini", "Anthropic", "OpenRouter", "Groq"};
    return providers[idx];
}

juce::String WhyCremisiProcessor::getAiModel() const
{
    if (!aiModelIndex) return "llama3.2";
    int idx = static_cast<int>(aiModelIndex->load());
    
    juce::StringArray ollamaModels{"llama3.2", "llama3.1:8b", "mistral", "codellama"};
    juce::StringArray openaiModels{"gpt-4o-mini", "gpt-4o", "gpt-3.5-turbo"};
    juce::StringArray geminiModels{"gemini-1.5-flash", "gemini-1.5-pro"};
    
    auto provider = getAiProvider();
    if (provider == "Ollama")    return ollamaModels[idx % ollamaModels.size()];
    if (provider == "OpenAI")   return openaiModels[idx % openaiModels.size()];
    if (provider == "Gemini")   return geminiModels[idx % geminiModels.size()];
    
    return "llama3.2";
}

void WhyCremisiProcessor::setAiApiKey(const juce::String& provider, const juce::String& apiKey)
{
    juce::ScopedLock lock(apiKeyLock);
    apiKeys[provider] = apiKey;
    updateAiEngineConfig();
}

juce::String WhyCremisiProcessor::getAiApiKey(const juce::String& provider) const
{
    juce::ScopedLock lock(apiKeyLock);
    auto it = apiKeys.find(provider);
    return (it != apiKeys.end()) ? it->second : juce::String();
}

void WhyCremisiProcessor::sendAiPrompt(const juce::String& prompt)
{
    if (!aiEngine || !aiEnabled || aiEnabled->load() <= 0.5f)
    {
        lastAiResponse = "AI disabled or not initialized";
        return;
    }
    
    updateAiEngineConfig();
    lastAiResponse = aiEngine->sendPrompt(prompt);
}

void WhyCremisiProcessor::setOscPort(int port)
{
    oscPort = port;
    // OscBridge manages its own OscHandler internally
    // Restart if running
    if (oscBridge && oscBridge->isRunning())
    {
        oscBridge->stop();
        // Recreate with new port
        oscBridge = std::make_unique<OscBridge>(oscPort, 8080);
        oscBridge->setAiEngine(aiEngine.get());
    oscBridge->setDawTarget("192.168.1.12", 8000);
        oscBridge->start();
    }
}

bool WhyCremisiProcessor::isOscBridgeRunning() const
{
    return oscBridge ? oscBridge->isRunning() : false;
}

int WhyCremisiProcessor::getOscBridgeWsPort() const
{
    return oscBridge ? oscBridge->getWebSocketPort() : 0;
}

//==============================================================================
juce::AudioProcessorEditor* WhyCremisiProcessor::createEditor()
{
    return new WhyCremisiProcessorEditor(*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WhyCremisiProcessor();
}