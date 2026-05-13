#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

class AgentSoul
{
public:
    AgentSoul();

    enum Style { Analytical, Consultative, Direct, Creative, Warm };
    static Style styleFromString(const juce::String& s);
    static juce::String styleToString(Style s);

    Style style;
    juce::String vibeDescription;
    std::vector<juce::String> coreTruths;
    std::vector<juce::String> boundaries;

    void addCoreTruth(const juce::String& truth);
    void addBoundary(const juce::String& boundary);
    bool isComplete() const;

    juce::String toContextString() const;
    juce::ValueTree save();
    void load(const juce::ValueTree& tree);
};
