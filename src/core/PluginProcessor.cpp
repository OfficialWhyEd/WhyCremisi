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
#include "SessionManager.h"
#include "MidiHandler.h"
#include "ParameterMapper.h"
#include "PluginChain.h"
#include "DSPEngine.h"
#include "PersonalityCore.h"
#include "AgentWorkspace.h"
#include <nlohmann/json.hpp>
#include <signal.h>

// Ignore SIGPIPE: when a WebSocket client disconnects, socket write returns -1
// instead of killing the process. Check errno == EPIPE in sendFrame.
static bool sigpipeIgnored = []() {
    signal(SIGPIPE, SIG_IGN);
    return true;
}();

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
        
        // OSC Receive Port (plugin listens for OSC from DAW on this port)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "oscPort", "OSC Port", 1024, 65535, 9000));
        
        // DAW OSC Send Port (where to send OSC messages to DAW)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "dawOscPort", "DAW OSC Port", 1024, 65535, 9001));
        
        // WebSocket Port (for communication with React UI)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "wsPort", "WebSocket Port", 1024, 65535, 8080));
        
        return { params.begin(), params.end() };
    }())
{
    // Get parameter pointers
    gainParam1 = parameters.getRawParameterValue("gain1");
    gainParam2 = parameters.getRawParameterValue("gain2");
    gainParam3 = parameters.getRawParameterValue("gain3");
    gainParam4 = parameters.getRawParameterValue("gain4");
    gainParam5 = parameters.getRawParameterValue("gain5");
    gainParam6 = parameters.getRawParameterValue("gain6");
    gainParam7 = parameters.getRawParameterValue("gain7");
    gainParam8 = parameters.getRawParameterValue("gain8");
    aiEnabled = parameters.getRawParameterValue("aiEnabled");
    aiProvider = parameters.getRawParameterValue("aiProvider");
    aiModelIndex = parameters.getRawParameterValue("aiModelIndex");
    oscPortParam = parameters.getRawParameterValue("oscPort");
    dawOscPortParam = parameters.getRawParameterValue("dawOscPort");
    wsPortParam = parameters.getRawParameterValue("wsPort");
    
    // Initialize AI Engine
    aiEngine = std::make_unique<AiEngine>();
    updateAiEngineConfig();
    
    // Initialize Session Manager
    sessionManager = std::make_unique<SessionManager>();
    sessionManager->startSession("Unknown DAW");
    
    // Initialize parameters from values
    oscPort = static_cast<int>(oscPortParam->load());
    dawIp = parameters.state.getProperty("dawIp", "127.0.0.1").toString();
    dawOscPort = static_cast<int>(dawOscPortParam->load());
    wsPort = static_cast<int>(wsPortParam->load());

    // Initialize Personality Core (memory + AI personality)
    personalityCore = std::make_unique<PersonalityCore>();
    personalityCore->startSession();
    personalityCore->onPersonalityEvent = [this](const juce::String& type, const juce::String& detail) {
        if (oscBridge)
        {
            nlohmann::json msg;
            msg["type"] = ("personality." + type).toStdString();
            msg["payload"]["detail"] = detail.toStdString();
            oscBridge->broadcastJson(msg);
        }
    };

    // Initialize AgentWorkspace (OpenClaw templates: identity, soul, user, memory, etc.)
    agentWorkspace = std::make_unique<AgentWorkspace>();
    agentWorkspace->linkPersonalityCore(personalityCore.get());
    agentWorkspace->onWorkspaceEvent = [this](const juce::String& type, const juce::String& detail) {
        if (oscBridge)
        {
            nlohmann::json msg;
            msg["type"] = ("workspace." + type).toStdString();
            msg["payload"]["detail"] = detail.toStdString();
            oscBridge->broadcastJson(msg);
        }
    };

    // Initialize MIDI + Parameter Mapping
    midiHandler = std::make_unique<MidiHandler>();
    paramMapper = std::make_unique<ParameterMapper>();
    paramMapper->setMidiHandler(midiHandler.get());

    // Initialize Plugin Chain
    pluginChain = std::make_unique<PluginChain>();

    // Initialize DSP Engine
    dspEngine = std::make_unique<DSPEngine>();
    
    // Initialize OscBridge (OSC receive port from parameter + WebSocket port from parameter)
    oscBridge = std::make_unique<OscBridge>(oscPort, wsPort);
    oscBridge->setAiEngine(aiEngine.get());
    oscBridge->setSessionManager(sessionManager.get());
    oscBridge->setDawTarget(dawIp, dawOscPort);  // DAW IP and OSC port from parameters
    oscBridge->setMidiHandler(midiHandler.get());
    oscBridge->setParameterMapper(paramMapper.get());
    oscBridge->setPluginChain(pluginChain.get());
    oscBridge->setPluginProcessor(this);
    paramMapper->setOscBridge(oscBridge.get());

    // Route widget changes from UI → ParameterMapper → MIDI/OSC + Personality + Memory
    oscBridge->widgetChangeCallback = [this](const juce::String& widgetId, float value) {
        if (paramMapper)
        {
            float prev = paramMapper->getValue(widgetId);
            paramMapper->setValue(widgetId, value);
            if (personalityCore)
                personalityCore->recordAction(widgetId, value, prev);
            if (agentWorkspace)
            {
                agentWorkspace->addMemoryEntry(
                    "User adjusted " + widgetId + " from " + juce::String(prev, 2)
                    + " to " + juce::String(value, 2), "interaction");
            }
            refreshAiContext();
        }
    };

    // Set up AI engine widget context + action execution + personality
    if (aiEngine)
    {
        aiEngine->setActionCallback([this](const AiEngine::AiAction& action) {
            if (paramMapper)
            {
                float prev = paramMapper->getValue(action.widgetId);
                paramMapper->setValue(action.widgetId, action.value);
                if (personalityCore)
                    personalityCore->recordAction(action.widgetId, action.value, prev,
                                                  action.description);
                if (agentWorkspace && action.description.isNotEmpty())
                {
                    agentWorkspace->addMemoryEntry(
                        "AI " + action.description + " (" + action.widgetId
                        + ": " + juce::String(prev, 2) + " -> " + juce::String(action.value, 2) + ")",
                        "ai_action");
                }
                refreshAiContext();
            }
        });

        // Provide initial widget list to AI engine
        std::vector<AiEngine::WidgetInfo> widgetList;
        auto addWidget = [&](const juce::String& id, const juce::String& label,
                              float val, float mn, float mx, const juce::String& unit = {})
        {
            AiEngine::WidgetInfo w;
            w.widgetId = id; w.label = label; w.currentValue = val;
            w.min = mn; w.max = mx; w.unit = unit;
            widgetList.push_back(w);
        };
        addWidget("masterGain", "Master Gain", gainParam1 ? gainParam1->load() : 0.0f, -60.0f, 12.0f, "dB");
        for (int i = 1; i <= 8; ++i)
        {
            auto id = "gain" + juce::String(i);
            float val = 0.0f;
            auto ptr = parameters.getRawParameterValue(id);
            if (ptr) val = ptr->load();
            addWidget(id, "Gain " + juce::String(i), val, -60.0f, 12.0f, "dB");
        }
        aiEngine->setWidgetList(widgetList);

        // Plugin chain context
        if (pluginChain)
        {
            juce::String chainDesc = pluginChain->toDescriptiveString();
            aiEngine->setContext(chainDesc, {});
        }

        // Personality context (stored separately from plugin chain / transport)
        if (personalityCore)
            aiEngine->setPersonalityContext(personalityCore->buildPersonalityContext());

        // AgentWorkspace context (OpenClaw templates: identity, soul, user, memory, rules, tools, heartbeat)
        if (agentWorkspace)
            aiEngine->setAgentWorkspaceContext(agentWorkspace->buildFullContext());
    }

    // MIDI Learn complete: notify UI via broadcast
    midiHandler->learnCompleteCallback = [this](const juce::String& widgetId, int cc, int channel) {
        if (oscBridge)
        {
            nlohmann::json response;
            response["type"] = "midi.learn.complete";
            response["timestamp"] = juce::Time::currentTimeMillis();
            response["payload"]["widgetId"] = widgetId.toStdString();
            response["payload"]["cc"] = cc;
            response["payload"]["channel"] = channel;
            oscBridge->broadcastJson(response);
        }
    };

    // Start bridge immediately so it's available before prepareToPlay
    // (critical for Standalone mode where prepareToPlay needs an audio device)
    {
        bool ok = oscBridge->start();
        DBG("[WhyCremisi] OscBridge early start: " + juce::String(ok ? "OK" : "FAILED"));
        if (!ok)
            DBG("[WhyCremisi] OscBridge error: " + oscBridge->getLastError());
    }

    // Initialize factory programs (presets)
    programNames.add("Default");
    programNames.add("Bright Mix");
    programNames.add("Warm Mix");
    programNames.add("Dark Mix");
    programNames.add("Mastering");
    programNames.add("Broadcast");
    programNames.add("Low End Focus");
    programNames.add("Airy Highs");

    // Broadcast initial program state
    broadcastCurrentProgram();

    // Listen for parameter changes to propagate to bridge dynamically
    parameters.addParameterListener("dawOscPort", this);
}

