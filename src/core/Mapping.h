#pragma once

// Panel units -> physical values, following the ranges in docs/research.md.

#include "core/Patch.h"
#include <algorithm>
#include <cmath>

namespace fourpole::mapping
{
/** Octave offset of a footage relative to 8' (concert pitch). LO sits 4 octaves below 32'. */
inline double rangeOctaves (int range)
{
    static constexpr double table[kNumRanges] = { -6.0, -2.0, -1.0, 0.0, 1.0, 2.0 };
    return table[std::clamp (range, 0, kNumRanges - 1)];
}

inline double noteToHz (double midiNote) { return 440.0 * std::pow (2.0, (midiNote - 69.0) / 12.0); }

/** Contour attack: 1 ms .. 10 s on a log scale. */
inline double attackSeconds (float knob) { return 0.001 * std::pow (10000.0, std::clamp (knob, 0.0f, 10.0f) / 10.0); }
/** Contour decay (and release with DECAY on): 4 ms .. 35 s. */
inline double decaySeconds (float knob) { return 0.004 * std::pow (35.0 / 0.004, std::clamp (knob, 0.0f, 10.0f) / 10.0); }
/** Release with DECAY off: short and fixed. */
constexpr double kShortRelease = 0.010;
inline double sustainLevel (float knob) { return std::clamp (knob, 0.0f, 10.0f) / 10.0; }

/** Glide: an RC lag, 0 (off) .. ~5 s time constant per the knob. */
inline double glideSeconds (float knob)
{
    if (knob <= 0.0f)
        return 0.0;
    return 0.001 * std::pow (5000.0, std::clamp (knob, 0.0f, 10.0f) / 10.0);
}

/** Cutoff knob -5..+5 -> 20 Hz .. 20.5 kHz, one octave per unit. */
inline double cutoffHz (float knob) { return 20.0 * std::pow (2.0, std::clamp (knob, -5.0f, 5.0f) + 5.0); }
/** Amount of contour 0..10 -> 0..4 octaves of sweep. */
inline double contourOctaves (float knob) { return std::clamp (knob, 0.0f, 10.0f) * 0.4; }
/** Keyboard control switches: 1/3, 2/3, both = full tracking. */
inline double keyboardTracking (bool kb1, bool kb2) { return (kb1 ? 1.0 / 3.0 : 0.0) + (kb2 ? 2.0 / 3.0 : 0.0); }
/** Note the filter's keyboard tracking pivots around. */
constexpr double kTrackingPivotNote = 60.0;

/** Emphasis 0..10 -> ladder feedback (self-oscillates a little above 4). */
inline double emphasisFeedback (float knob)
{
    const double e = std::clamp (knob, 0.0f, 10.0f) / 10.0;
    return 4.25 * std::pow (e, 1.15);
}

/** Mixer volume 0..10 -> gain (audio taper; full is hot enough to drive the filter). */
inline double mixerGain (float knob)
{
    const double v = std::clamp (knob, 0.0f, 10.0f) / 10.0;
    return 1.2 * v * v;
}

/** Output volume 0..10 -> gain. */
inline double outputGain (float knob)
{
    const double v = std::clamp (knob, 0.0f, 10.0f) / 10.0;
    return v * v;
}

/** Modulation depths at full mod wheel. */
constexpr double kPitchModSemitones = 7.0;
constexpr double kFilterModOctaves = 2.5;

/** Pitch oscillator 3 rests at when OSC 3 CONTROL is off (middle C at 8'). */
constexpr double kOsc3FreeNote = 60.0;

} // namespace fourpole::mapping
