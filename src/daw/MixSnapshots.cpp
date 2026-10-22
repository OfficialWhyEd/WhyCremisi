#include "MixSnapshots.h"
#include "AbletonDawHandler.h"
#include <cmath>

double MixSnapshots::dbFromLiveVolume (double v)
{
    return AbletonDawHandler::dbFromLiveVolume ((float) v);
}

juce::File MixSnapshots::getFile() const
{
    if (file != juce::File())
        return file;
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("WhyCremisi").getChildFile ("mix-snapshots.json");
}

bool MixSnapshots::save (const juce::String& name, const std::vector<MixTrackState>& tracks)
{
    if (name.trim().isEmpty() || tracks.empty())
        return false;

    auto root = juce::JSON::parse (getFile());
    auto* obj = root.getDynamicObject();
    juce::var base = obj != nullptr ? root : juce::var (new juce::DynamicObject());

    juce::Array<juce::var> righe;
    for (const auto& t : tracks)
    {
        auto* r = new juce::DynamicObject();
        r->setProperty ("index", t.index);
        r->setProperty ("volumeDb", t.volumeDb);
        r->setProperty ("pan", t.pan);
        r->setProperty ("mute", t.mute);
        righe.add (juce::var (r));
    }
    base.getDynamicObject()->setProperty (name.trim(), righe);

    auto f = getFile();
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (juce::JSON::toString (base));
}

bool MixSnapshots::recall (const juce::String& name, std::vector<MixTrackState>& out) const
{
    auto root = juce::JSON::parse (getFile());
    if (root.getDynamicObject() == nullptr)
        return false;
    const auto lista = root.getDynamicObject()->getProperty (name.trim());
    auto* arr = lista.getArray();
    if (arr == nullptr)
        return false;

    out.clear();
    for (const auto& r : *arr)
    {
        MixTrackState t;
        t.index    = (int) r.getProperty ("index", 0);
        t.volumeDb = (double) r.getProperty ("volumeDb", 0.0);
        t.pan      = (double) r.getProperty ("pan", 0.0);
        t.mute     = (bool) r.getProperty ("mute", false);
        out.push_back (t);
    }
    return ! out.empty();
}

juce::StringArray MixSnapshots::names() const
{
    juce::StringArray n;
    auto root = juce::JSON::parse (getFile());
    if (auto* o = root.getDynamicObject())
        for (const auto& p : o->getProperties())
            n.add (p.name.toString());
    return n;
}
