#pragma once

// Every knob, selector and switch on the panel, by stable ID: its printed name, its range in panel
// units, and the Patch field it sets. The UI binds its widgets through this table, MIDI learn stores
// these IDs, and the tests walk it to prove that each control reaches the sound.

#include "core/Patch.h"
#include <functional>
#include <vector>

namespace fourpole::panel
{
enum class Kind { Knob, Selector, Switch };

struct Control
{
    juce::String id;   // "cutoff", "osc2.frequency", "filterModulation", ...
    juce::String name; // unambiguous, for menus: "FILTER ATTACK TIME", "OSC 2 FREQUENCY"
    Kind kind = Kind::Knob;
    float min = 0.0f, max = 1.0f; // switches: 0 = off, 1 = on; selectors: positions 0..5
    std::function<float (const Patch&)> get;
    std::function<void (Patch&, float)> set; // clamps to min..max (selectors round, switches threshold at 0.5)
    bool isSwitch() const { return kind == Kind::Switch; }
};

/** All of them, in panel order (controllers, oscillators, mixer, modifiers, output, keyboard bed). */
const std::vector<Control>& controls();
/** nullptr if there's no control with that ID. */
const Control* find (const juce::String& id);
} // namespace fourpole::panel
