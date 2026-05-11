#include "AgentIdentity.h"

AgentIdentity::AgentIdentity()
    : name("WhyCremisi"), creature("AI Mixing Engineer"),
      vibe("analytical"), emoji("🎛️"), avatarUrl("")
{
}

bool AgentIdentity::isComplete() const
{
    return name.isNotEmpty() && creature.isNotEmpty() && vibe.isNotEmpty();
}

juce::String AgentIdentity::toContextString() const
{
    juce::String ctx;
    ctx += "=== IDENTITY ===\n";
    ctx += "Name: " + name + "\n";
    ctx += "Creature: " + creature + "\n";
    ctx += "Vibe: " + vibe + "\n";
    if (emoji.isNotEmpty())
        ctx += "Emoji: " + emoji + "\n";
    if (avatarUrl.isNotEmpty())
        ctx += "Avatar: " + avatarUrl + "\n";
    ctx += "\n";
    return ctx;
}

juce::ValueTree AgentIdentity::save()
{
    auto tree = juce::ValueTree("agentIdentity");
    tree.setProperty("name", name, nullptr);
    tree.setProperty("creature", creature, nullptr);
    tree.setProperty("vibe", vibe, nullptr);
    tree.setProperty("emoji", emoji, nullptr);
    tree.setProperty("avatarUrl", avatarUrl, nullptr);
    return tree;
}

void AgentIdentity::load(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("agentIdentity")) return;
    name = tree.getProperty("name", "WhyCremisi");
    creature = tree.getProperty("creature", "AI Mixing Engineer");
    vibe = tree.getProperty("vibe", "analytical");
    emoji = tree.getProperty("emoji", "🎛️");
    avatarUrl = tree.getProperty("avatarUrl", "");
}
