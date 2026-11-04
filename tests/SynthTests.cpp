#include "TestHelpers.h"
#include "core/Mapping.h"
#include "core/Synth.h"
#include <juce_core/juce_core.h>

using namespace fourpole;

namespace
{
constexpr double sr = 48000.0;

std::vector<float> render (Synth& s, int samples)
{
    std::vector<float> out ((size_t) samples);
    for (int i = 0; i < samples; i += 256)
        s.process (out.data() + i, std::min (256, samples - i));
    return out;
}

Patch quiet()
{
    Patch p; // one saw, filter wide open, no contour sweep
    p.drift = 0.0f;
    p.cutoff = 5.0f;
    p.contourAmount = 0.0f;
    p.loudSustain = 10.0f;
    return p;
}
} // namespace

struct SynthTests : juce::UnitTest
{
    SynthTests() : juce::UnitTest ("Synth voice", "fourpole") {}

    void runTest() override
    {
        beginTest ("a key plays at concert pitch on 8', octaves on the other footages");
        for (auto [range, expected] : { std::pair { (int) Range::Ft8, 440.0 }, std::pair { (int) Range::Ft16, 220.0 }, std::pair { (int) Range::Ft4, 880.0 } })
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.osc[0].range = range;
            p.osc[0].wave = (int) Wave::Triangle;
            p.cutoff = 3.0f;
            s.setPatch (p);
            s.noteOn (69, 100);
            const auto x = render (s, 48000);
            const double hz = fptest::zeroCrossingHz (x, sr, 4800);
            expectWithinAbsoluteError (hz, expected, expected * 0.004, juce::String (hz, 2));
        }

