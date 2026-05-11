#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class AgentIdentity
{
public:
    AgentIdentity();

    juce::String name;
    juce::String creature;
    juce::String vibe;
    juce::String emoji;
    juce::String avatarUrl;

    bool isComplete() const;
    juce::String toContextString() const;

    juce::ValueTree save();
    void load(const juce::ValueTree& tree);
};
