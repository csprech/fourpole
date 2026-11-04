#include "TestHelpers.h"
#include "core/Dsp.h"
#include "core/Mapping.h"
#include <juce_core/juce_core.h>

using namespace fourpole;

struct DspTests : juce::UnitTest
{
    DspTests() : juce::UnitTest ("DSP building blocks", "fourpole") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;

        beginTest ("oscillators hit their frequency on every waveform");
        for (int w = 0; w < kNumWaves; ++w)
        {
            Oscillator o;
            o.prepare (sr, 3);
            std::vector<float> x ((size_t) sr);
            for (auto& s : x)
                s = o.process (440.0, w, false);
            const double hz = fptest::zeroCrossingHz (x, sr, 100);
            expectWithinAbsoluteError (hz, 440.0, 1.0, "wave " + juce::String (w) + ": " + juce::String (hz, 2));
        }

        beginTest ("footages are octaves; LO is sub-audio");
        expectEquals (mapping::rangeOctaves ((int) Range::Ft16), -1.0);
        expectEquals (mapping::rangeOctaves ((int) Range::Ft2), 2.0);
        expect (mapping::noteToHz (60 + 12 * mapping::rangeOctaves ((int) Range::Lo)) < 5.0, "LO range at middle C is a few Hz");

        beginTest ("pulse waves have no DC offset");
        for (int w : { (int) Wave::Square, (int) Wave::WidePulse, (int) Wave::NarrowPulse })
        {
            Oscillator o;
            o.prepare (sr, 9);
            double sum = 0;
            for (int i = 0; i < 48000; ++i)
                sum += o.process (200.0, w, false);
            expectWithinAbsoluteError (sum / 48000.0, 0.0, 0.01);
        }

        beginTest ("polyBLEP saw aliases far less than a naive one");
        {
            // a 5 kHz saw at 48 kHz: energy between harmonics (aliases) should be small
            Oscillator o;
            o.prepare (sr, 1);
            const int n = 8192;
            std::vector<float> x ((size_t) n);
            for (auto& s : x)
                s = o.process (5000.0, (int) Wave::Saw, false);
            // compare against a naive saw via a crude "smoothness" measure: mean squared second difference
            double blep = 0, naive = 0, ph = 0;
            float prevN = 0, prev2N = 0;
            for (int i = 2; i < n; ++i)
            {
                blep += std::pow (x[(size_t) i] - 2 * x[(size_t) i - 1] + x[(size_t) i - 2], 2);
                ph += 5000.0 / sr;
                if (ph >= 1)
                    ph -= 1;
                const float v = (float) (2 * ph - 1);
                naive += std::pow (v - 2 * prevN + prev2N, 2);
                prev2N = prevN;
                prevN = v;
            }
            expect (blep < naive * 0.7, "band-limited steps are smoother");
        }

        beginTest ("ladder: 24 dB/oct low-pass");
        {
            auto levelAt = [&] (double hz, double cutoff) {
                LadderFilter f;
                f.prepare (sr);
                std::vector<float> y ((size_t) sr / 2);
                double ph = 0;
                for (auto& s : y)
                {
                    s = f.process ((float) (0.1 * std::sin (ph)), cutoff, 0.0);
                    ph += 2 * 3.141592653589793 * hz / sr;
                }
                return fptest::rms (y, y.size() / 2, y.size());
            };
            const double pass = levelAt (100.0, 1000.0), stop = levelAt (8000.0, 1000.0);
            const double db = 20.0 * std::log10 (stop / pass);
            logMessage ("3 octaves above cutoff: " + juce::String (db, 1) + " dB");
            expect (db < -54.0, "about 72 dB down 3 octaves above cutoff (24 dB/oct): " + juce::String (db, 1));
            const double at = levelAt (1000.0, 1000.0) / pass;
            expect (at > 0.15 && at < 0.6, "around -6..-12 dB at cutoff: " + juce::String (at, 3));
        }

        beginTest ("ladder: full emphasis self-oscillates at the cutoff frequency");
        for (double cutoff : { 220.0, 880.0, 2500.0 })
        {
            LadderFilter f;
            f.prepare (sr);
            std::vector<float> y ((size_t) sr);
            for (size_t i = 0; i < y.size(); ++i)
                y[i] = f.process (i < 10 ? 0.5f : 0.0f, cutoff, mapping::emphasisFeedback (10.0f));
            const double hz = fptest::zeroCrossingHz (y, sr, y.size() / 2);
            const double level = fptest::rms (y, y.size() / 2, y.size());
            logMessage ("cutoff " + juce::String (cutoff) + " Hz -> oscillates at " + juce::String (hz, 1) + " Hz, rms " + juce::String (level, 3));
            expect (level > 0.05, "sustained oscillation");
            expect (std::abs (hz / cutoff - 1.0) < 0.08, "within 8% of the cutoff: " + juce::String (hz, 1));
        }