        beginTest ("oscillator 2 frequency detunes by semitones");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.osc[1].frequency = 7.0f;
            p.oscOn = { false, true, false };
            s.setPatch (p);
            s.noteOn (69, 100);
            render (s, 480);
            expectWithinAbsoluteError (s.oscillatorHz (1), 440.0 * std::pow (2.0, 7.0 / 12.0), 0.5);
        }

        beginTest ("gate: silence before, sound while held, release after");
        {
            Synth s;
            s.prepare (sr);
            s.setPatch (quiet());
            expect (fptest::rms (render (s, 4800), 0, 4800) < 1.0e-6);
            s.noteOn (60, 100);
            expect (fptest::rms (render (s, 9600), 4800, 9600) > 0.05);
            s.noteOff (60);
            render (s, 4800); // 100 ms: the 10 ms release is long over
            expect (fptest::rms (render (s, 4800), 0, 4800) < 1.0e-3);
        }

        beginTest ("single trigger: legato keeps the contour, low note wins");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.loudAttack = 6.0f; // ~0.25 s
            s.setPatch (p);
            s.noteOn (60, 100);
            render (s, 4800);
            const double before = s.getLoudness();
            s.noteOn (55, 100);
            render (s, 48);
            expect (s.getLoudness() >= before, "attack continues instead of restarting");
            expectWithinAbsoluteError (s.getPitch(), 55.0, 0.01);
            s.noteOn (64, 100);
            render (s, 48);
            expectWithinAbsoluteError (s.getPitch(), 55.0, 0.01, "low-note priority");
        }

        beginTest ("glide lags the pitch like an RC");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            s.setPatch (p); // first note without glide, so it starts exactly on 48
            s.noteOn (48, 100);
            s.noteOff (48);
            render (s, 480);
            p.glideOn = true;
            p.glide = 5.0f;
            s.setPatch (p);
            s.noteOn (60, 100);
            const double tau = mapping::glideSeconds (5.0f);
            render (s, (int) (tau * sr));
            expectWithinAbsoluteError (s.getPitch(), 60.0 - 12.0 * std::exp (-1.0), 0.1, "one time constant: 63% of the way");
        }

        beginTest ("filter keyboard control: full tracking moves the cutoff an octave per octave");
        {
            auto cutoffFor = [] (int note, bool kb1, bool kb2) {
                Synth s;
                s.prepare (sr);
                auto p = quiet();
                p.cutoff = -1.0f;
                p.keyboardControl1 = kb1;
                p.keyboardControl2 = kb2;
                s.setPatch (p);
                s.noteOn (note, 100);
                render (s, 256);
                return s.getCutoffHz();
            };
            expectWithinAbsoluteError (cutoffFor (72, true, true) / cutoffFor (60, true, true), 2.0, 0.01);
            expectWithinAbsoluteError (cutoffFor (72, false, true) / cutoffFor (60, false, true), std::pow (2.0, 2.0 / 3.0), 0.01);
            expectWithinAbsoluteError (cutoffFor (72, false, false) / cutoffFor (60, false, false), 1.0, 0.001);
        }

        beginTest ("contour amount sweeps the cutoff up to 4 octaves");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.cutoff = -2.0f;
            p.contourAmount = 10.0f;
            p.filterSustain = 10.0f;
            s.setPatch (p);
            s.noteOn (60, 100);
            render (s, 4800);
            expectWithinAbsoluteError (std::log2 (s.getCutoffHz() / mapping::cutoffHz (-2.0f)), 4.0, 0.02);
        }

        beginTest ("OSC 3 CONTROL off: oscillator 3 ignores the keyboard (an LFO in LO)");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.osc3Control = false;
            p.osc[2].range = (int) Range::Lo;
            s.setPatch (p);
            s.noteOn (40, 100);
            render (s, 256);
            const double a = s.oscillatorHz (2);
            s.noteOff (40);
            s.noteOn (80, 100);
            render (s, 256);
            expectWithinAbsoluteError (s.oscillatorHz (2), a, 1.0e-6);
            expect (a < 10.0, "sub-audio: " + juce::String (a, 2) + " Hz");
        }

        beginTest ("oscillator modulation: mod wheel + oscillator 3 vibrato");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.osc3Control = false;
            p.osc[2].range = (int) Range::Lo;
            p.osc[2].frequency = 7.0f;
            p.osc[2].wave = (int) Wave::Triangle;
            p.oscModulation = true;
            s.setPatch (p);
            s.setModWheel (1.0f);
            s.noteOn (69, 100);
            double lo = 1e9, hi = 0;
            for (int i = 0; i < 200; ++i)
            {
                render (s, 240);
                lo = std::min (lo, s.oscillatorHz (0));
                hi = std::max (hi, s.oscillatorHz (0));
            }
            expect (12.0 * std::log2 (hi / lo) > 8.0, "wide vibrato at full wheel");
            s.setModWheel (0.0f);
            render (s, 240);
            expectWithinAbsoluteError (s.oscillatorHz (0), 440.0, 0.5, "none with the wheel down");
        }

        beginTest ("mixer overload: three oscillators at 10 light the lamp; one at 4 doesn't");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.oscVolume = { 4, 0, 0 };
            s.setPatch (p);
            s.noteOn (48, 100);
            render (s, 4800);
            expect (! s.isOverloaded());
            p.oscVolume = { 10, 10, 10 };
            p.oscOn = { true, true, true };
            s.setPatch (p);
            render (s, 4800);
            expect (s.isOverloaded());
        }

        beginTest ("EXT INPUT feedback with nothing plugged in drives the mixer");
        {
            auto levelWith = [] (bool ext) {
                Synth s;
                s.prepare (sr);
                auto p = quiet();
                p.oscVolume = { 5, 0, 0 };
                p.extOn = ext;
                p.extVolume = 10.0f;
                p.volume = 10.0f;
                s.setPatch (p);
                s.noteOn (45, 100);
                const auto x = render (s, 9600);
                return std::make_pair (fptest::rms (x, 4800, 9600), s.isOverloaded());
            };
            const auto dry = levelWith (false), fed = levelWith (true);
            expect (fed.first > dry.first * 1.1, "louder / denser with feedback");
            expect (fed.second, "overload lamp lights");
        }

        beginTest ("A-440 outputs a 440 Hz reference tone");
        {
            Synth s;
            s.prepare (sr);
            auto p = quiet();
            p.a440 = true;
            s.setPatch (p);
            const auto x = render (s, 48000);
            expectWithinAbsoluteError (fptest::zeroCrossingHz (x, sr, 100), 440.0, 0.5);
        }

        beginTest ("patches round-trip through JSON; every factory patch plays");
        {
            for (const auto& fp : factoryPatches())
            {
                const auto back = Patch::fromJson (juce::JSON::parse (juce::JSON::toString (fp.toJson())));
                expectEquals (back.name, fp.name);
                expect (juce::JSON::toString (back.toJson()) == juce::JSON::toString (fp.toJson()), fp.name);
                Synth s;
                s.prepare (sr);
                s.setPatch (fp);
                s.noteOn (48, 100);
                const auto x = render (s, 24000);
                bool finite = true;
                for (auto v : x)
                    finite = finite && std::isfinite (v);
                expect (finite, fp.name + " stays finite");
                expect (fptest::rms (x, 0, x.size()) > 1.0e-3, fp.name + " makes sound");
            }
        }
    }
};

static SynthTests synthTests;
