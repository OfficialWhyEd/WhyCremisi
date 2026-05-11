#include "AgentWorkspace.h"
#include "PersonalityCore.h"

// ── MemoryEntry ─────────────────────────────────────────────────────

juce::ValueTree MemoryEntry::save() const
{
    auto tree = juce::ValueTree("memoryEntry");
    tree.setProperty("date", date, nullptr);
    tree.setProperty("text", text, nullptr);
    tree.setProperty("category", category, nullptr);
    tree.setProperty("timestamp", timestamp, nullptr);
    return tree;
}

MemoryEntry MemoryEntry::load(const juce::ValueTree& tree)
{
    MemoryEntry e;
    e.date = tree.getProperty("date", "");
    e.text = tree.getProperty("text", "");
    e.category = tree.getProperty("category", "general");
    e.timestamp = tree.getProperty("timestamp", (int64_t)0);
    return e;
}

// ── HeartbeatState ──────────────────────────────────────────────────

juce::ValueTree HeartbeatState::save() const
{
    auto tree = juce::ValueTree("heartbeat");
    auto checks = juce::ValueTree("lastChecks");
    for (const auto& [name, ts] : lastCheckTimestamps)
    {
        auto c = juce::ValueTree("check");
        c.setProperty("name", name, nullptr);
        c.setProperty("timestamp", ts, nullptr);
        checks.appendChild(c, nullptr);
    }
    tree.appendChild(checks, nullptr);
    auto tasksTree = juce::ValueTree("tasks");
    for (const auto& t : tasks)
    {
        auto item = juce::ValueTree("task");
        item.setProperty("text", t, nullptr);
        tasksTree.appendChild(item, nullptr);
    }
    tree.appendChild(tasksTree, nullptr);
    return tree;
}

void HeartbeatState::load(const juce::ValueTree& tree)
{
    if (!tree.isValid()) return;
    lastCheckTimestamps.clear();
    auto checks = tree.getChildWithName("lastChecks");
    if (checks.isValid())
    {
        for (int i = 0; i < checks.getNumChildren(); ++i)
        {
            auto c = checks.getChild(i);
            lastCheckTimestamps[c.getProperty("name", "").toString()] =
                c.getProperty("timestamp", (int64_t)0);
        }
    }
    tasks.clear();
    auto tasksTree = tree.getChildWithName("tasks");
    if (tasksTree.isValid())
    {
        for (int i = 0; i < tasksTree.getNumChildren(); ++i)
            tasks.add(tasksTree.getChild(i).getProperty("text", "").toString());
    }
}

// ── AgentWorkspace ──────────────────────────────────────────────────

AgentWorkspace::AgentWorkspace()
    : redLinesEnabled(true), memoryEnabled(true), heartbeatEnabled(false),
      bootstrapComplete(false), bootEnabled(false), personalityCore(nullptr)
{
}

void AgentWorkspace::linkPersonalityCore(PersonalityCore* pc)
{
    personalityCore = pc;
}

void AgentWorkspace::markBootstrapComplete()
{
    bootstrapComplete = true;
    if (onWorkspaceEvent)
        onWorkspaceEvent("bootstrap", "Bootstrap complete — identity established");
}

void AgentWorkspace::setBootInstructions(const juce::String& instructions)
{
    bootInstructions = instructions;
    bootEnabled = instructions.isNotEmpty();
}

void AgentWorkspace::addMemoryEntry(const juce::String& text, const juce::String& category)
{
    MemoryEntry entry;
    entry.text = text;
    entry.category = category;
    {
        auto now = juce::Time::getCurrentTime();
        entry.date = juce::String(now.getYear()) + "-"
                   + juce::String(now.getMonth() + 1).paddedLeft('0', 2) + "-"
                   + juce::String(now.getDayOfMonth()).paddedLeft('0', 2);
    }
    entry.timestamp = juce::Time::currentTimeMillis();
    dailyMemory.push_back(entry);
    if (dailyMemory.size() > 1000)
        dailyMemory.erase(dailyMemory.begin());

    if (category == "long-term")
    {
        longTermMemory.push_back(entry);
        if (longTermMemory.size() > 200)
            longTermMemory.erase(longTermMemory.begin());
    }

    if (onWorkspaceEvent)
        onWorkspaceEvent("memory", "Memory added: " + category + " — " + text.substring(0, 80));
}