WhyCremisiProcessor::~WhyCremisiProcessor()
{
    parameters.removeParameterListener("dawOscPort", this);
    if (oscBridge)
        oscBridge->stop();
    if (sessionManager)
        sessionManager->endSession();
}

//==============================================================================
void WhyCremisiProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBufferSize = samplesPerBlock;
    
    auto logToFile = [](const juce::String& msg) {
        DBG(msg);
#ifndef NDEBUG
        juce::File logFile("/tmp/whycremisi-debug.log");
        juce::String timestamp = juce::Time::getCurrentTime().toString(true, true, true, true);
        logFile.appendText("[" + timestamp + "] " + msg + "\n");
#endif
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
        // Push real audio device info to any connected UI clients
        oscBridge->broadcastPluginStats(sampleRate, samplesPerBlock);
    }
    else
    {
        logToFile("[WhyCremisi] OscBridge is NULL!");
    }

    // Initialize parameter smoothing (100ms ramp for zipper-free automation)
    smoothedGain.reset(sampleRate, 0.1);
    smoothingInitialised = true;
    if (gainParam1)
        smoothedGain.setTargetValue(juce::Decibels::decibelsToGain(gainParam1->load()));

    // Prepare DSP Engine
    if (dspEngine)
        dspEngine->prepare(sampleRate, samplesPerBlock);
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

    // Route incoming MIDI messages to MidiHandler (for MIDI Learn)
    if (midiHandler)
    {
        for (const auto& metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            if (msg.isController())
                midiHandler->onMidiInput(msg);
        }
    }

    // Flush MIDI CC output messages (control external plugins)
    if (midiHandler)
        midiHandler->flush(midiMessages, 0);

    // MIDI Through: keep incoming MIDI in output when learning
    bool isLearning = midiHandler && midiHandler->isLearning();
    if (!isLearning && !midiThroughEnabled)
        midiMessages.clear();

    const int totalIn   = getTotalNumInputChannels();
    const int totalOut  = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, numSamples);

    // Apply smoothed gain for zipper-free automation
    if (gainParam1 != nullptr)
    {
        float targetGain = juce::Decibels::decibelsToGain(gainParam1->load());
        if (smoothingInitialised)
        {
            smoothedGain.setTargetValue(targetGain);
            if (!smoothedGain.isSmoothing())
                buffer.applyGain(targetGain);
            else
                buffer.applyGain(smoothedGain.getNextValue());
        }
        else
        {
            buffer.applyGain(targetGain);
        }
    }

    // ── DSP Processing ──────────────────────────────────────────────
    double processStart = juce::Time::getMillisecondCounterHiRes();
    if (dspEngine)
    {
        dspEngine->process(buffer);

        // Push analyzer data to OscBridge for UI broadcast
        auto& dspAnalyzer = dspEngine->analyzer;
        if (dspAnalyzer && dspAnalyzer->hasNewData() && oscBridge)
        {
            const auto& data = dspAnalyzer->getData();

            std::vector<float> spectrum = data.magnitudes;
            oscBridge->updateAnalyzer(data.corrMono, data.momentaryLoudness, data.shortTermLoudness,
                                      data.integratedLoudness, data.truePeak, spectrum, data.clippingCount);
            dspAnalyzer->clearNewData();
        }
    }

    // CPU usage tracking
    double elapsedUs = (juce::Time::getMillisecondCounterHiRes() - processStart) * 1000.0;
    lastProcessTimeUs = elapsedUs;
    if (elapsedUs > peakProcessTimeUs) peakProcessTimeUs = elapsedUs;
    avgProcessTimeUs = avgProcessTimeUs * (1.0 - 1.0 / TIMING_HISTORY_SIZE) + elapsedUs / TIMING_HISTORY_SIZE;
    cpuCounter++;

    if (oscBridge && cpuCounter % cpuBroadcastEvery == 0)
    {
        double blockTimeUs = (static_cast<double>(currentBufferSize) / currentSampleRate) * 1e6;
        double cpuPct = juce::jmin(100.0, (avgProcessTimeUs / blockTimeUs) * 100.0);
        oscBridge->broadcastPluginStats(currentSampleRate, currentBufferSize);
        oscBridge->broadcastCpuUsage(cpuPct, peakProcessTimeUs);
        peakProcessTimeUs = avgProcessTimeUs;  // reset peak to average for next window

        // CPU throttle detection
        if (cpuPct > cpuThrottleThreshold) {
            cpuHighCount++;
            if (cpuHighCount > 10 && !cpuThrottled) {
                cpuThrottled = true;
                cpuThrottleCooldown = 500;
                cpuBroadcastEvery = juce::jmin(128, cpuBroadcastEvery * 2);
            }
        } else {
            cpuHighCount = 0;
            if (cpuThrottled) {
                cpuThrottleCooldown--;
                if (cpuThrottleCooldown <= 0) {
                    cpuThrottled = false;
                    cpuBroadcastEvery = juce::jmax(4, cpuBroadcastEvery / 2);
                }
            }
        }
    }

    // ── Universal transport via getPlayHead() ───────────────────────
    // Works in every DAW that implements VST3: Ableton, Logic, REAPER,
    // FL Studio, Cubase, Studio One, Bitwig, Pro Tools, GarageBand...
    if (auto* ph = getPlayHead())
    {
        auto opt = ph->getPosition();
        if (opt)
        {
            bool  playing   = opt->getIsPlaying();
            bool  recording = opt->getIsRecording();
            float bpm       = static_cast<float>(opt->getBpm().orFallback(120.0));
            float posSec    = static_cast<float>(opt->getTimeInSeconds().orFallback(0.0));

            // Only broadcast + log when something actually changed
            bool changed = (playing   != lastIsPlaying)   ||
                           (recording != lastIsRecording)  ||
                           (std::abs(bpm - lastBpm) > 0.01f);

            if (changed)
            {
                lastIsPlaying   = playing;
                lastIsRecording = recording;
                lastBpm         = bpm;

                if (oscBridge && oscBridge->isRunning())
                    oscBridge->broadcastTransport(playing, recording, bpm, posSec);

                if (sessionManager)
                    sessionManager->logTransport(playing, recording, bpm, posSec);
            }

            // Always push position to bridge (timer will broadcast it)
            if (oscBridge)
                oscBridge->setPosition(posSec, bpm);
        }
    }

    // ── RMS meters ──────────────────────────────────────────────────
    if (totalOut >= 1)
    {
        float rmsL = buffer.getRMSLevel(0, 0, numSamples);
        float rmsR = (totalOut >= 2) ? buffer.getRMSLevel(1, 0, numSamples) : rmsL;

        float dbL = rmsL > 0.0001f ? juce::Decibels::gainToDecibels(rmsL) : -60.0f;
        float dbR = rmsR > 0.0001f ? juce::Decibels::gainToDecibels(rmsR) : -60.0f;

        const float attack  = 0.98f;
        const float release = 0.98f;
        float prevL = meterLevelL.load();
        float prevR = meterLevelR.load();
        meterLevelL.store(dbL > prevL ? dbL * attack + prevL * (1.0f - attack) : dbL * (1.0f - release) + prevL * release);
        meterLevelR.store(dbR > prevR ? dbR * attack + prevR * (1.0f - attack) : dbR * (1.0f - release) + prevR * release);

        // Push meter levels into bridge (timer broadcasts at 30fps)
        if (oscBridge)
            oscBridge->updateMeter(meterLevelL.load(), meterLevelR.load());

        // Also direct broadcast every N blocks as backup
        if (++meterBroadcastCounter >= METER_BROADCAST_EVERY && oscBridge && oscBridge->isRunning())
        {
            meterBroadcastCounter = 0;
            oscBridge->broadcastMeter(-1, meterLevelL.load(), meterLevelR.load(),
                                          meterLevelL.load(), meterLevelR.load());
        }

        // Periodic personality context broadcast to keep React UI in sync
        if (++personalityBroadcastCounter >= PERSONALITY_BROADCAST_EVERY && oscBridge && oscBridge->isRunning())
        {
            personalityBroadcastCounter = 0;
            if (personalityCore)
            {
                nlohmann::json msg;
                msg["type"] = "personality.context";
                msg["payload"]["style"] = personalityCore->getPreferredStyle().toStdString();
                msg["payload"]["confidence"] = juce::jmin(1.0f, personalityCore->getTotalActions() / 100.0f);
                msg["payload"]["experienceLevel"] = personalityCore->getTotalActions() / 10;
                msg["payload"]["sessionCount"] = personalityCore->getSessionCount();
                msg["payload"]["totalActions"] = personalityCore->getTotalActions();
                msg["payload"]["userName"] = personalityCore->getUserName().toStdString();
                auto recent = personalityCore->getRecentActions(3);
                nlohmann::json ra = nlohmann::json::array();
                for (const auto& a : recent)
                    ra.push_back(a.toStdString());
                msg["payload"]["recentActions"] = ra;
                msg["payload"]["description"] = personalityCore->buildPersonalityContext().toStdString();
                oscBridge->broadcastJson(msg);
            }
        }
    }
}

