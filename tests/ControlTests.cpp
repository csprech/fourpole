// Every control on the panel, one by one: does turning it change the sound the way the hardware's
// does? Each check plays the voice with the control at different settings and measures the result
// (pitch, brightness, level, envelope timing, cutoff). The last check fails if a control in the
// panel table has no sound test here, so a new knob can't be added without one.

#include "TestHelpers.h"
#include "core/Mapping.h"
#include "core/MidiMap.h"
#include "core/PanelControls.h"
#include "core/Synth.h"
#include <juce_core/juce_core.h>
#include <set>

using namespace fourpole;

namespace
{
constexpr double sr = 48000.0;

/** One sawtooth on oscillator 1 at 8', filter wide open, no contour sweep, full sustain, no drift. */
Patch clean()
{
    Patch p;
    p.drift = 0.0f;
    p.cutoff = 5.0f;
    p.emphasis = 0.0f;
    p.contourAmount = 0.0f;
    p.loudAttack = 0.0f;
    p.loudSustain = 10.0f;
    p.oscOn = { true, false, false };
    p.oscVolume = { 6.0f, 6.0f, 6.0f };
    for (auto& o : p.osc)
        o = { (int) Range::Ft8, 0.0f, (int) Wave::Saw };
    return p;
}

struct Voice
{
    Synth s;
    explicit Voice (const Patch& p)
    {
        s.prepare (sr);
        s.setPatch (p);
    }
    std::vector<float> run (int n, const float* externalInput = nullptr)
    {
        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; i += 256)
        {
            s.setExternalInput (externalInput != nullptr ? externalInput + i : nullptr);
            s.process (out.data() + i, std::min (256, n - i));
        }
        return out;
    }
    /** Runs n samples one at a time, recording f(synth) after each. */
    template <typename F>
    std::vector<double> trace (int n, F f)
    {
        std::vector<double> t;
        t.reserve ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float x;
            s.process (&x, 1);
            t.push_back (f (s));
        }
        return t;
    }
};

double brightness (const std::vector<float>& x, size_t from, size_t to)
{
    // RMS of the first difference over the RMS: rises with the spectral centroid
    double d = 0, a = 0;
    for (size_t i = std::max<size_t> (from, 1); i < to && i < x.size(); ++i)
    {
        d += (x[i] - x[i - 1]) * (x[i] - x[i - 1]);
        a += x[i] * x[i];
    }
    return a > 0 ? std::sqrt (d / a) : 0.0;
}

/** Amplitude of the component at hz (Goertzel over from..to). */
double toneAt (const std::vector<float>& x, size_t from, size_t to, double hz)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < to && i < x.size(); ++i)
    {
        const double s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / (double) (to - from);
}

double span (const std::vector<double>& t, size_t from = 0)
{
    const auto [lo, hi] = std::minmax_element (t.begin() + (long) from, t.end());
    return *hi - *lo;
}

double meanStep (const std::vector<double>& t)
{
    double a = 0;
    for (size_t i = 1; i < t.size(); ++i)
        a += std::abs (t[i] - t[i - 1]);
    return a / (double) std::max<size_t> (1, t.size() - 1);
}

double correlation (const std::vector<float>& a, const std::vector<float>& b, size_t from)
{
    double ab = 0, aa = 0, bb = 0;
    for (size_t i = from; i < a.size(); ++i)
    {
        ab += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    return ab / std::sqrt (std::max (1.0e-20, aa * bb));
}

double dutyCycle (const std::vector<float>& x, size_t from)
{
    size_t up = 0;
    for (size_t i = from; i < x.size(); ++i)
        up += x[i] > 0.0f ? 1 : 0;
    return (double) up / (double) (x.size() - from);
}

double semitones (double hz, double ref) { return 12.0 * std::log2 (hz / ref); }
} // namespace

struct ControlTests : juce::UnitTest
{
    ControlTests() : juce::UnitTest ("Panel controls", "fourpole") {}

    std::set<juce::String> covered;
    void covers (std::initializer_list<const char*> ids)
    {
        for (auto* id : ids)
        {
            expect (panel::find (id) != nullptr, juce::String ("no such control: ") + id);
            covered.insert (id);
        }
    }

    void runTest() override
    {
        tableTests();
        controllerTests();
        oscillatorTests();
        mixerTests();
        filterTests();
        contourTests();
        outputTests();
        performanceTests();

        beginTest ("every control on the panel has a sound test");
        for (auto& c : panel::controls())
            expect (covered.count (c.id) == 1, c.id + " has no sound test");
    }

    // ------------------------------------------------------------------ the table itself

