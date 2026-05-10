#include "MidiHandler.h"

void MidiHandler::setMapping(const juce::String& widgetId, int cc, int channel,
                              float minVal, float maxVal)
{
    juce::ScopedLock sl(lock);
    auto& m = mappings[widgetId];
    m.ccNumber = cc;
    m.channel  = channel;
    m.minValue = minVal;
    m.maxValue = maxVal;
}

void MidiHandler::removeMapping(const juce::String& widgetId)
{
    juce::ScopedLock sl(lock);
    mappings.erase(widgetId);
}

MidiHandler::MidiMapping* MidiHandler::getMapping(const juce::String& widgetId)
{
    juce::ScopedLock sl(lock);
    auto it = mappings.find(widgetId);
    if (it != mappings.end())
        return &it->second;
    return nullptr;
}

void MidiHandler::sendCC(const juce::String& widgetId, float normalizedValue)
{
    juce::ScopedLock sl(lock);
    auto it = mappings.find(widgetId);
    if (it == mappings.end())
        return;

    const auto& m = it->second;
    int ccVal = static_cast<int>(m.minValue + normalizedValue * (m.maxValue - m.minValue));
    ccVal = juce::jlimit(0, 127, ccVal);

    auto msg = juce::MidiMessage::controllerEvent(m.channel + 1, m.ccNumber, ccVal);
    pendingCC.push_back(msg);
}

void MidiHandler::flush(juce::MidiBuffer& buffer, int sampleOffset)
{
    juce::ScopedLock sl(lock);
    for (const auto& msg : pendingCC)
        buffer.addEvent(msg, sampleOffset);
    pendingCC.clear();
}

// ── MIDI Learn ─────────────────────────────────────────────────

void MidiHandler::startLearn(const juce::String& widgetId)
{
    juce::ScopedLock sl(lock);
    learningWidget = widgetId;
}

void MidiHandler::stopLearn()
{
    juce::ScopedLock sl(lock);
    learningWidget.clear();
}

void MidiHandler::onMidiInput(const juce::MidiMessage& msg)
{
    if (!msg.isController())
        return;

    juce::ScopedLock sl(lock);
    if (learningWidget.isEmpty())
        return;

    int cc = msg.getControllerNumber();
    int channel = msg.getChannel() - 1;
    juce::String widgetId = learningWidget;

    auto& m = mappings[widgetId];
    m.ccNumber = cc;
    m.channel = channel;
    m.minValue = 0.0f;
    m.maxValue = 1.0f;

    learningWidget.clear();

    if (learnCompleteCallback)
        learnCompleteCallback(widgetId, cc, channel);
}

// ── Persistence ────────────────────────────────────────────────

juce::ValueTree MidiHandler::save()
{
    juce::ValueTree tree("midiMappings");
    for (const auto& [id, m] : mappings)
    {
        juce::ValueTree item("mapping");
        item.setProperty("widgetId", id, nullptr);
        item.setProperty("cc", m.ccNumber, nullptr);
        item.setProperty("channel", m.channel, nullptr);
        item.setProperty("min", m.minValue, nullptr);
        item.setProperty("max", m.maxValue, nullptr);
        tree.appendChild(item, nullptr);
    }
    return tree;
}

void MidiHandler::load(const juce::ValueTree& tree)
{
    mappings.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto item = tree.getChild(i);
        if (!item.hasType("mapping")) continue;

        juce::String widgetId = item.getProperty("widgetId", "").toString();
        if (widgetId.isEmpty()) continue;

        auto& m = mappings[widgetId];
        m.ccNumber = item.getProperty("cc", -1);
        m.channel  = item.getProperty("channel", 0);
        m.minValue = item.getProperty("min", 0.0f);
        m.maxValue = item.getProperty("max", 1.0f);
    }
}
