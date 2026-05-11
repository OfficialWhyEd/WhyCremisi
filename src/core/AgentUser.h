#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class AgentUser
{
public:
    AgentUser();

    juce::String name;
    juce::String pronouns;
    juce::String timezone;
    juce::String contextNotes;

    bool isComplete() const;
    juce::String toContextString() const;

    juce::ValueTree save();
    void load(const juce::ValueTree& tree);
};
