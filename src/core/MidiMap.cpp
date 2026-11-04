#include "core/MidiMap.h"

namespace fourpole
{
void MidiMap::resetToDefaults()
{
    map = { { 70, "contourAmount" }, { 71, "emphasis" },     { 72, "loudDecay" }, { 73, "loudAttack" },
            { 74, "cutoff" },        { 75, "filterDecay" },  { 76, "glide" },     { 77, "osc2.frequency" },
            { 7, "volume" } }; // one CC per control, so the map survives a save / load unchanged
}

void MidiMap::assign (int cc, const juce::String& id)
{
    if (cc < 0 || cc > 127 || isReserved (cc))
        return;
    forgetControl (id); // one CC per control
    map[cc] = id;
}

void MidiMap::forgetControl (const juce::String& id)
{
    for (auto it = map.begin(); it != map.end();)
        it = it->second == id ? map.erase (it) : std::next (it);
}

juce::String MidiMap::controlFor (int cc) const
{
    auto it = map.find (cc);
    return it != map.end() ? it->second : juce::String();
}

int MidiMap::ccFor (const juce::String& id) const
{
    for (auto& [cc, c] : map)
        if (c == id)
            return cc;
    return -1;
}

juce::var MidiMap::toJson() const
{
    auto o = juce::var (new juce::DynamicObject());
    for (auto& [cc, id] : map)
        o.getDynamicObject()->setProperty (juce::String (cc), id);
    return o;
}

void MidiMap::fromJson (const juce::var& v)
{
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return;
    map.clear();
    for (auto& p : o->getProperties())
        assign (p.name.toString().getIntValue(), p.value.toString());
}
} // namespace fourpole
