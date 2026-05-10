#include "PluginChain.h"

// ── PluginInfo ─────────────────────────────────────────────────

juce::ValueTree PluginInfo::save() const
{
    juce::ValueTree tree("plugin");
    tree.setProperty("id", id, nullptr);
    tree.setProperty("name", name, nullptr);
    tree.setProperty("manufacturer", manufacturer, nullptr);
    tree.setProperty("format", format, nullptr);
    tree.setProperty("slot", slot, nullptr);
    tree.setProperty("enabled", enabled, nullptr);
    return tree;
}

PluginInfo PluginInfo::load(const juce::ValueTree& tree)
{
    PluginInfo p;
    p.id           = tree.getProperty("id", "").toString();
    p.name         = tree.getProperty("name", "").toString();
    p.manufacturer = tree.getProperty("manufacturer", "").toString();
    p.format       = tree.getProperty("format", "VST3").toString();
    p.slot         = tree.getProperty("slot", 0);
    p.enabled      = tree.getProperty("enabled", true);
    return p;
}

// ── PluginChain ────────────────────────────────────────────────

void PluginChain::setPlugins(const std::vector<PluginInfo>& newPlugins)
{
    juce::ScopedLock sl(lock);
    plugins = newPlugins;
}

void PluginChain::addPlugin(const PluginInfo& plugin)
{
    juce::ScopedLock sl(lock);
    plugins.push_back(plugin);
}

void PluginChain::removePlugin(const juce::String& id)
{
    juce::ScopedLock sl(lock);
    plugins.erase(std::remove_if(plugins.begin(), plugins.end(),
        [&](const PluginInfo& p) { return p.id == id; }), plugins.end());
}

void PluginChain::updatePlugin(const PluginInfo& plugin)
{
    juce::ScopedLock sl(lock);
    for (auto& p : plugins)
    {
        if (p.id == plugin.id)
        {
            p = plugin;
            return;
        }
    }
}

void PluginChain::clear()
{
    juce::ScopedLock sl(lock);
    plugins.clear();
}

PluginInfo* PluginChain::getPlugin(const juce::String& id)
{
    juce::ScopedLock sl(lock);
    for (auto& p : plugins)
    {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

juce::ValueTree PluginChain::save() const
{
    juce::ValueTree tree("pluginChain");
    for (const auto& p : plugins)
        tree.appendChild(p.save(), nullptr);
    return tree;
}

void PluginChain::load(const juce::ValueTree& tree)
{
    plugins.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        if (child.hasType("plugin"))
            plugins.push_back(PluginInfo::load(child));
    }

    // Sort by slot
    std::sort(plugins.begin(), plugins.end(),
        [](const PluginInfo& a, const PluginInfo& b) { return a.slot < b.slot; });
}

juce::String PluginChain::toDescriptiveString() const
{
    juce::String desc;
    for (const auto& p : plugins)
    {
        if (!p.enabled) continue;
        if (desc.isNotEmpty()) desc += " → ";
        desc += p.name;
        if (p.manufacturer.isNotEmpty())
            desc += " (" + p.manufacturer + ")";
    }
    if (desc.isEmpty())
        desc = "(no plugins configured)";
    return desc;
}
