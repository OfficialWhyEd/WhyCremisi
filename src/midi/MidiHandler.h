#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>
#include <vector>
#include <functional>

class MidiHandler
{
public:
    struct MidiMapping
    {
        int ccNumber = -1;
        int channel  = 0;
        float minValue = 0.0f;
        float maxValue = 1.0f;
    };

    void setMapping(const juce::String& widgetId, int cc, int channel,
                    float minVal = 0.0f, float maxVal = 1.0f);
    void removeMapping(const juce::String& widgetId);
    MidiMapping* getMapping(const juce::String& widgetId);

    void sendCC(const juce::String& widgetId, float normalizedValue);
    void flush(juce::MidiBuffer& buffer, int sampleOffset);

    // MIDI Learn
    void startLearn(const juce::String& widgetId);
    void stopLearn();
    bool isLearning() const { return learningWidget.isNotEmpty(); }
    juce::String getLearningWidget() const { return learningWidget; }
    void onMidiInput(const juce::MidiMessage& msg);

    // Callback when a MIDI Learn completes
    std::function<void(const juce::String& widgetId, int cc, int channel)> learnCompleteCallback;

    juce::ValueTree save();
    void load(const juce::ValueTree& tree);

    const std::map<juce::String, MidiMapping>& getMappings() const { return mappings; }

private:
    std::map<juce::String, MidiMapping> mappings;
    std::vector<juce::MidiMessage> pendingCC;
    juce::String learningWidget;
    juce::CriticalSection lock;
};
