#include "core/Patch.h"
#include <functional>

namespace fourpole
{
namespace
{
    juce::var obj() { return juce::var (new juce::DynamicObject()); }
    void set (juce::var& o, const char* k, const juce::var& v) { o.getDynamicObject()->setProperty (k, v); }
    template <typename T>
    T get (const juce::var& o, const char* k, T def)
    {
        if (! o.hasProperty (k))
            return def;
        if constexpr (std::is_same_v<T, bool>)
            return (bool) o[k];
        else if constexpr (std::is_same_v<T, int>)
            return (int) o[k];
        else
            return (T) (double) o[k];
    }
}

juce::var Patch::toJson() const
{
    auto root = obj();
    set (root, "format", "fourpole-patch");
    set (root, "version", 1);
    set (root, "name", name);
    auto c = obj();
    set (c, "tune", tune);
    set (c, "glide", glide);
    set (c, "modMix", modMix);
    set (c, "oscModulation", oscModulation);
    set (c, "osc3Control", osc3Control);
    set (c, "glideOn", glideOn);
    set (c, "decayOn", decayOn);
    set (root, "controllers", c);
    juce::Array<juce::var> oscs;
    for (int i = 0; i < 3; ++i)
    {
        auto o = obj();
        set (o, "range", osc[(size_t) i].range);
        set (o, "frequency", osc[(size_t) i].frequency);
        set (o, "wave", osc[(size_t) i].wave);
        set (o, "volume", oscVolume[(size_t) i]);
        set (o, "on", oscOn[(size_t) i]);
        oscs.add (o);
    }
    set (root, "oscillators", oscs);
    auto m = obj();
    set (m, "extVolume", extVolume);
    set (m, "extOn", extOn);
    set (m, "noiseVolume", noiseVolume);
    set (m, "noiseOn", noiseOn);
    set (m, "pinkNoise", pinkNoise);
    set (root, "mixer", m);
    auto f = obj();
    set (f, "cutoff", cutoff);
    set (f, "emphasis", emphasis);
    set (f, "contourAmount", contourAmount);
    set (f, "keyboardControl1", keyboardControl1);
    set (f, "keyboardControl2", keyboardControl2);
    set (f, "modulation", filterModulation);
    set (f, "attack", filterAttack);
    set (f, "decay", filterDecay);
    set (f, "sustain", filterSustain);
    set (root, "filter", f);
    auto l = obj();
    set (l, "attack", loudAttack);
    set (l, "decay", loudDecay);
    set (l, "sustain", loudSustain);
    set (root, "loudness", l);
    auto out = obj();
    set (out, "volume", volume);
    set (out, "on", outputOn);
    set (out, "a440", a440);
    set (root, "output", out);
    auto x = obj();
    set (x, "bendRange", bendRange);
    set (x, "priority", (int) priority);
    set (x, "velocity", velocity);
    set (x, "drift", drift);
    set (root, "extras", x);
    return root;
}

Patch Patch::fromJson (const juce::var& v)
{
    Patch p;
    p.name = v.getProperty ("name", p.name).toString();
    const auto c = v["controllers"];
    p.tune = get (c, "tune", p.tune);
    p.glide = get (c, "glide", p.glide);
    p.modMix = get (c, "modMix", p.modMix);
    p.oscModulation = get (c, "oscModulation", p.oscModulation);
    p.osc3Control = get (c, "osc3Control", p.osc3Control);
    p.glideOn = get (c, "glideOn", p.glideOn);
    p.decayOn = get (c, "decayOn", p.decayOn);
    if (auto* oscs = v["oscillators"].getArray())
        for (int i = 0; i < std::min (3, oscs->size()); ++i)
        {
            const auto& o = oscs->getReference (i);
            auto& s = p.osc[(size_t) i];
            s.range = std::clamp (get (o, "range", s.range), 0, kNumRanges - 1);
            s.frequency = std::clamp (get (o, "frequency", s.frequency), -7.0f, 7.0f);
            s.wave = std::clamp (get (o, "wave", s.wave), 0, kNumWaves - 1);
            p.oscVolume[(size_t) i] = get (o, "volume", p.oscVolume[(size_t) i]);
            p.oscOn[(size_t) i] = get (o, "on", p.oscOn[(size_t) i]);
        }
    const auto m = v["mixer"];
    p.extVolume = get (m, "extVolume", p.extVolume);
    p.extOn = get (m, "extOn", p.extOn);
    p.noiseVolume = get (m, "noiseVolume", p.noiseVolume);
    p.noiseOn = get (m, "noiseOn", p.noiseOn);
    p.pinkNoise = get (m, "pinkNoise", p.pinkNoise);
    const auto f = v["filter"];
    p.cutoff = get (f, "cutoff", p.cutoff);
    p.emphasis = get (f, "emphasis", p.emphasis);
    p.contourAmount = get (f, "contourAmount", p.contourAmount);
    p.keyboardControl1 = get (f, "keyboardControl1", p.keyboardControl1);
    p.keyboardControl2 = get (f, "keyboardControl2", p.keyboardControl2);
    p.filterModulation = get (f, "modulation", p.filterModulation);
    p.filterAttack = get (f, "attack", p.filterAttack);
    p.filterDecay = get (f, "decay", p.filterDecay);
    p.filterSustain = get (f, "sustain", p.filterSustain);
    const auto l = v["loudness"];
    p.loudAttack = get (l, "attack", p.loudAttack);
    p.loudDecay = get (l, "decay", p.loudDecay);
    p.loudSustain = get (l, "sustain", p.loudSustain);
    const auto out = v["output"];
    p.volume = get (out, "volume", p.volume);
    p.outputOn = get (out, "on", p.outputOn);
    p.a440 = get (out, "a440", p.a440);
    const auto x = v["extras"];
    p.bendRange = std::clamp (get (x, "bendRange", p.bendRange), 0.0f, 12.0f);
    p.priority = (NotePriority) std::clamp (get (x, "priority", (int) p.priority), 0, 2);
    p.velocity = std::clamp (get (x, "velocity", p.velocity), 0.0f, 1.0f);
    p.drift = std::clamp (get (x, "drift", p.drift), 0.0f, 1.0f);
    return p;
}

// ------------------------------------------------------------------ factory patches

namespace
{
    Patch make (const char* name, std::function<void (Patch&)> fn)
    {
        Patch p;
        p.name = name;
        fn (p);
        return p;
    }
    using W = Wave;
    using R = Range;
}

const std::vector<Patch>& factoryPatches()
{
    static const std::vector<Patch> patches = {
        make ("Init", [] (Patch&) {}),
        make ("Fat Bass", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft16, 0, (int) W::Saw };
            p.osc[1] = { (int) R::Ft16, 0.08f, (int) W::Square };
            p.osc[2] = { (int) R::Ft32, -0.06f, (int) W::Saw };
            p.oscVolume = { 7.5f, 6.5f, 5.0f };
            p.oscOn = { true, true, true };
            p.cutoff = -2.6f; p.emphasis = 2.0f; p.contourAmount = 4.5f;
            p.filterAttack = 0; p.filterDecay = 4.2f; p.filterSustain = 2.5f;
            p.loudAttack = 0; p.loudDecay = 5; p.loudSustain = 9;
            p.decayOn = true;
        }),
        make ("Plucked Funk", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft16, 0, (int) W::NarrowPulse };
            p.osc[1] = { (int) R::Ft16, 0.05f, (int) W::Saw };
            p.oscVolume = { 9, 6, 0 };
            p.oscOn = { true, true, false };
            p.cutoff = -3.4f; p.emphasis = 7.0f; p.contourAmount = 6.0f;
            p.filterDecay = 2.8f; p.filterSustain = 0;
            p.loudDecay = 3.6f; p.loudSustain = 0;
            p.decayOn = true;
        }),
        make ("Lead Saw", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft8, 0, (int) W::Saw };
            p.osc[1] = { (int) R::Ft8, 0.12f, (int) W::Saw };
            p.osc[2] = { (int) R::Lo, 3.0f, (int) W::Triangle };
            p.oscVolume = { 8, 8, 0 };
            p.oscOn = { true, true, false };
            p.osc3Control = false; p.oscModulation = true; p.modMix = 0;
            p.cutoff = 0.4f; p.emphasis = 4.0f; p.contourAmount = 3.0f;
            p.keyboardControl1 = true;
            p.filterAttack = 1.5f; p.filterDecay = 5; p.filterSustain = 6;
            p.loudSustain = 10; p.glideOn = true; p.glide = 3.0f;
        }),
        make ("Brass", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft8, 0, (int) W::Saw };
            p.osc[1] = { (int) R::Ft8, -0.1f, (int) W::Saw };
            p.osc[2] = { (int) R::Ft16, 0.07f, (int) W::Saw };
            p.oscVolume = { 7.5f, 6.5f, 4.5f };
            p.oscOn = { true, true, true };
            p.cutoff = -1.5f; p.emphasis = 1.5f; p.contourAmount = 6.5f;
            p.filterAttack = 4.2f; p.filterDecay = 5.5f; p.filterSustain = 6;
            p.loudAttack = 2.5f; p.loudSustain = 9; p.decayOn = true; p.loudDecay = 4.5f;
        }),
        make ("Whistle (Self-Osc)", [] (Patch& p) {
            p.oscOn = { false, false, false };
            p.cutoff = 0; p.emphasis = 10; p.contourAmount = 0;
            p.keyboardControl1 = p.keyboardControl2 = true;
            p.loudSustain = 10; p.glideOn = true; p.glide = 2.5f;
        }),
        make ("Overdrive Growl", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft16, 0, (int) W::Saw };
            p.osc[1] = { (int) R::Ft16, 7.0f, (int) W::Square };
            p.oscVolume = { 10, 9, 0 };
            p.oscOn = { true, true, false };
            p.extOn = true; p.extVolume = 7.5f; // the output-to-EXT-INPUT feedback trick
            p.cutoff = -1.0f; p.emphasis = 3.5f; p.contourAmount = 3.5f;
            p.filterDecay = 5; p.filterSustain = 4; p.loudSustain = 10; p.volume = 7;
        }),
        make ("Sub Wobble", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft32, 0, (int) W::Square };
            p.osc[1] = { (int) R::Ft16, 0.04f, (int) W::Saw };
            p.osc[2] = { (int) R::Lo, -2.0f, (int) W::Triangle };
            p.oscVolume = { 9, 6, 0 };
            p.oscOn = { true, true, false };
            p.osc3Control = false; p.filterModulation = true; p.modMix = 0;
            p.cutoff = -2.0f; p.emphasis = 5.5f; p.contourAmount = 1.0f;
            p.loudSustain = 10;
        }),
        make ("Noise Sweep", [] (Patch& p) {
            p.oscOn = { false, false, false };
            p.noiseOn = true; p.noiseVolume = 9; p.pinkNoise = false;
            p.cutoff = -3.0f; p.emphasis = 8.0f; p.contourAmount = 8.0f;
            p.filterAttack = 6.5f; p.filterDecay = 7; p.filterSustain = 0;
            p.loudAttack = 5; p.loudDecay = 7; p.loudSustain = 6; p.decayOn = true;
        }),
        make ("Trill Lead", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft8, 0, (int) W::WidePulse };
            p.osc[1] = { (int) R::Ft4, 0.05f, (int) W::Triangle };
            p.osc[2] = { (int) R::Lo, 4.0f, (int) W::Square };
            p.oscVolume = { 8, 4, 0 };
            p.oscOn = { true, true, false };
            p.osc3Control = false; p.oscModulation = true;
            p.cutoff = 1.0f; p.emphasis = 3; p.contourAmount = 2;
            p.loudSustain = 10;
        }),
        make ("Tri-Saw Sweep Bass", [] (Patch& p) {
            p.osc[0] = { (int) R::Ft16, 0, (int) W::TriSaw };
            p.osc[1] = { (int) R::Ft8, 0.15f, (int) W::TriSaw };
            p.oscVolume = { 9, 7, 0 };
            p.oscOn = { true, true, false };
            p.cutoff = -3.5f; p.emphasis = 6.0f; p.contourAmount = 7.0f;
            p.filterDecay = 3.5f; p.filterSustain = 1.5f; p.loudSustain = 8; p.decayOn = true;
        }),
    };
    return patches;
}
} // namespace fourpole
