#include "ParameterMapper.h"
#include "MidiHandler.h"
#include "OscBridge.h"

void ParameterMapper::setValue(const juce::String& widgetId, float normalizedValue)
{
    auto it = bindings.find(widgetId);
    if (it == bindings.end())
        return;

    auto& binding = it->second;
    binding.currentValue = juce::jlimit(0.0f, 1.0f, normalizedValue);

    switch (binding.type)
    {
        case TargetType::midiCc:
            if (midiHandler)
                midiHandler->sendCC(widgetId, normalizedValue);
            break;

        case TargetType::dawOsc:
            if (oscBridge && binding.oscAddress.isNotEmpty())
            {
                float scaled = binding.min + normalizedValue * (binding.max - binding.min);
                oscBridge->sendOscToDaw(binding.oscAddress, scaled);
            }
            break;
    }
}

float ParameterMapper::getValue(const juce::String& widgetId) const
{
    auto it = bindings.find(widgetId);
    if (it != bindings.end())
        return it->second.currentValue;
    return 0.0f;
}

void ParameterMapper::registerBinding(const Binding& binding)
{
    bindings[binding.widgetId] = binding;
}

void ParameterMapper::removeBinding(const juce::String& widgetId)
{
    bindings.erase(widgetId);
}

ParameterMapper::Binding* ParameterMapper::getBinding(const juce::String& widgetId)
{
    auto it = bindings.find(widgetId);
    if (it != bindings.end())
        return &it->second;
    return nullptr;
}

juce::ValueTree ParameterMapper::save()
{
    juce::ValueTree tree("paramMappings");
    for (const auto& [id, b] : bindings)
    {
        juce::ValueTree item("binding");
        item.setProperty("widgetId", id, nullptr);
        item.setProperty("type", static_cast<int>(b.type), nullptr);
        item.setProperty("oscAddress", b.oscAddress, nullptr);
        item.setProperty("currentValue", b.currentValue, nullptr);
        item.setProperty("min", b.min, nullptr);
        item.setProperty("max", b.max, nullptr);
        item.setProperty("defaultValue", b.defaultValue, nullptr);
        tree.appendChild(item, nullptr);
    }
    return tree;
}

void ParameterMapper::load(const juce::ValueTree& tree)
{
    bindings.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto item = tree.getChild(i);
        if (!item.hasType("binding")) continue;

        juce::String widgetId = item.getProperty("widgetId", "").toString();
        if (widgetId.isEmpty()) continue;

        Binding b;
        b.widgetId      = widgetId;
        b.type          = static_cast<TargetType>(static_cast<int>(item.getProperty("type", 0)));
        b.oscAddress    = item.getProperty("oscAddress", "").toString();
        b.currentValue  = item.getProperty("currentValue", 0.0f);
        b.min           = item.getProperty("min", 0.0f);
        b.max           = item.getProperty("max", 1.0f);
        b.defaultValue  = item.getProperty("defaultValue", 0.0f);
        bindings[widgetId] = b;
    }
}