//==============================================================================
void WhyCremisiProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < (int)programNames.size() && index != currentProgramIndex.load())
    {
        currentProgramIndex.store(index);
        broadcastCurrentProgram();
    }
}

const juce::String WhyCremisiProcessor::getProgramName(int index)
{
    if (index >= 0 && index < (int)programNames.size())
        return programNames[index];
    return "Default";
}

void WhyCremisiProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index >= 0 && index < (int)programNames.size() && newName.isNotEmpty())
        programNames.set(index, newName);
}

int WhyCremisiProcessor::addProgram(const juce::String& name)
{
    programNames.add(name.isNotEmpty() ? name : ("Program " + juce::String(programNames.size() + 1)));
    return programNames.size() - 1;
}

bool WhyCremisiProcessor::removeProgram(int index)
{
    if (index < 0 || index >= (int)programNames.size())
        return false;
    programNames.remove(index);
    if (currentProgramIndex.load() >= (int)programNames.size())
        currentProgramIndex.store(juce::jmax(0, (int)programNames.size() - 1));
    return true;
}

bool WhyCremisiProcessor::savePreset(const juce::File& file)
{
    juce::MemoryBlock data;
    getStateInformation(data);
    return file.replaceWithData(data.getData(), data.getSize());
}

bool WhyCremisiProcessor::loadPreset(const juce::File& file)
{
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data))
        return false;
    setStateInformation(data.getData(), (int)data.getSize());
    return true;
}

