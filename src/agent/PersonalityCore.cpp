#include "PersonalityCore.h"
#include <algorithm>

PersonalityCore::PersonalityCore()
    : sessionCount(0), totalActions(0), sessionStartTime(0)
{
}

void PersonalityCore::recordAction(const juce::String& widgetId, float value, float previousValue,
                                    const juce::String& description)
{
    MixAction action;
    action.widgetId = widgetId;
    action.value = value;
    action.previousValue = previousValue;
    action.description = description.isNotEmpty() ? description : widgetId;
    action.timestamp = juce::Time::currentTimeMillis();
    action.reverted = false;
    actionHistory.push_back(action);
    totalActions++;

    updatePreference(widgetId, juce::String(value, 3));
    trimActions();

    if (onPersonalityEvent)
        onPersonalityEvent("action", description.isNotEmpty() ? description : widgetId);
}

void PersonalityCore::recordFeedback(const juce::String& actionId, bool positive)
{
    for (auto& action : actionHistory)
    {
        if (action.description == actionId && !action.reverted)
        {
            auto& pref = preferences[actionId + "_feedback"];
            pref.count++;
            pref.weight = positive ? juce::jmin(1.0f, pref.weight + 0.1f)
                                   : juce::jmax(0.0f, pref.weight - 0.15f);
            break;
        }
    }
}

void PersonalityCore::recordSessionEvent(const juce::String& category, const juce::String& detail)
{
    sessionEvents.push_back(category + ": " + detail);
    if (sessionEvents.size() > 100)
        sessionEvents.erase(sessionEvents.begin());
}

juce::String PersonalityCore::buildPersonalityContext() const
{
    juce::String ctx;

    if (userName.isNotEmpty())
        ctx += "User: " + userName + "\n";

    ctx += "Session: " + juce::String(sessionCount) + "\n";
    ctx += "Total actions taken: " + juce::String(totalActions) + "\n";

    if (totalActions > 0)
    {
        ctx += "Experience: " + juce::String(totalActions > 50 ? "advanced" :
                                               totalActions > 20 ? "intermediate" : "beginner") + "\n";

        std::map<juce::String, int> categoryCounts;
        for (const auto& a : actionHistory)
        {
            juce::String cat = a.widgetId.contains("gain") ? "gain" :
                               a.widgetId.contains("eq") ? "eq" :
                               a.widgetId.contains("comp") ? "compression" :
                               a.widgetId.contains("pan") ? "pan" : "other";
            categoryCounts[cat]++;
        }

        juce::String mostUsed = "gain";
        int maxCount = 0;
        for (const auto& [cat, count] : categoryCounts)
        {
            if (count > maxCount) { maxCount = count; mostUsed = cat; }
        }
        ctx += "Preferred domain: " + mostUsed + "\n";
    }

    if (!sessionEvents.empty())
    {
        auto last = sessionEvents.back();
        ctx += "Last event: " + last + "\n";
    }

    return ctx;
}

juce::String PersonalityCore::getPreferredStyle() const
{
    if (preferences.empty())
        return "warm";

    float gainCount = 0, eqCount = 0, compCount = 0;
    for (const auto& [key, pref] : preferences)
    {
        if (key.contains("gain")) gainCount += pref.weight;
        else if (key.contains("eq")) eqCount += pref.weight;
        else if (key.contains("comp")) compCount += pref.weight;
    }

    if (eqCount > gainCount && eqCount > compCount) return "analytical";
    if (compCount > gainCount && compCount > eqCount) return "direct";
    if (gainCount > compCount && gainCount > eqCount) return "consultative";
    return "warm";
}

juce::StringArray PersonalityCore::getRecentActions(int count) const
{
    juce::StringArray recent;
    int start = juce::jmax(0, (int)actionHistory.size() - count);
    for (int i = start; i < (int)actionHistory.size(); ++i)
        recent.add(actionHistory[i].description);
    return recent;
}

void PersonalityCore::setUserName(const juce::String& name)
{
    userName = name;
    if (onPersonalityEvent)
        onPersonalityEvent("identity", "User identified as: " + name);
}

juce::ValueTree PersonalityCore::save()
{
    auto tree = juce::ValueTree("personalityCore");
    tree.setProperty("userName", userName, nullptr);
    tree.setProperty("sessionCount", sessionCount, nullptr);
    tree.setProperty("totalActions", totalActions, nullptr);

    auto prefsTree = juce::ValueTree("preferences");
    for (const auto& [key, pref] : preferences)
    {
        auto p = juce::ValueTree("pref");
        p.setProperty("key", key, nullptr);
        p.setProperty("weight", pref.weight, nullptr);
        p.setProperty("count", pref.count, nullptr);
        p.setProperty("lastValue", pref.lastValue, nullptr);
        prefsTree.appendChild(p, nullptr);
    }
    tree.appendChild(prefsTree, nullptr);

    return tree;
}

void PersonalityCore::load(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("personalityCore")) return;

    userName = tree.getProperty("userName", "");
    sessionCount = tree.getProperty("sessionCount", 0);
    totalActions = tree.getProperty("totalActions", 0);

    auto prefsTree = tree.getChildWithName("preferences");
    if (prefsTree.isValid())
    {
        preferences.clear();
        for (int i = 0; i < prefsTree.getNumChildren(); ++i)
        {
            auto p = prefsTree.getChild(i);
            Preference pref;
            pref.weight = p.getProperty("weight", 0.0f);
            pref.count = p.getProperty("count", 0);
            pref.lastValue = p.getProperty("lastValue", "");
            preferences[p.getProperty("key", "").toString()] = pref;
        }
    }
}

void PersonalityCore::startSession()
{
    sessionCount++;
    sessionStartTime = juce::Time::currentTimeMillis();
    sessionEvents.clear();

    if (onPersonalityEvent)
        onPersonalityEvent("session", "Session " + juce::String(sessionCount) + " started");
}

void PersonalityCore::endSession()
{
    if (onPersonalityEvent)
    {
        auto duration = (juce::Time::currentTimeMillis() - sessionStartTime) / 1000;
        onPersonalityEvent("session_end",
            "Session " + juce::String(sessionCount) + " ended. Duration: " +
            juce::String(duration) + "s. Actions: " + juce::String(totalActions));
    }
}

void PersonalityCore::updatePreference(const juce::String& key, const juce::String& value)
{
    auto& pref = preferences[key];
    pref.count++;
    pref.weight = juce::jmin(1.0f, pref.weight + 0.05f);
    pref.lastValue = value;
}

void PersonalityCore::trimActions()
{
    while (actionHistory.size() > 500)
        actionHistory.erase(actionHistory.begin());
}

juce::String PersonalityContext::toContextString() const
{
    juce::String ctx;
    ctx += "Style: " + style + "\n";
    ctx += "Confidence: " + juce::String(confidence * 100, 0) + "%\n";
    ctx += "Experience: " + juce::String(experienceLevel) + "\n";
    if (!strengths.isEmpty())
        ctx += "Strengths: " + strengths.joinIntoString(", ") + "\n";
    if (description.isNotEmpty())
        ctx += "Approach: " + description + "\n";
    return ctx;
}