void AgentWorkspace::addToolNote(const juce::String& note)
{
    if (toolsNotes.isNotEmpty())
        toolsNotes += "\n";
    toolsNotes += "- " + note;
    if (onWorkspaceEvent)
        onWorkspaceEvent("tools", "Tool note added: " + note);
}

void AgentWorkspace::addRule(const juce::String& rule)
{
    extraRules.add(rule);
}

void AgentWorkspace::addHeartbeatTask(const juce::String& task)
{
    heartbeat.tasks.add(task);
    heartbeatEnabled = true;
}

void AgentWorkspace::touchHeartbeatCheck(const juce::String& checkName)
{
    heartbeat.lastCheckTimestamps[checkName] = juce::Time::currentTimeMillis();
}

bool AgentWorkspace::isHeartbeatStale(const juce::String& checkName, int64_t maxAgeMs) const
{
    auto it = heartbeat.lastCheckTimestamps.find(checkName);
    if (it == heartbeat.lastCheckTimestamps.end()) return true;
    return (juce::Time::currentTimeMillis() - it->second) > maxAgeMs;
}

juce::String AgentWorkspace::buildFullContext() const
{
    juce::String ctx;
    ctx += "=== WORKSPACE CONTEXT ===\n\n";

    ctx += identity.toContextString();
    ctx += soul.toContextString();
    ctx += user.toContextString();

    // AGENTS.md rules
    ctx += "=== AGENTS RULES ===\n";
    ctx += "Red lines enabled: " + juce::String(redLinesEnabled ? "yes" : "no") + "\n";
    ctx += "Memory enabled: " + juce::String(memoryEnabled ? "yes" : "no") + "\n";
    ctx += "Heartbeat enabled: " + juce::String(heartbeatEnabled ? "yes" : "no") + "\n";
    for (const auto& rule : extraRules)
        ctx += "- " + rule + "\n";
    ctx += "\n";

    ctx += buildBootstrapContext();
    ctx += buildHeartbeatContext();
    ctx += buildToolsContext();
    ctx += buildMemoryContext();

    // PersonalityCore integration
    if (personalityCore)
    {
        ctx += "=== PERSONALITY ===\n";
        ctx += personalityCore->buildPersonalityContext();
        ctx += "\n";
    }

    return ctx;
}

juce::String AgentWorkspace::buildMemoryContext() const
{
    juce::String ctx;
    ctx += "=== MEMORY ===\n";

    if (!longTermMemory.empty())
    {
        ctx += "Long-term memory:\n";
        int count = 0;
        for (const auto& m : longTermMemory)
        {
            ctx += "- [" + m.date + "] (" + m.category + ") " + m.text + "\n";
            if (++count >= 20) break;
        }
        ctx += "\n";
    }

    if (!dailyMemory.empty())
    {
        ctx += "Recent daily memory (last 10):\n";
        int start = juce::jmax(0, (int)dailyMemory.size() - 10);
        for (int i = start; i < (int)dailyMemory.size(); ++i)
        {
            const auto& m = dailyMemory[i];
            ctx += "- [" + m.date + "] " + m.text + "\n";
        }
        ctx += "\n";
    }

    return ctx;
}

juce::String AgentWorkspace::buildHeartbeatContext() const
{
    if (!heartbeatEnabled) return {};

    juce::String ctx;
    ctx += "=== HEARTBEAT ===\n";
    if (!heartbeat.tasks.isEmpty())
    {
        ctx += "Periodic tasks:\n";
        for (const auto& t : heartbeat.tasks)
            ctx += "- " + t + "\n";
        ctx += "\n";
    }
    if (!heartbeat.lastCheckTimestamps.empty())
    {
        ctx += "Last checks:\n";
        for (const auto& [name, ts] : heartbeat.lastCheckTimestamps)
        {
            auto diff = (juce::Time::currentTimeMillis() - ts) / 60000;
            ctx += "- " + name + ": " + juce::String(diff) + " min ago\n";
        }
        ctx += "\n";
    }
    return ctx;
}

juce::String AgentWorkspace::buildToolsContext() const
{
    if (toolsNotes.isEmpty()) return {};

    juce::String ctx;
    ctx += "=== TOOLS ===\n";
    ctx += toolsNotes + "\n\n";
    return ctx;
}

