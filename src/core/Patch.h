#pragma once

// A patch, in panel units: the same numbers you'd write on a patch sheet for the hardware
// (knobs 0-10, cutoff -5..+5, footages, waveform positions). Mapping.h turns them into
// physical values (Hz, seconds, octaves).

#include <juce_core/juce_core.h>
#include <array>

namespace fourpole
{
enum class Range : int { Lo = 0, Ft32, Ft16, Ft8, Ft4, Ft2 };
constexpr int kNumRanges = 6;

/** Waveform selector positions. Position 1 is tri-saw on oscillators 1 and 2, reverse saw on oscillator 3. */
enum class Wave : int { Triangle = 0, TriSaw, Saw, Square, WidePulse, NarrowPulse };
constexpr int kNumWaves = 6;

enum class NotePriority : int { Low = 0, High, Last };

struct OscSettings
{
    int range = (int) Range::Ft8;
    float frequency = 0.0f; // semitones, -7..+7 (oscillators 2 and 3)
    int wave = (int) Wave::Saw;
};

struct Patch
{
    juce::String name = "Init";

    // ---- controllers
    float tune = 0.0f;          // semitones, -2.5..+2.5
    float glide = 0.0f;         // 0..10
    float modMix = 0.0f;        // 0 = all oscillator 3 .. 10 = all noise
    bool oscModulation = false;
    bool osc3Control = true;    // oscillator 3 follows the keyboard
    bool glideOn = false;
    bool decayOn = false;       // contours release at their decay time

    // ---- oscillators
    std::array<OscSettings, 3> osc {};

    // ---- mixer
    std::array<float, 3> oscVolume { 8.0f, 6.0f, 0.0f }; // 0..10
    std::array<bool, 3> oscOn { true, false, false };
    float extVolume = 0.0f;
    bool extOn = false;         // with nothing plugged in, the output feeds back here
    float noiseVolume = 0.0f;
    bool noiseOn = false;
    bool pinkNoise = false;

    // ---- filter
    float cutoff = 0.0f;        // -5..+5 (an octave per unit)
    float emphasis = 0.0f;      // 0..10
    float contourAmount = 5.0f; // 0..10 (0..4 octaves)
    bool keyboardControl1 = false; // 1/3 tracking
    bool keyboardControl2 = false; // 2/3 tracking (both = full)
    bool filterModulation = false;
    float filterAttack = 0.0f, filterDecay = 5.0f, filterSustain = 5.0f; // 0..10

    // ---- loudness contour
    float loudAttack = 0.0f, loudDecay = 5.0f, loudSustain = 8.0f;

    // ---- output
    float volume = 6.0f;        // 0..10
    bool outputOn = true;
    bool a440 = false;

    // ---- not on the hardware
    float bendRange = 2.0f;     // semitones each way
    NotePriority priority = NotePriority::Low;
    float velocity = 0.0f;      // 0..1: how much key velocity affects loudness (0 = none, like the original)
    float drift = 1.0f;         // 0..1: analog pitch drift

    juce::var toJson() const;
    static Patch fromJson (const juce::var& v);
};

/** The built-in patches. */
const std::vector<Patch>& factoryPatches();

} // namespace fourpole