void WhyCremisiProcessor::broadcastCurrentProgram()
{
    if (!oscBridge || !oscBridge->isRunning())
        return;

    nlohmann::json msg;
    msg["type"] = "plugin.program";
    msg["timestamp"] = juce::Time::currentTimeMillis();
    msg["payload"]["currentIndex"] = currentProgramIndex.load();
    msg["payload"]["currentName"] = getProgramName(currentProgramIndex.load()).toStdString();

    nlohmann::json names = nlohmann::json::array();
    for (int i = 0; i < (int)programNames.size(); ++i)
        names.push_back(programNames[i].toStdString());
    msg["payload"]["programs"] = names;

    oscBridge->broadcastJson(msg);
}

//==============================================================================
void WhyCremisiProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty("dawIp", dawIp, nullptr);
    state.setProperty("currentProgram", currentProgramIndex.load(), nullptr);
    {
        juce::ValueTree programsTree("programs");
        for (int i = 0; i < (int)programNames.size(); ++i)
        {
            juce::ValueTree prog("program");
            prog.setProperty("name", programNames[i], nullptr);
            prog.setProperty("index", i, nullptr);
            programsTree.appendChild(prog, nullptr);
        }
        state.appendChild(programsTree, nullptr);
    }
    if (midiHandler)
        state.appendChild(midiHandler->save(), nullptr);
    if (pluginChain)
        state.appendChild(pluginChain->save(), nullptr);
    if (personalityCore)
        state.appendChild(personalityCore->save(), nullptr);
    if (agentWorkspace)
        state.appendChild(agentWorkspace->save(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void WhyCremisiProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(parameters.state.getType()))
    {
        auto restored = juce::ValueTree::fromXml(*xmlState);
        parameters.replaceState(restored);
        dawIp = restored.getProperty("dawIp", "127.0.0.1").toString();

        // Restore program/preset state
        {
            auto programsTree = restored.getChildWithName("programs");
            if (programsTree.isValid())
            {
                juce::StringArray restoredNames;
                for (int i = 0; i < programsTree.getNumChildren(); ++i)
                {
                    auto prog = programsTree.getChild(i);
                    if (prog.hasProperty("name"))
                        restoredNames.add(prog.getProperty("name").toString());
                }
                if (restoredNames.size() > 0)
                    programNames = restoredNames;
            }
            currentProgramIndex.store((int)restored.getProperty("currentProgram", 0));
            if (currentProgramIndex.load() >= (int)programNames.size())
                currentProgramIndex.store(0);
        }
        broadcastCurrentProgram();
        if (midiHandler)
        {
            auto midiTree = restored.getChildWithName("midiMappings");
            if (midiTree.isValid())
                midiHandler->load(midiTree);
        }
        if (pluginChain)
        {
            auto chainTree = restored.getChildWithName("pluginChain");
            if (chainTree.isValid())
                pluginChain->load(chainTree);
        }
        if (personalityCore)
        {
            auto personalityTree = restored.getChildWithName("personalityCore");
            if (personalityTree.isValid())
                personalityCore->load(personalityTree);
        }
        if (agentWorkspace)
        {
            auto wsTree = restored.getChildWithName("agentWorkspace");
            if (wsTree.isValid())
                agentWorkspace->load(wsTree);
        }
        if (oscBridge)
            oscBridge->setDawTarget(dawIp, dawOscPort);

        // Refresh AI engine context after loading personality + workspace
        if (aiEngine)
        {
            if (personalityCore)
                aiEngine->setPersonalityContext(personalityCore->buildPersonalityContext());
            if (agentWorkspace)
                aiEngine->setAgentWorkspaceContext(agentWorkspace->buildFullContext());
        }
    }
}