    void tableTests()
    {
        beginTest ("panel table: unique IDs and names, each control sets only its own field, survives JSON");
        std::set<juce::String> ids, names;
        for (auto& c : panel::controls())
        {
            expect (ids.insert (c.id).second, "duplicate ID " + c.id);
            expect (names.insert (c.name).second, "duplicate name " + c.name);
            for (float v : { c.min, c.max })
            {
                Patch p = clean();
                const Patch before = p;
                c.set (p, v);
                expectWithinAbsoluteError (c.get (p), v, 1.0e-6f, c.id);
                for (auto& other : panel::controls())
                    if (&other != &c)
                        expectWithinAbsoluteError (other.get (p), other.get (before), 1.0e-6f, c.id + " also changed " + other.id);
                const auto back = Patch::fromJson (juce::JSON::parse (juce::JSON::toString (p.toJson())));
                expectWithinAbsoluteError (c.get (back), v, 1.0e-6f, c.id + " after a save / load");
            }
            Patch p;
            c.set (p, c.max + 100.0f);
            expect (c.get (p) <= c.max, c.id + " clamps");
        }
        expectEquals ((int) panel::controls().size(), 41, "the panel's 41 controls");
        const MidiMap defaults; // (not MidiMap().all() in the loop: the temporary would die before the loop runs)
        for (auto& [cc, id] : defaults.all())
            expect (panel::find (id) != nullptr, "default CC " + juce::String (cc) + " -> unknown control " + id);
    }

    // ------------------------------------------------------------------ controllers

    void controllerTests()
    {
        beginTest ("TUNE: all three oscillators, +/-2.5 semitones (oscillator 3 even when free-running)");
        covers ({ "tune" });
        for (float t : { -2.5f, 0.0f, 1.0f, 2.5f })
        {
            auto p = clean();
            p.tune = t;
            p.osc[0].wave = (int) Wave::Triangle;
            p.osc3Control = false;
            Voice v (p);
            v.s.noteOn (69, 100);
            const auto x = v.run (48000);
            const double want = 440.0 * std::pow (2.0, t / 12.0);
            expectWithinAbsoluteError (fptest::zeroCrossingHz (x, sr, 4800), want, want * 0.004, "heard pitch, tune " + juce::String (t));
            expectWithinAbsoluteError (v.s.oscillatorHz (1), want, want * 1.0e-6, "oscillator 2");
            expectWithinAbsoluteError (v.s.oscillatorHz (2), mapping::noteToHz (mapping::kOsc3FreeNote + t), 0.01, "free oscillator 3");
        }

        beginTest ("GLIDE + the GLIDE switch: the knob sets the time; switched off, pitch jumps");
        covers ({ "glide", "glideOn" });
        {
            auto settle = [] (float glide, bool on) {
                auto p = clean();
                Voice v (p); // the first note lands without glide...
                v.s.noteOn (48, 100);
                v.run (4800);
                v.s.noteOff (48);
                p.glide = glide; // ...then the glide settings apply to the next
                p.glideOn = on;
                v.s.setPatch (p);
                v.s.noteOn (60, 100);
                for (int i = 0; i < (int) sr * 10; ++i)
                {
                    float x;
                    v.s.process (&x, 1);
                    if (std::abs (v.s.getPitch() - 60.0) < 0.1)
                        return i / sr;
                }
                return 99.0;
            };
            expect (settle (8.0f, false) < 0.001, "GLIDE off: no glide whatever the knob says");
            expect (settle (0.0f, true) < 0.001, "knob at 0: no glide");
            const double t3 = settle (3.0f, true), t6 = settle (6.0f, true);
            expect (t3 > 0.02, "glide at 3 takes time: " + juce::String (t3, 3) + " s");
            expect (t6 > t3 * 3.0, "more knob, longer glide: " + juce::String (t6, 3) + " s");
            // an RC lag: within 0.1 of 12 semitones after ln(120) time constants
            expectWithinAbsoluteError (t6, std::log (120.0) * mapping::glideSeconds (6.0f), t6 * 0.05, "matches the knob's time");
        }

        beginTest ("OSC. MODULATION + MOD. wheel: oscillator 3 bends oscillators 1 and 2 (not itself), scaled by the wheel");
        covers ({ "oscModulation" });
        {
            auto lfoPatch = [] {
                auto p = clean();
                p.osc3Control = false;
                p.osc[2] = { (int) Range::Lo, 0.0f, (int) Wave::Triangle };
                p.oscModulation = true;
                return p;
            };
            auto pitchSpan = [] (const Patch& p, float wheel, int osc) {
                Voice v (p);
                v.s.setModWheel (wheel);
                v.s.noteOn (60, 100);
                return span (v.trace (48000, [osc] (Synth& s) { return semitones (s.oscillatorHz (osc), 440.0); }));
            };
            auto off = lfoPatch();
            off.oscModulation = false;
            expect (pitchSpan (off, 1.0f, 0) < 0.01, "switch off: no vibrato");
            expect (pitchSpan (lfoPatch(), 0.0f, 0) < 0.01, "wheel at 0: no vibrato");
            const double half = pitchSpan (lfoPatch(), 0.5f, 0), full = pitchSpan (lfoPatch(), 1.0f, 0);
            expect (half > 1.0 && full > half * 2.0, "wheel scales it: " + juce::String (half, 2) + " / " + juce::String (full, 2) + " semitones");
            expect (pitchSpan (lfoPatch(), 1.0f, 1) > 10.0, "oscillator 2 too");
            expect (pitchSpan (lfoPatch(), 1.0f, 2) < 0.01, "oscillator 3 isn't modulated by itself");
        }

        beginTest ("MODULATION MIX: 0 = oscillator 3 (smooth), 10 = noise (random)");
        covers ({ "modMix" });
        {
            auto jitter = [] (float mix) {
                auto p = clean();
                p.osc3Control = false;
                p.osc[2] = { (int) Range::Lo, 0.0f, (int) Wave::Triangle };
                p.oscModulation = true;
                p.modMix = mix;
                Voice v (p);
                v.s.setModWheel (1.0f);
                v.s.noteOn (60, 100);
                return meanStep (v.trace (24000, [] (Synth& s) { return semitones (s.oscillatorHz (0), 440.0); }));
            };
            const double lfo = jitter (0.0f), mid = jitter (5.0f), noise = jitter (10.0f);
            expect (noise > lfo * 50.0, "noise is jittery, the LFO smooth: " + juce::String (lfo, 5) + " vs " + juce::String (noise, 5));
            expect (mid > lfo && mid < noise, "in between at 5");
        }

        beginTest ("OSC. 3 CONTROL: on, oscillator 3 follows the keys; off, it holds its own pitch");
        covers ({ "osc3Control" });
        for (bool on : { true, false })
        {
            auto p = clean();
            p.osc3Control = on;
            Voice v (p);
            v.s.noteOn (48, 100);
            v.run (256);
            const double low = v.s.oscillatorHz (2);
            v.s.noteOff (48);
            v.s.noteOn (60, 100);
            v.run (256);
            const double high = v.s.oscillatorHz (2);
            if (on)
                expectWithinAbsoluteError (high / low, 2.0, 1.0e-6, "tracks the keyboard");
            else
            {
                expectWithinAbsoluteError (high, low, 1.0e-9, "ignores the keyboard");
                expectWithinAbsoluteError (high, mapping::noteToHz (mapping::kOsc3FreeNote), 0.01, "rests near 175 Hz at 8'");
            }
        }
    }

