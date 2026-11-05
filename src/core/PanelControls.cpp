#include "PanelControls.h"
#include <algorithm>
#include <cmath>

namespace fourpole::panel
{
namespace
{
    // (reading through the mutable accessor is safe: get() never writes)
    Control knob (const juce::String& id, const juce::String& name, float lo, float hi, std::function<float& (Patch&)> field)
    {
        return { id, name, Kind::Knob, lo, hi, [field] (const Patch& p) { return field (const_cast<Patch&> (p)); },
                 [field, lo, hi] (Patch& p, float v) { field (p) = std::clamp (v, lo, hi); } };
    }

    Control selector (const juce::String& id, const juce::String& name, std::function<int& (Patch&)> field)
    {
        return { id, name, Kind::Selector, 0.0f, 5.0f, [field] (const Patch& p) { return (float) field (const_cast<Patch&> (p)); },
                 [field] (Patch& p, float v) { field (p) = std::clamp ((int) std::lround (v), 0, 5); } };
    }

    Control toggle (const juce::String& id, const juce::String& name, std::function<bool& (Patch&)> field)
    {
        return { id, name, Kind::Switch, 0.0f, 1.0f, [field] (const Patch& p) { return field (const_cast<Patch&> (p)) ? 1.0f : 0.0f; },
                 [field] (Patch& p, float v) { field (p) = v >= 0.5f; } };
    }

    std::vector<Control> build()
    {
        std::vector<Control> c;
        // ---- controllers
        c.push_back (knob ("tune", "TUNE", -2.5f, 2.5f, [] (Patch& p) -> float& { return p.tune; }));
        c.push_back (knob ("glide", "GLIDE", 0, 10, [] (Patch& p) -> float& { return p.glide; }));
        c.push_back (knob ("modMix", "MODULATION MIX", 0, 10, [] (Patch& p) -> float& { return p.modMix; }));
        c.push_back (toggle ("oscModulation", "OSC. MODULATION", [] (Patch& p) -> bool& { return p.oscModulation; }));
        c.push_back (toggle ("osc3Control", "OSC. 3 CONTROL", [] (Patch& p) -> bool& { return p.osc3Control; }));

        // ---- oscillator bank (oscillator 1 has no FREQUENCY knob: it's the reference)
        for (int i = 0; i < 3; ++i)
        {
            const auto n = juce::String (i + 1);
            c.push_back (selector ("osc" + n + ".range", "OSC " + n + " RANGE", [i] (Patch& p) -> int& { return p.osc[(size_t) i].range; }));
            if (i > 0)
                c.push_back (knob ("osc" + n + ".frequency", "OSC " + n + " FREQUENCY", -7, 7,
                                   [i] (Patch& p) -> float& { return p.osc[(size_t) i].frequency; }));
            c.push_back (selector ("osc" + n + ".wave", "OSC " + n + " WAVEFORM", [i] (Patch& p) -> int& { return p.osc[(size_t) i].wave; }));
        }

        // ---- mixer
        for (int i = 0; i < 3; ++i)
        {
            const auto n = juce::String (i + 1);
            c.push_back (knob ("osc" + n + ".volume", "MIXER OSC " + n + " VOLUME", 0, 10, [i] (Patch& p) -> float& { return p.oscVolume[(size_t) i]; }));
            c.push_back (toggle ("osc" + n + ".on", "MIXER OSC " + n + " ON/OFF", [i] (Patch& p) -> bool& { return p.oscOn[(size_t) i]; }));
        }
        c.push_back (knob ("extVolume", "EXTERNAL INPUT VOLUME", 0, 10, [] (Patch& p) -> float& { return p.extVolume; }));
        c.push_back (toggle ("extOn", "EXTERNAL INPUT ON/OFF", [] (Patch& p) -> bool& { return p.extOn; }));
        c.push_back (knob ("noiseVolume", "NOISE VOLUME", 0, 10, [] (Patch& p) -> float& { return p.noiseVolume; }));
        c.push_back (toggle ("noiseOn", "NOISE ON/OFF", [] (Patch& p) -> bool& { return p.noiseOn; }));
        c.push_back (toggle ("pinkNoise", "NOISE PINK/WHITE", [] (Patch& p) -> bool& { return p.pinkNoise; }));

        // ---- modifiers
        c.push_back (knob ("cutoff", "CUTOFF FREQUENCY", -5, 5, [] (Patch& p) -> float& { return p.cutoff; }));
        c.push_back (knob ("emphasis", "EMPHASIS", 0, 10, [] (Patch& p) -> float& { return p.emphasis; }));
        c.push_back (knob ("contourAmount", "AMOUNT OF CONTOUR", 0, 10, [] (Patch& p) -> float& { return p.contourAmount; }));
        c.push_back (toggle ("filterModulation", "FILTER MODULATION", [] (Patch& p) -> bool& { return p.filterModulation; }));
        c.push_back (toggle ("keyboardControl1", "KEYBOARD CONTROL 1", [] (Patch& p) -> bool& { return p.keyboardControl1; }));
        c.push_back (toggle ("keyboardControl2", "KEYBOARD CONTROL 2", [] (Patch& p) -> bool& { return p.keyboardControl2; }));
        c.push_back (knob ("filterAttack", "FILTER ATTACK TIME", 0, 10, [] (Patch& p) -> float& { return p.filterAttack; }));
        c.push_back (knob ("filterDecay", "FILTER DECAY TIME", 0, 10, [] (Patch& p) -> float& { return p.filterDecay; }));
        c.push_back (knob ("filterSustain", "FILTER SUSTAIN LEVEL", 0, 10, [] (Patch& p) -> float& { return p.filterSustain; }));
        c.push_back (knob ("loudAttack", "LOUDNESS ATTACK TIME", 0, 10, [] (Patch& p) -> float& { return p.loudAttack; }));
        c.push_back (knob ("loudDecay", "LOUDNESS DECAY TIME", 0, 10, [] (Patch& p) -> float& { return p.loudDecay; }));
        c.push_back (knob ("loudSustain", "LOUDNESS SUSTAIN LEVEL", 0, 10, [] (Patch& p) -> float& { return p.loudSustain; }));

        // ---- output
        c.push_back (knob ("volume", "VOLUME", 0, 10, [] (Patch& p) -> float& { return p.volume; }));
        c.push_back (toggle ("outputOn", "MAIN OUTPUT", [] (Patch& p) -> bool& { return p.outputOn; }));
        c.push_back (toggle ("a440", "A-440", [] (Patch& p) -> bool& { return p.a440; }));

        // ---- keyboard bed
        c.push_back (toggle ("glideOn", "GLIDE ON/OFF", [] (Patch& p) -> bool& { return p.glideOn; }));
        c.push_back (toggle ("decayOn", "DECAY ON/OFF", [] (Patch& p) -> bool& { return p.decayOn; }));
        return c;
    }
} // namespace

const std::vector<Control>& controls()
{
    static const std::vector<Control> all = build();
    return all;
}

const Control* find (const juce::String& id)
{
    for (auto& c : controls())
        if (c.id == id)
            return &c;
    return nullptr;
}
} // namespace fourpole::panel
