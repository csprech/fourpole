#pragma once

// MIDI CC -> panel control assignments (MIDI learn), with defaults that suit typical controllers.
// Controls are named by stable IDs ("cutoff", "osc2.frequency", "filterModulation", ...).

#include <juce_core/juce_core.h>
#include <map>

namespace fourpole
{
class MidiMap
{
public:
    MidiMap() { resetToDefaults(); }

    /** Default knobs: an 8-knob controller sending CC 70-77 (e.g. MPK Mini K1-K8), which also lines up
        with the GM sound controllers (71 resonance, 72/73 release/attack, 74 cutoff, 75 decay), plus CC 7 volume. */
    void resetToDefaults();
    void assign (int cc, const juce::String& controlId);
    void forgetControl (const juce::String& controlId);
    juce::String controlFor (int cc) const;
    /** CC assigned to a control, or -1. */
    int ccFor (const juce::String& controlId) const;
    const std::map<int, juce::String>& all() const { return map; }

    juce::var toJson() const;
    void fromJson (const juce::var& v);

    /** CCs the synth handles itself: never assignable. */
    static bool isReserved (int cc) { return cc == 1 || cc == 64 || cc == 120 || cc == 121 || cc == 123; }

private:
    std::map<int, juce::String> map;
};
} // namespace fourpole