    // ------------------------------------------------------------------ oscillator bank

    void oscillatorTests()
    {
        beginTest ("RANGE: each footage is an octave, LO is sub-audio, on every oscillator");
        covers ({ "osc1.range", "osc2.range", "osc3.range" });
        for (int i = 0; i < 3; ++i)
        {
            for (int r = 0; r < kNumRanges; ++r)
            {
                auto p = clean();
                p.osc[(size_t) i].range = r;
                Voice v (p);
                v.s.noteOn (69, 100);
                v.run (256);
                const double want = 440.0 * std::pow (2.0, mapping::rangeOctaves (r));
                expectWithinAbsoluteError (v.s.oscillatorHz (i), want, want * 1.0e-6, "osc " + juce::String (i + 1) + " range " + juce::String (r));
            }
            for (auto [r, want] : { std::pair { (int) Range::Ft16, 220.0 }, std::pair { (int) Range::Ft4, 880.0 } })
            {
                auto p = clean();
                p.oscOn = { i == 0, i == 1, i == 2 };
                p.osc[(size_t) i] = { r, 0.0f, (int) Wave::Triangle };
                p.cutoff = 3.0f;
                Voice v (p);
                v.s.noteOn (69, 100);
                const auto x = v.run (48000);
                expectWithinAbsoluteError (fptest::zeroCrossingHz (x, sr, 4800), want, want * 0.004, "heard, osc " + juce::String (i + 1));
            }
        }

        beginTest ("WAVEFORM: six different shapes on each oscillator; position 2 is tri-saw on 1 and 2, reverse saw on 3");
        covers ({ "osc1.wave", "osc2.wave", "osc3.wave" });
        for (int i = 0; i < 3; ++i)
        {
            std::array<std::vector<float>, kNumWaves> shapes;
            for (int w = 0; w < kNumWaves; ++w)
            {
                auto p = clean();
                p.oscOn = { i == 0, i == 1, i == 2 };
                p.oscVolume = { 5.0f, 5.0f, 5.0f };
                p.osc[(size_t) i].wave = w;
                Voice v (p);
                v.s.noteOn (57, 100); // 220 Hz
                shapes[(size_t) w] = v.run (12000);
            }
            const auto osc = "osc " + juce::String (i + 1) + ": ";
            for (int a = 0; a < kNumWaves; ++a)
                for (int b = a + 1; b < kNumWaves; ++b)
                    if (! (i == 2 && a == 1 && b == 2)) // (reverse saw vs saw: checked as a flip below)
                        expect (std::abs (correlation (shapes[(size_t) a], shapes[(size_t) b], 2400)) < 0.97,
                            osc + "positions " + juce::String (a + 1) + " and " + juce::String (b + 1) + " sound different");
            const double saw = correlation (shapes[1], shapes[2], 2400);
            if (i == 2)
                expect (saw < -0.97, osc + "reverse saw is the saw flipped: " + juce::String (saw, 3));
            else
            {
                expect (saw > 0.5, osc + "tri-saw has the saw in it");
                expect (correlation (shapes[1], shapes[0], 2400) > 0.5, osc + "and the triangle");
            }
            expect (brightness (shapes[0], 2400, 12000) < brightness (shapes[2], 2400, 12000) * 0.5, osc + "triangle is darker than saw");
            expectWithinAbsoluteError (dutyCycle (shapes[3], 2400), 0.50, 0.03, osc + "square");
            expectWithinAbsoluteError (dutyCycle (shapes[4], 2400), 0.32, 0.03, osc + "wide pulse");
            expectWithinAbsoluteError (dutyCycle (shapes[5], 2400), 0.14, 0.03, osc + "narrow pulse");
        }

        beginTest ("FREQUENCY: oscillators 2 and 3 tune +/-7 semitones; oscillator 3 free-running spans +/-3 octaves");
        covers ({ "osc2.frequency", "osc3.frequency" });
        for (int i = 1; i < 3; ++i)
            for (float f : { -7.0f, -3.5f, 0.0f, 2.0f, 7.0f })
            {
                auto p = clean();
                p.osc[(size_t) i].frequency = f;
                Voice v (p);
                v.s.noteOn (69, 100);
                v.run (256);
                expectWithinAbsoluteError (semitones (v.s.oscillatorHz (i), v.s.oscillatorHz (0)), (double) f, 1.0e-6,
                                           "osc " + juce::String (i + 1) + " at " + juce::String (f));
            }
        {
            auto p = clean();
            p.oscOn = { false, true, false };
            p.osc[1] = { (int) Range::Ft8, 7.0f, (int) Wave::Triangle };
            p.cutoff = 3.0f;
            Voice v (p);
            v.s.noteOn (57, 100);
            const double want = 220.0 * std::pow (2.0, 7.0 / 12.0);
            expectWithinAbsoluteError (fptest::zeroCrossingHz (v.run (48000), sr, 4800), want, want * 0.004, "heard: a fifth up");
        }
        for (auto [f, st] : { std::pair { -7.0f, -36.0 }, std::pair { 0.0f, 0.0 }, std::pair { 3.5f, 18.0 }, std::pair { 7.0f, 36.0 } })
        {
            auto p = clean();
            p.osc3Control = false;
            p.osc[2].frequency = f;
            Voice v (p);
            v.s.noteOn (69, 100);
            v.run (256);
            expectWithinAbsoluteError (semitones (v.s.oscillatorHz (2), mapping::noteToHz (mapping::kOsc3FreeNote)), st, 1.0e-6,
                                       "free osc 3 at " + juce::String (f));
        }
        {
            // the LFO range in LO
            auto lfoHz = [] (float f) {
                auto p = clean();
                p.osc3Control = false;
                p.osc[2] = { (int) Range::Lo, f, (int) Wave::Triangle };
                Voice v (p);
                v.run (256);
                return v.s.oscillatorHz (2);
            };
            expect (lfoHz (-7.0f) < 0.5, "slowest LFO: " + juce::String (lfoHz (-7.0f), 2) + " Hz");
            expect (lfoHz (7.0f) > 15.0, "fastest LFO: " + juce::String (lfoHz (7.0f), 1) + " Hz");
        }
    }

