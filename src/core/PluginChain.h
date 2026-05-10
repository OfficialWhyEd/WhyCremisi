#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

struct PluginInfo
{
    juce::String id;             // unique identifier
    juce::String name;           // display name (e.g. "FabFilter Pro-Q 3")
    juce::String manufacturer;   // e.g. "FabFilter"
    juce::String format;         // "VST3", "AU", "AAX"
    int slot = 0;                // position in chain (0 = first)
    bool enabled = true;

    juce::ValueTree save() const;
    static PluginInfo load(const juce::ValueTree& tree);
};

class PluginChain
{
public:
    void setPlugins(const std::vector<PluginInfo>& plugins);
    void addPlugin(const PluginInfo& plugin);
    void removePlugin(const juce::String& id);
    void updatePlugin(const PluginInfo& plugin);
    void clear();

    const std::vector<PluginInfo>& getPlugins() const { return plugins; }
    PluginInfo* getPlugin(const juce::String& id);
    int size() const { return static_cast<int>(plugins.size()); }

    juce::ValueTree save() const;
    void load(const juce::ValueTree& tree);

    // Build a human-readable chain description for AI prompt
    juce::String toDescriptiveString() const;

private:
    std::vector<PluginInfo> plugins;
    juce::CriticalSection lock;
};
