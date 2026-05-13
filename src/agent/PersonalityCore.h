#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>
#include <vector>
#include <functional>

class PersonalityCore
{
public:
    struct MixAction
    {
        juce::String widgetId;
        float value;
        float previousValue;
        juce::String description;
        int64_t timestamp;
        bool reverted;
    };

    struct Preference
    {
        float weight;
        int count;
        juce::String lastValue;
    };

    PersonalityCore();

    void recordAction(const juce::String& widgetId, float value, float previousValue,
                      const juce::String& description = {});
    void recordFeedback(const juce::String& actionId, bool positive);
    void recordSessionEvent(const juce::String& category, const juce::String& detail);

    juce::String buildPersonalityContext() const;
    juce::String getPreferredStyle() const;
    juce::StringArray getRecentActions(int count = 5) const;

    void setUserName(const juce::String& name);
    juce::String getUserName() const { return userName; }

    int getSessionCount() const { return sessionCount; }
    int getTotalActions() const { return totalActions; }

    juce::ValueTree save();
    void load(const juce::ValueTree& tree);

    void startSession();
    void endSession();

    std::function<void(const juce::String&, const juce::String&)> onPersonalityEvent;

private:
    juce::String userName;
    int sessionCount;
    int totalActions;
    int64_t sessionStartTime;

    std::vector<MixAction> actionHistory;
    std::map<juce::String, Preference> preferences;
    std::vector<juce::String> sessionEvents;

    void updatePreference(const juce::String& key, const juce::String& value);
    void trimActions();
};

class PersonalityContext
{
public:
    juce::String style;        
    float confidence;          
    int experienceLevel;       
    juce::StringArray strengths; 
    juce::String description;   

    juce::String toContextString() const;
};