        beginTest ("ladder: no self-oscillation at moderate emphasis");
        {
            LadderFilter f;
            f.prepare (sr);
            std::vector<float> y ((size_t) sr);
            for (size_t i = 0; i < y.size(); ++i)
                y[i] = f.process (i < 10 ? 0.5f : 0.0f, 1000.0, mapping::emphasisFeedback (6.0f));
            expect (fptest::rms (y, y.size() / 2, y.size()) < 1.0e-4, "rings out");
        }

        beginTest ("contour: attack reaches the peak on time, then decays to sustain");
        {
            Contour c;
            c.prepare (sr);
            c.trigger();
            int toPeak = -1;
            for (int i = 0; i < 48000; ++i)
                if (c.process (0.1, 0.5, 0.4, 0.01) >= 0.9999 && toPeak < 0)
                    toPeak = i;
            expectWithinAbsoluteError (toPeak / sr, 0.1, 0.01, "attack time");
            for (int i = 0; i < 48000 * 3; ++i)
                c.process (0.1, 0.5, 0.4, 0.01);
            expectWithinAbsoluteError (c.getLevel(), 0.4, 0.005, "settles at sustain");
            c.release();
            for (int i = 0; i < 480; ++i) // 10 ms
                c.process (0.1, 0.5, 0.4, 0.01);
            expect (c.getLevel() < 0.01, "short release");
        }

        beginTest ("contour: DECAY switch on releases at the decay time");
        {
            Contour c;
            c.prepare (sr);
            c.trigger();
            for (int i = 0; i < 48000; ++i)
                c.process (0.001, 0.5, 1.0, 0.5);
            c.release();
            for (int i = 0; i < 12000; ++i) // 0.25 s: half of a 0.5 s release
                c.process (0.001, 0.5, 1.0, 0.5);
            const double half = c.getLevel();
            expect (half > 0.05 && half < 0.2, "halfway through a 0.5 s release: " + juce::String (half, 3));
        }

        beginTest ("keyboard: low-note priority and single triggering");
        {
            KeyAssigner k;
            auto r = k.noteOn (60, NotePriority::Low);
            expect (r.retrigger && r.note == 60);
            r = k.noteOn (55, NotePriority::Low);
            expect (! r.retrigger && r.note == 55, "lower key takes over, legato");
            r = k.noteOn (67, NotePriority::Low);
            expect (r.note == 55 && ! r.pitchChanged, "higher key ignored while a lower one is held");
            r = k.noteOff (55, NotePriority::Low);
            expect (r.gateOn && r.note == 60 && ! r.retrigger, "falls back to the lowest still held");
            k.noteOff (60, NotePriority::Low);
            r = k.noteOff (67, NotePriority::Low);
            expect (! r.gateOn && r.note == -1);
            r = k.noteOn (62, NotePriority::Low);
            expect (r.retrigger, "a new phrase retriggers");
        }
        beginTest ("keyboard: high and last priority");
        {
            KeyAssigner k;
            k.noteOn (60, NotePriority::High);
            expect (k.noteOn (64, NotePriority::High).note == 64);
            expect (k.noteOn (62, NotePriority::High).note == 64);
            KeyAssigner l;
            l.noteOn (60, NotePriority::Last);
            l.noteOn (64, NotePriority::Last);
            expect (l.noteOn (62, NotePriority::Last).note == 62);
            expect (l.noteOff (62, NotePriority::Last).note == 64, "back to the previous key");
        }

        beginTest ("noise: pink falls off, white is flat");
        {
            Noise nz;
            double wLow = 0, wHigh = 0, pLow = 0, pHigh = 0, lpW = 0, lpP = 0;
            for (int i = 0; i < 200000; ++i)
            {
                const double w = nz.white(), pk = nz.pink();
                lpW += 0.02 * (w - lpW);
                lpP += 0.02 * (pk - lpP);
                wLow += lpW * lpW; wHigh += (w - lpW) * (w - lpW);
                pLow += lpP * lpP; pHigh += (pk - lpP) * (pk - lpP);
            }
            expect (pLow / pHigh > 4.0 * (wLow / wHigh), "pink has much more low end relative to highs");
        }
    }
};

static DspTests dspTests;
