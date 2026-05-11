#include "AgentUser.h"

AgentUser::AgentUser()
    : name(""), pronouns(""), timezone("UTC"), contextNotes("")
{
}

bool AgentUser::isComplete() const
{
    return name.isNotEmpty();
}

juce::String AgentUser::toContextString() const
{
    juce::String ctx;
    ctx += "=== USER ===\n";
    if (name.isNotEmpty())
        ctx += "Name: " + name + "\n";
    if (pronouns.isNotEmpty())
        ctx += "Pronouns: " + pronouns + "\n";
    if (timezone.isNotEmpty())
        ctx += "Timezone: " + timezone + "\n";
    if (contextNotes.isNotEmpty())
        ctx += "Context:\n" + contextNotes + "\n";
    ctx += "\n";
    return ctx;
}

juce::ValueTree AgentUser::save()
{
    auto tree = juce::ValueTree("agentUser");
    tree.setProperty("name", name, nullptr);
    tree.setProperty("pronouns", pronouns, nullptr);
    tree.setProperty("timezone", timezone, nullptr);
    tree.setProperty("contextNotes", contextNotes, nullptr);
    return tree;
}

void AgentUser::load(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("agentUser")) return;
    name = tree.getProperty("name", "");
    pronouns = tree.getProperty("pronouns", "");
    timezone = tree.getProperty("timezone", "UTC");
    contextNotes = tree.getProperty("contextNotes", "");
}