    // ------------------------------------------------------------------ mixer

    void mixerTests()
    {
        beginTest ("MIXER: each oscillator's volume sets its level, its switch cuts it");
        covers ({ "osc1.volume", "osc2.volume", "osc3.volume", "osc1.on", "osc2.on", "osc3.on" });
        for (int i = 0; i < 3; ++i)
        {
            auto level = [i] (float vol, bool on) {
                auto p = clean();
                p.oscOn = { false, false, false };
                p.oscOn[(size_t) i] = on;
                p.oscVolume[(size_t) i] = vol;
                Voice v (p);
                v.s.noteOn (57, 100);
                return fptest::rms (v.run (9600), 2400, 9600);
            };
            const auto osc = "osc " + juce::String (i + 1) + ": ";
            expect (level (0.0f, true) < 1.0e-4, osc + "volume 0 is silent");
            expect (level (10.0f, false) < 1.0e-4, osc + "switched off is silent at 10");
            double last = 0.0;
            for (float vol : { 2.5f, 5.0f, 7.5f, 10.0f })
            {
                const double l = level (vol, true);
                expect (l > last * 1.1, osc + "louder at " + juce::String (vol) + ": " + juce::String (l, 4));
                last = l;
            }
        }

        beginTest ("EXTERNAL INPUT: with a cable in, its volume and switch pass the input to the filter");
        covers ({ "extVolume", "extOn" });
        {
            std::vector<float> input (24000);
            for (size_t n = 0; n < input.size(); ++n)
                input[n] = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * (double) n / sr);
            auto heard = [&input] (float vol, bool on) {
                auto p = clean();
                p.oscOn = { false, false, false };
                p.extOn = on;
                p.extVolume = vol;
                Voice v (p);
                v.s.noteOn (60, 100);
                return toneAt (v.run ((int) input.size(), input.data()), 4800, input.size(), 330.0);
            };
            expect (heard (8.0f, false) < 1.0e-4, "switched off");
            expect (heard (0.0f, true) < 1.0e-4, "volume 0");
            const double quiet = heard (4.0f, true), loud = heard (8.0f, true);
            expect (quiet > 0.005 && loud > quiet * 2.0, "the knob sets the level: " + juce::String (quiet, 4) + " / " + juce::String (loud, 4));
        }
        beginTest ("EXTERNAL INPUT, nothing plugged in: the output feeds back, harder with the knob and with the main VOLUME");
        {
            struct Fed
            {
                double drive, fundamental, rumble;
                bool lit;
            };
            auto fed = [] (float ext, float volume, float cutoff) {
                auto p = clean();
                p.oscVolume[0] = 5.0f;
                p.extOn = true;
                p.extVolume = ext;
                p.volume = volume;
                p.cutoff = cutoff;
                p.emphasis = cutoff < 0.0f ? 4.0f : 0.0f;
                Voice v (p);
                v.s.noteOn (45, 100);
                bool lit = false;
                std::vector<float> x;
                for (int k = 0; k < 20; ++k)
                {
                    const auto b = v.run (2400);
                    x.insert (x.end(), b.begin(), b.end());
                    lit = lit || v.s.isOverloaded();
                }
                const size_t from = 9600;
                return Fed { fptest::rms (x, from, x.size()) / mapping::outputGain (volume), // how hard the voice itself is driven
                             toneAt (x, from, x.size(), 110.0),
                             toneAt (x, from, x.size(), 12.0) + toneAt (x, from, x.size(), 20.0) + toneAt (x, from, x.size(), 30.0), lit };
            };
            for (float cutoff : { 5.0f, -1.0f })
            {
                const auto where = juce::String (cutoff > 0 ? "filter open: " : "filter at -1: ");
                const auto dry = fed (0.0f, 6.0f, cutoff), some = fed (5.0f, 6.0f, cutoff), lots = fed (10.0f, 6.0f, cutoff);
                expect (some.drive > dry.drive * 1.1 && lots.drive > some.drive * 1.5,
                        where + "drive rises with EXT: " + juce::String (dry.drive, 3) + ", " + juce::String (some.drive, 3) + ", " + juce::String (lots.drive, 3));
                expect (! dry.lit && ! fed (10.0f, 3.0f, cutoff).lit, where + "no overload without feedback, or with the main VOLUME down");
                const auto hot = fed (10.0f, 10.0f, cutoff);
                expect (hot.lit, where + "EXT and VOLUME up: OVERLOAD");
                expect (hot.rumble < hot.fundamental * 0.15, where + "and no subsonic latch-up: " + juce::String (hot.rumble, 4) + " vs " + juce::String (hot.fundamental, 4));
            }
        }