juce::String AgentWorkspace::buildBootstrapContext() const
{
    if (bootstrapComplete) return {};

    juce::String ctx;
    ctx += "=== BOOTSTRAP ===\n";
    ctx += "Bootstrap not yet complete.\n";
    ctx += "This is a fresh workspace — identity and user information need to be established.\n";
    ctx += "Refer to the SOUL and IDENTITY templates for guidance.\n\n";

    if (bootEnabled && bootInstructions.isNotEmpty())
    {
        ctx += "=== BOOT INSTRUCTIONS ===\n";
        ctx += bootInstructions + "\n\n";
    }

    return ctx;
}

void AgentWorkspace::resetToDefaults()
{
    identity = AgentIdentity();
    soul = AgentSoul();
    user = AgentUser();
    redLinesEnabled = true;
    memoryEnabled = true;
    heartbeatEnabled = false;
    bootstrapComplete = false;
    bootEnabled = false;
    bootInstructions.clear();
    extraRules.clear();
    toolsNotes.clear();
    dailyMemory.clear();
    longTermMemory.clear();
    heartbeat = HeartbeatState();
}

juce::ValueTree AgentWorkspace::save()
{
    auto tree = juce::ValueTree("agentWorkspace");

    tree.appendChild(identity.save(), nullptr);
    tree.appendChild(soul.save(), nullptr);
    tree.appendChild(user.save(), nullptr);

    tree.setProperty("redLinesEnabled", redLinesEnabled, nullptr);
    tree.setProperty("memoryEnabled", memoryEnabled, nullptr);
    tree.setProperty("heartbeatEnabled", heartbeatEnabled, nullptr);
    tree.setProperty("bootstrapComplete", bootstrapComplete, nullptr);
    tree.setProperty("bootEnabled", bootEnabled, nullptr);
    tree.setProperty("bootInstructions", bootInstructions, nullptr);

    auto rulesTree = juce::ValueTree("extraRules");
    for (const auto& r : extraRules)
    {
        auto item = juce::ValueTree("rule");
        item.setProperty("text", r, nullptr);
        rulesTree.appendChild(item, nullptr);
    }
    tree.appendChild(rulesTree, nullptr);

    tree.setProperty("toolsNotes", toolsNotes, nullptr);

    tree.appendChild(heartbeat.save(), nullptr);

    auto dailyTree = juce::ValueTree("dailyMemory");
    for (const auto& m : dailyMemory)
        dailyTree.appendChild(m.save(), nullptr);
    tree.appendChild(dailyTree, nullptr);

    auto ltTree = juce::ValueTree("longTermMemory");
    for (const auto& m : longTermMemory)
        ltTree.appendChild(m.save(), nullptr);
    tree.appendChild(ltTree, nullptr);

    return tree;
}

void AgentWorkspace::load(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("agentWorkspace")) return;

    auto idTree = tree.getChildWithName("agentIdentity");
    if (idTree.isValid()) identity.load(idTree);

    auto soulTree = tree.getChildWithName("agentSoul");
    if (soulTree.isValid()) soul.load(soulTree);

    auto userTree = tree.getChildWithName("agentUser");
    if (userTree.isValid()) user.load(userTree);

    redLinesEnabled = tree.getProperty("redLinesEnabled", true);
    memoryEnabled = tree.getProperty("memoryEnabled", true);
    heartbeatEnabled = tree.getProperty("heartbeatEnabled", false);
    bootstrapComplete = tree.getProperty("bootstrapComplete", false);
    bootEnabled = tree.getProperty("bootEnabled", false);
    bootInstructions = tree.getProperty("bootInstructions", "");

    extraRules.clear();
    auto rulesTree = tree.getChildWithName("extraRules");
    if (rulesTree.isValid())
    {
        for (int i = 0; i < rulesTree.getNumChildren(); ++i)
            extraRules.add(rulesTree.getChild(i).getProperty("text", "").toString());
    }

    toolsNotes = tree.getProperty("toolsNotes", "");

    auto hbTree = tree.getChildWithName("heartbeat");
    if (hbTree.isValid()) heartbeat.load(hbTree);

    dailyMemory.clear();
    auto dailyTree = tree.getChildWithName("dailyMemory");
    if (dailyTree.isValid())
    {
        for (int i = 0; i < dailyTree.getNumChildren(); ++i)
            dailyMemory.push_back(MemoryEntry::load(dailyTree.getChild(i)));
    }

    longTermMemory.clear();
    auto ltTree = tree.getChildWithName("longTermMemory");
    if (ltTree.isValid())
    {
        for (int i = 0; i < ltTree.getNumChildren(); ++i)
            longTermMemory.push_back(MemoryEntry::load(ltTree.getChild(i)));
    }
}
