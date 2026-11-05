#pragma once

// The voice's building blocks: band-limited oscillators, the nonlinear transistor-ladder filter,
// RC-style contour generators, noise, and keyboard assignment. All allocation-free.

#include "Patch.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace fourpole
{
// ------------------------------------------------------------------ oscillator

/** One VCO: six waveshapes, polyBLEP anti-aliasing, free-running phase, slow analog drift. */
class Oscillator
{
public:
    void prepare (double sampleRate, uint32_t seed);
    /** wave: a Wave position; reverseSawSlot: position 1 is reverse saw (oscillator 3) instead of tri-saw. */
    float process (double hz, int wave, bool reverseSawSlot);
    /** Slow random pitch wander in semitones (call per sample; scaled by the patch's drift). */
    double drift (double amount);
    double getPhase() const { return phase; }

private:
    double sampleRate = 48000.0, phase = 0.0;
    uint32_t rng = 1;
    double driftState = 0.0, driftTarget = 0.0;
    int driftCounter = 0;
};

// ------------------------------------------------------------------ filter

/** 24 dB/oct transistor ladder: four tanh-saturated one-pole stages in a feedback loop
    (after Huovilainen, DAFx 2004), run at 2x the sample rate. */
class LadderFilter
{
public:
    void prepare (double sampleRate);
    void reset();
    /** cutoff in Hz, feedback 0..~4.3 (self-oscillates above ~4). */
    float process (float input, double cutoffHz, double feedback);

private:
    float tick (float input, float g, float k);
    double sampleRate = 48000.0;
    float stage[4] {}, stageTanh[3] {}, delay[6] {};
};

// ------------------------------------------------------------------ contour

/** Attack / decay to sustain / release, with exponential (RC) segments like the hardware's. */
class Contour
{
public:
    void prepare (double sampleRate) { sr = sampleRate; }
    /** Gate on. A retrigger starts the attack from the current level, as analog contours do. */
    void trigger() { stage = Stage::Attack; }
    void release() { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset() { stage = Stage::Idle; level = 0.0; }
    /** Times in seconds; releaseTime is decay (DECAY on) or the short fixed release. */
    double process (double attack, double decay, double sustain, double releaseTime);
    double getLevel() const { return level; }
    bool isActive() const { return stage != Stage::Idle; }

private:
    enum class Stage { Idle, Attack, Decay, Release };
    Stage stage = Stage::Idle;
    double sr = 48000.0, level = 0.0;
};

// ------------------------------------------------------------------ noise

class Noise
{
public:
    float white();
    float pink(); // Paul Kellet's refined filter
private:
    uint32_t state = 22222;
    float b[7] {};
};

// ------------------------------------------------------------------ keyboard

/** Which key sounds, and whether the contours retrigger (single triggering: legato doesn't). */
class KeyAssigner
{
public:
    struct Result
    {
        int note = -1;          // note to play, -1 = none held
        bool gateOn = false;    // any key held
        bool retrigger = false; // start the contours (only from no keys held)
        bool pitchChanged = false;
    };

    Result noteOn (int note, NotePriority p);
    Result noteOff (int note, NotePriority p);
    void clear();
    int heldCount() const { return count; }

private:
    int choose (NotePriority p) const;
    std::array<bool, 128> held {};
    std::array<int, 128> order {}; // press order for last-note priority
    int counter = 0, count = 0, current = -1;
};

} // namespace fourpole