        beginTest ("NOISE: volume, on/off, and pink is darker than white");
        covers ({ "noiseVolume", "noiseOn", "pinkNoise" });
        {
            auto noise = [] (float vol, bool on, bool pink) {
                auto p = clean();
                p.oscOn = { false, false, false };
                p.noiseOn = on;
                p.noiseVolume = vol;
                p.pinkNoise = pink;
                Voice v (p);
                v.s.noteOn (60, 100);
                return v.run (24000);
            };
            expect (fptest::rms (noise (8.0f, false, false), 2400, 24000) < 1.0e-4, "switched off");
            expect (fptest::rms (noise (0.0f, true, false), 2400, 24000) < 1.0e-4, "volume 0");
            expect (fptest::rms (noise (8.0f, true, false), 2400, 24000) > fptest::rms (noise (4.0f, true, false), 2400, 24000) * 2.0, "louder");
            const double white = brightness (noise (8.0f, true, false), 2400, 24000), pink = brightness (noise (8.0f, true, true), 2400, 24000);
            expect (pink < white * 0.6, "pink " + juce::String (pink, 3) + " vs white " + juce::String (white, 3));
        }

        beginTest ("MIXER: pushing it lights OVERLOAD");
        {
            auto p = clean();
            p.oscOn = { true, true, true };
            p.oscVolume = { 10.0f, 10.0f, 10.0f };
            p.osc[1].frequency = 0.1f;
            p.osc[2].frequency = -0.1f;
            Voice v (p);
            v.s.noteOn (45, 100);
            bool lit = false;
            for (int k = 0; k < 20 && ! lit; ++k)
            {
                v.run (2400);
                lit = v.s.isOverloaded();
            }
            expect (lit);
        }
    }

    // ------------------------------------------------------------------ filter

    void filterTests()
    {
        beginTest ("CUTOFF FREQUENCY: brighter as it opens, one octave per unit");
        covers ({ "cutoff" });
        {
            double last = 0.0;
            for (float c : { -3.0f, -1.0f, 1.0f, 3.0f })
            {
                auto p = clean();
                p.cutoff = c;
                Voice v (p);
                v.s.noteOn (33, 100); // a 55 Hz saw: harmonics all the way up
                const auto x = v.run (24000);
                const double b = brightness (x, 4800, 24000);
                expect (b > last * 1.3, "brighter at " + juce::String (c) + ": " + juce::String (b, 3));
                last = b;
                expectWithinAbsoluteError (v.s.getCutoffHz(), mapping::cutoffHz (c), mapping::cutoffHz (c) * 1.0e-6, "cutoff " + juce::String (c));
            }
        }

        beginTest ("EMPHASIS: a resonant peak at the cutoff, self-oscillation at full");
        covers ({ "emphasis" });
        {
            auto peak = [] (float e) {
                // a 98 Hz saw with the cutoff on its 10th harmonic: the peak lifts that harmonic over the fundamental
                auto p = clean();
                p.cutoff = (float) std::log2 (980.0 / 20.0) - 5.0f;
                p.emphasis = e;
                p.oscVolume[0] = 4.0f;
                Voice v (p);
                v.s.noteOn (43, 100);
                const auto x = v.run (24000);
                return toneAt (x, 4800, 24000, mapping::noteToHz (43) * 10.0) / toneAt (x, 4800, 24000, mapping::noteToHz (43));
            };
            const double none = peak (0.0f), some = peak (5.0f), lots = peak (8.0f);
            expect (some > none * 1.5 && lots > some * 1.5, "peak grows: " + juce::String (none, 3) + ", " + juce::String (some, 3) + ", " + juce::String (lots, 3));
            auto selfOsc = [] (float e) {
                auto p = clean();
                p.oscOn = { false, false, false };
                p.cutoff = 0.0f;
                p.emphasis = e;
                Voice v (p);
                v.s.noteOn (60, 100);
                return v.run (48000);
            };
            expect (fptest::rms (selfOsc (0.0f), 24000, 48000) < 1.0e-4, "no input, no emphasis: silent");
            const auto whistle = selfOsc (10.0f);
            expect (fptest::rms (whistle, 24000, 48000) > 0.01, "at 10 it whistles with nothing in");
            const double hz = fptest::zeroCrossingHz (whistle, sr, 24000);
            expectWithinAbsoluteError (hz, mapping::cutoffHz (0.0f), mapping::cutoffHz (0.0f) * 0.1, "at the cutoff: " + juce::String (hz, 1) + " Hz");
        }

        beginTest ("AMOUNT OF CONTOUR: the filter contour sweeps 0 to 4 octaves");
        covers ({ "contourAmount" });
        for (float a : { 0.0f, 5.0f, 10.0f })
        {
            auto p = clean();
            p.cutoff = -3.0f;
            p.contourAmount = a;
            p.filterAttack = 0.0f;
            p.filterDecay = 4.0f;
            p.filterSustain = 0.0f;
            Voice v (p);
            v.s.noteOn (60, 100);
            const auto oct = v.trace (9600, [] (Synth& s) { return std::log2 (s.getCutoffHz() / mapping::cutoffHz (-3.0f)); });
            expectWithinAbsoluteError (*std::max_element (oct.begin(), oct.end()), mapping::contourOctaves (a), 0.05, "amount " + juce::String (a));
        }

        beginTest ("FILTER MODULATION + MOD. wheel: the modulation mix sweeps the cutoff");
        covers ({ "filterModulation" });
        for (bool on : { false, true })
        {
            auto p = clean();
            p.cutoff = -1.0f;
            p.osc3Control = false;
            p.osc[2] = { (int) Range::Lo, 0.0f, (int) Wave::Triangle };
            p.filterModulation = on;
            Voice v (p);
            v.s.setModWheel (1.0f);
            v.s.noteOn (60, 100);
            const double s = span (v.trace (48000, [] (Synth& x) { return std::log2 (x.getCutoffHz()); }));
            if (on)
                expect (s > 4.0, "sweeps " + juce::String (s, 2) + " octaves");
            else
                expect (s < 1.0e-6, "off: still");
        }

        beginTest ("KEYBOARD CONTROL 1 and 2: the cutoff follows the keys by 1/3, 2/3, or (both) fully");
        covers ({ "keyboardControl1", "keyboardControl2" });
        for (auto [k1, k2, want] : { std::tuple { false, false, 0.0 }, std::tuple { true, false, 1.0 / 3.0 }, std::tuple { false, true, 2.0 / 3.0 },
                                     std::tuple { true, true, 1.0 } })
        {
            auto p = clean();
            p.cutoff = -2.0f;
            p.keyboardControl1 = k1;
            p.keyboardControl2 = k2;
            auto cutoffFor = [&p] (int note) {
                Voice v (p);
                v.s.noteOn (note, 100);
                v.run (256);
                return v.s.getCutoffHz();
            };
            expectWithinAbsoluteError (std::log2 (cutoffFor (72) / cutoffFor (60)), want, 1.0e-6,
                                       "octaves of cutoff per octave of keys: " + juce::String (want, 3));
        }
    }

    // ------------------------------------------------------------------ contours

    /** The contour's level per sample: the filter one read back from the cutoff, or the loudness one. */
    static std::vector<double> contour (bool filter, float attack, float decay, float sustain, int samples)
    {
        auto p = clean();
        p.cutoff = -5.0f;
        p.contourAmount = 10.0f;
        (filter ? p.filterAttack : p.loudAttack) = attack;
        (filter ? p.filterDecay : p.loudDecay) = decay;
        (filter ? p.filterSustain : p.loudSustain) = sustain;
        Voice v (p);
        v.s.noteOn (60, 100);
        return v.trace (samples, [filter] (Synth& s) {
            return filter ? std::log2 (s.getCutoffHz() / mapping::cutoffHz (-5.0f)) / mapping::contourOctaves (10.0f) : s.getLoudness();
        });
    }

    static double secondsToPeak (const std::vector<double>& c)
    {
        return (double) std::distance (c.begin(), std::max_element (c.begin(), c.end())) / sr;
    }

    static double secondsToSettle (const std::vector<double>& c, double sustain)
    {
        const size_t peak = (size_t) std::distance (c.begin(), std::max_element (c.begin(), c.end()));
        for (size_t i = peak; i < c.size(); ++i)
            if (c[i] - sustain < 0.05 * (1.0 - sustain))
                return (double) (i - peak) / sr;
        return 99.0;
    }

    void contourTests()
    {
        for (bool filter : { true, false })
        {
            const juce::String which = filter ? "FILTER" : "LOUDNESS";
            covers ({ filter ? "filterAttack" : "loudAttack", filter ? "filterDecay" : "loudDecay", filter ? "filterSustain" : "loudSustain" });

            beginTest (which + " ATTACK TIME: reaches the peak in the knob's time");
            for (float a : { 0.0f, 4.0f, 7.0f })
            {
                const double t = secondsToPeak (contour (filter, a, 5.0f, 5.0f, (int) (sr * 1.2)));
                const double want = mapping::attackSeconds (a);
                expectWithinAbsoluteError (t, want, want * 0.1 + 0.0005, "attack " + juce::String (a) + ": " + juce::String (t, 4) + " s");
            }

            beginTest (which + " DECAY TIME: falls to the sustain level in the knob's time");
            double last = 0.0;
            for (float d : { 2.0f, 5.0f, 7.0f })
            {
                const double t = secondsToSettle (contour (filter, 0.0f, d, 0.0f, (int) (sr * 6.0)), 0.0);
                // RC: 95 % of the way after 3 time constants of decay / 4.6
                const double want = std::log (20.0) * mapping::decaySeconds (d) / 4.6;
                expectWithinAbsoluteError (t, want, want * 0.1, "decay " + juce::String (d) + ": " + juce::String (t, 3) + " s");
                expect (t > last, "longer with more knob");
                last = t;
            }

            beginTest (which + " SUSTAIN LEVEL: where the contour rests while the key is held");
            for (float s : { 0.0f, 3.0f, 8.0f, 10.0f })
            {
                const auto c = contour (filter, 0.0f, 2.0f, s, (int) sr);
                expectWithinAbsoluteError (c.back(), mapping::sustainLevel (s), 0.01, "sustain " + juce::String (s));
            }
        }

        beginTest ("LOUDNESS contour is what you hear: slow attack starts quiet, sustain 0 dies away");
        {
            auto onset = [] (float attack) {
                auto p = clean();
                p.loudAttack = attack;
                Voice v (p);
                v.s.noteOn (57, 100);
                return fptest::rms (v.run (2400), 0, 2400);
            };
            expect (onset (6.0f) < onset (0.0f) * 0.2, "attack 6 is still quiet after 50 ms");
            auto p = clean();
            p.loudSustain = 0.0f;
            p.loudDecay = 2.0f;
            Voice v (p);
            v.s.noteOn (57, 100);
            const auto x = v.run (48000);
            expect (fptest::rms (x, 40000, 48000) < fptest::rms (x, 0, 2400) * 0.01, "sustain 0: the note decays to silence while held");
        }

        beginTest ("DECAY switch: off, notes stop at once; on, they release at the DECAY times");
        covers ({ "decayOn" });
        for (bool on : { false, true })
        {
            auto p = clean();
            p.loudDecay = 6.0f;
            p.filterDecay = 6.0f;
            p.cutoff = -3.0f;
            p.contourAmount = 5.0f;
            p.filterSustain = 10.0f;
            p.decayOn = on;
            Voice v (p);
            v.s.noteOn (57, 100);
            v.run (24000);
            v.s.noteOff (57);
            v.run (4800); // 100 ms after letting go
            const double loud = v.s.getLoudness(), cut = std::log2 (v.s.getCutoffHz() / mapping::cutoffHz (-3.0f));
            if (on)
                expect (loud > 0.5 && cut > 1.0, "still ringing out: " + juce::String (loud, 3));
            else
                expect (loud < 1.0e-3 && cut < 0.01, "already silent: " + juce::String (loud, 5));
        }
    }

    // ------------------------------------------------------------------ output

    void outputTests()
    {
        beginTest ("VOLUME: sets the output level on an audio taper; 0 is silent");
        covers ({ "volume" });
        {
            auto level = [] (float vol) {
                auto p = clean();
                p.volume = vol;
                Voice v (p);
                v.s.noteOn (57, 100);
                return fptest::rms (v.run (9600), 2400, 9600);
            };
            expect (level (0.0f) == 0.0, "volume 0");
            expectWithinAbsoluteError (level (10.0f) / level (5.0f), mapping::outputGain (10.0f) / mapping::outputGain (5.0f), 0.02, "10 vs 5");
        }

        beginTest ("MAIN OUTPUT switch: off is silent");
        covers ({ "outputOn" });
        for (bool on : { true, false })
        {
            auto p = clean();
            p.outputOn = on;
            Voice v (p);
            v.s.noteOn (57, 100);
            const double l = fptest::rms (v.run (9600), 2400, 9600);
            expect (on ? l > 0.05 : l == 0.0, on ? "on" : "off");
        }

        beginTest ("A-440: a 440 Hz reference tone with no key held");
        covers ({ "a440" });
        for (bool on : { false, true })
        {
            auto p = clean();
            p.a440 = on;
            Voice v (p);
            const auto x = v.run (48000);
            if (on)
                expectWithinAbsoluteError (fptest::zeroCrossingHz (x, sr, 0), 440.0, 0.5, "440 Hz");
            else
                expect (fptest::rms (x, 0, 48000) == 0.0, "off: silence");
        }
    }

    // ------------------------------------------------------------------ wheels, keys and the Setup menu

    void performanceTests()
    {
        beginTest ("PITCH wheel: bends all three oscillators by the bend range");
        for (auto [range, bend] : { std::pair { 2.0f, 1.0f }, std::pair { 2.0f, -1.0f }, std::pair { 7.0f, 1.0f }, std::pair { 12.0f, -0.5f } })
        {
            auto p = clean();
            p.bendRange = range;
            p.osc3Control = false;
            Voice v (p);
            v.s.noteOn (69, 100);
            v.s.setPitchBend (bend);
            v.run (256);
            const double want = range * bend;
            expectWithinAbsoluteError (semitones (v.s.oscillatorHz (0), 440.0), (double) want, 1.0e-6, "osc 1");
            expectWithinAbsoluteError (semitones (v.s.oscillatorHz (1), 440.0), (double) want, 1.0e-6, "osc 2");
            expectWithinAbsoluteError (semitones (v.s.oscillatorHz (2), mapping::noteToHz (mapping::kOsc3FreeNote)), (double) want, 1.0e-6,
                                       "osc 3 (free-running, still bends)");
        }

        beginTest ("Note priority (Setup): low, high and last note");
        for (auto [prio, want] : { std::pair { NotePriority::Low, 60.0 }, std::pair { NotePriority::High, 67.0 }, std::pair { NotePriority::Last, 64.0 } })
        {
            auto p = clean();
            p.priority = prio;
            Voice v (p);
            v.s.noteOn (60, 100);
            v.s.noteOn (67, 100);
            v.s.noteOn (64, 100);
            v.run (256);
            expectWithinAbsoluteError (v.s.getPitch(), want, 1.0e-9, "priority " + juce::String ((int) prio));
        }

        beginTest ("Velocity (Setup): off ignores how hard you play; full follows it");
        for (float amount : { 0.0f, 1.0f })
        {
            auto level = [amount] (int velocity) {
                auto p = clean();
                p.velocity = amount;
                Voice v (p);
                v.s.noteOn (57, velocity);
                return fptest::rms (v.run (9600), 2400, 9600);
            };
            const double ratio = level (32) / level (127);
            if (amount == 0.0f)
                expectWithinAbsoluteError (ratio, 1.0, 1.0e-9, "off");
            else
                expectWithinAbsoluteError (ratio, 32.0 / 127.0, 0.01, "full");
        }

        beginTest ("Drift (Setup): off is dead steady; vintage wanders a few cents");
        for (float d : { 0.0f, 1.0f })
        {
            auto p = clean();
            p.drift = d;
            Voice v (p);
            v.s.noteOn (69, 100);
            std::vector<double> cents;
            for (int k = 0; k < 400; ++k)
            {
                v.run (240);
                cents.push_back (100.0 * semitones (v.s.oscillatorHz (0), 440.0));
            }
            if (d == 0.0f)
                expect (span (cents) < 1.0e-9, "steady");
            else
                expect (span (cents) > 0.5 && span (cents) < 15.0, "wanders " + juce::String (span (cents), 2) + " cents");
        }
    }
};

static ControlTests controlTests;
