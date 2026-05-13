#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <functional>
#include <map>

#include "AgentIdentity.h"
#include "AgentSoul.h"
#include "AgentUser.h"

class PersonalityCore;

struct MemoryEntry
{
    juce::String date;
    juce::String text;
    juce::String category;
    int64_t timestamp;

    juce::ValueTree save() const;
    static MemoryEntry load(const juce::ValueTree& tree);
};

struct HeartbeatState
{
    std::map<juce::String, int64_t> lastCheckTimestamps;
    juce::StringArray tasks;

    juce::ValueTree save() const;
    void load(const juce::ValueTree& tree);
};

class AgentWorkspace
{
public:
    AgentWorkspace();

    AgentIdentity identity;
    AgentSoul soul;
    AgentUser user;

    // AGENTS.md rules
    bool redLinesEnabled;
    bool memoryEnabled;
    bool heartbeatEnabled;
    juce::StringArray extraRules;

    // BOOT.md / BOOTSTRAP.md state
    bool bootstrapComplete;
    bool bootEnabled;
    juce::String bootInstructions;

    // HEARTBEAT.md state
    HeartbeatState heartbeat;

    // TOOLS.md
    juce::String toolsNotes;

    // MEMORY (daily notes + long-term)
    std::vector<MemoryEntry> dailyMemory;
    std::vector<MemoryEntry> longTermMemory;

    void linkPersonalityCore(PersonalityCore* pc);

    void markBootstrapComplete();
    void setBootInstructions(const juce::String& instructions);
    void addMemoryEntry(const juce::String& text, const juce::String& category = "general");
    void addToolNote(const juce::String& note);
    void addRule(const juce::String& rule);
    void addHeartbeatTask(const juce::String& task);
    void touchHeartbeatCheck(const juce::String& checkName);
    bool isHeartbeatStale(const juce::String& checkName, int64_t maxAgeMs) const;

    juce::String buildFullContext() const;
    juce::String buildMemoryContext() const;
    juce::String buildHeartbeatContext() const;
    juce::String buildToolsContext() const;
    juce::String buildBootstrapContext() const;

    void deriveStyleFromPersonality(const PersonalityCore& pc);
    void refreshFromPersonalityCore(const PersonalityCore& pc);
    void resetToDefaults();

    std::function<void(const juce::String& type, const juce::String& detail)> onWorkspaceEvent;

    juce::ValueTree save();
    void load(const juce::ValueTree& tree);

private:
    PersonalityCore* personalityCore;
};
