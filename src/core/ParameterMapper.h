#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>

class MidiHandler;
class OscBridge;

class ParameterMapper
{
public:
    enum class TargetType { midiCc, dawOsc };

    struct Binding
    {
        juce::String widgetId;
        TargetType type = TargetType::midiCc;
        juce::String oscAddress;
        float currentValue = 0.0f;
        float min = 0.0f;
        float max = 1.0f;
        float defaultValue = 0.0f;
    };

    void setMidiHandler(MidiHandler* mh) { midiHandler = mh; }
    void setOscBridge(OscBridge* ob) { oscBridge = ob; }

    void setValue(const juce::String& widgetId, float normalizedValue);
    float getValue(const juce::String& widgetId) const;

    void registerBinding(const Binding& binding);
    void removeBinding(const juce::String& widgetId);
    Binding* getBinding(const juce::String& widgetId);

    juce::ValueTree save();
    void load(const juce::ValueTree& tree);

    const std::map<juce::String, Binding>& getBindings() const { return bindings; }

private:
    std::map<juce::String, Binding> bindings;
    MidiHandler* midiHandler = nullptr;
    OscBridge* oscBridge = nullptr;
};
