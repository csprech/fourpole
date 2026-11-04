#pragma once

// The complete monophonic voice, wired like the hardware:
//   3 oscillators + noise + external (output feedback) -> mixer (saturating, overload lamp)
//   -> transistor ladder (filter contour, keyboard tracking, modulation) -> loudness contour -> output
// Platform-neutral and allocation-free; the engine calls it from the audio thread.

#include "core/Dsp.h"
#include "core/Patch.h"

namespace fourpole
{
class Synth
{
public:
    void prepare (double sampleRate);
    void setPatch (const Patch& p) { patch = p; }
    const Patch& getPatch() const { return patch; }

    void noteOn (int note, int velocity);
    void noteOff (int note);
    void allNotesOff();
    void setPitchBend (float bend) { pitchBend = std::clamp (bend, -1.0f, 1.0f); } // -1..1
    void setModWheel (float m) { modWheel = std::clamp (m, 0.0f, 1.0f); }           // 0..1

    /** Renders mono into `out` (numSamples). */
    void process (float* out, int numSamples);

    // ---- state for the UI and tests
    bool isOverloaded() const { return overloadHold > 0; }
    bool isGateOn() const { return gate; }
    double getPitch() const { return pitch; }           // glided keyboard pitch (MIDI note units)
    double getCutoffHz() const { return lastCutoffHz; }
    double getLoudness() const { return loudness.getLevel(); }
    double oscillatorHz (int index) const { return lastOscHz[(size_t) index]; }

private:
    double sampleRate = 48000.0;
    Patch patch;
    std::array<Oscillator, 3> osc;
    LadderFilter filter;
    Contour filterContour, loudness;
    Noise noise;
    KeyAssigner keys;

    bool gate = false;
    double targetPitch = 60.0, pitch = 60.0;
    float velocityGain = 1.0f, pitchBend = 0.0f, modWheel = 0.0f;
    float lastOsc3 = 0.0f, lastOutput = 0.0f;
    double dcState = 0.0, a440Phase = 0.0;
    int overloadHold = 0;
    double lastCutoffHz = 1000.0;
    std::array<double, 3> lastOscHz {};
};
} // namespace fourpole