void WhyCremisiProcessor::refreshAiContext()
{
    if (!aiEngine) return;

    if (personalityCore)
    {
        aiEngine->setPersonalityContext(personalityCore->buildPersonalityContext());
    }

    if (agentWorkspace)
    {
        if (personalityCore)
            agentWorkspace->refreshFromPersonalityCore(*personalityCore);
        aiEngine->setAgentWorkspaceContext(agentWorkspace->buildFullContext());
    }

    // Plugin chain context refresh
    if (pluginChain)
        aiEngine->setContext(pluginChain->toDescriptiveString(), {});
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
    if (oscBridge && oscBridge->isRunning())
    {
        oscBridge->stop();
        oscBridge = std::make_unique<OscBridge>(oscPort, wsPort);
        oscBridge->setAiEngine(aiEngine.get());
        oscBridge->setDawTarget(dawIp, dawOscPort);
        oscBridge->setSessionManager(sessionManager.get());
        oscBridge->setMidiHandler(midiHandler.get());
        oscBridge->setParameterMapper(paramMapper.get());
        oscBridge->setPluginChain(pluginChain.get());
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

void WhyCremisiProcessor::parameterChanged(const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID == "dawOscPort")
    {
        dawOscPort = static_cast<int>(dawOscPortParam->load());
        if (oscBridge)
            oscBridge->setDawTarget(dawIp, dawOscPort);
    }
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