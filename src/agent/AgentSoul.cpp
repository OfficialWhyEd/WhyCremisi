#include "AgentSoul.h"

AgentSoul::AgentSoul()
    : style(Analytical), vibeDescription("analytical, focused, precise")
{
    coreTruths = {
        "Be genuinely helpful, not performatively helpful",
        "Have opinions and share them constructively",
        "Be resourceful before asking — read, check, search first",
        "Earn trust through competence and reliability",
        "Remember you are a guest in the user's creative process"
    };
    boundaries = {
        "Private session data stays private — never shared across sessions",
        "When in doubt, ask before taking irreversible actions",
        "Never send half-baked suggestions — only well-considered advice",
        "You guide the mix, but the artist makes the final call"
    };
}

AgentSoul::Style AgentSoul::styleFromString(const juce::String& s)
{
    if (s == "analytical") return Analytical;
    if (s == "consultative") return Consultative;
    if (s == "direct") return Direct;
    if (s == "creative") return Creative;
    if (s == "warm") return Warm;
    return Analytical;
}

juce::String AgentSoul::styleToString(Style s)
{
    switch (s)
    {
        case Analytical: return "analytical";
        case Consultative: return "consultative";
        case Direct: return "direct";
        case Creative: return "creative";
        case Warm: return "warm";
        default: return "analytical";
    }
}

void AgentSoul::addCoreTruth(const juce::String& truth)
{
    coreTruths.push_back(truth);
}

void AgentSoul::addBoundary(const juce::String& boundary)
{
    boundaries.push_back(boundary);
}

bool AgentSoul::isComplete() const
{
    return !coreTruths.empty() && vibeDescription.isNotEmpty();
}

juce::String AgentSoul::toContextString() const
{
    juce::String ctx;
    ctx += "=== SOUL ===\n";
    ctx += "Style: " + styleToString(style) + "\n";
    ctx += "Vibe: " + vibeDescription + "\n\n";

    ctx += "Core Truths:\n";
    for (const auto& t : coreTruths)
        ctx += "- " + t + "\n";
    ctx += "\n";

    ctx += "Boundaries:\n";
    for (const auto& b : boundaries)
        ctx += "- " + b + "\n";
    ctx += "\n";

    return ctx;
}

juce::ValueTree AgentSoul::save()
{
    auto tree = juce::ValueTree("agentSoul");
    tree.setProperty("style", styleToString(style), nullptr);
    tree.setProperty("vibeDescription", vibeDescription, nullptr);

    auto truthsTree = juce::ValueTree("coreTruths");
    for (const auto& t : coreTruths)
    {
        auto item = juce::ValueTree("truth");
        item.setProperty("text", t, nullptr);
        truthsTree.appendChild(item, nullptr);
    }
    tree.appendChild(truthsTree, nullptr);

    auto boundariesTree = juce::ValueTree("boundaries");
    for (const auto& b : boundaries)
    {
        auto item = juce::ValueTree("boundary");
        item.setProperty("text", b, nullptr);
        boundariesTree.appendChild(item, nullptr);
    }
    tree.appendChild(boundariesTree, nullptr);

    return tree;
}

void AgentSoul::load(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("agentSoul")) return;
    style = styleFromString(tree.getProperty("style", "analytical").toString());
    vibeDescription = tree.getProperty("vibeDescription", "analytical, focused, precise");

    coreTruths.clear();
    auto truthsTree = tree.getChildWithName("coreTruths");
    if (truthsTree.isValid())
    {
        for (int i = 0; i < truthsTree.getNumChildren(); ++i)
            coreTruths.push_back(truthsTree.getChild(i).getProperty("text", ""));
    }

    boundaries.clear();
    auto boundariesTree = tree.getChildWithName("boundaries");
    if (boundariesTree.isValid())
    {
        for (int i = 0; i < boundariesTree.getNumChildren(); ++i)
            boundaries.push_back(boundariesTree.getChild(i).getProperty("text", ""));
    }
}
