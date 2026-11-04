#include "core/Synth.h"
#include "core/Mapping.h"
#include <algorithm>

namespace fourpole
{
void Synth::prepare (double sr)
{
    sampleRate = sr > 0 ? sr : 48000.0;
    for (int i = 0; i < 3; ++i)
        osc[(size_t) i].prepare (sampleRate, (uint32_t) (i + 1) * 7919u);
    filter.prepare (sampleRate);
    filterContour.prepare (sampleRate);
    loudness.prepare (sampleRate);
    filterContour.reset();
    loudness.reset();
    keys.clear();
    gate = false;
    dcState = 0.0;
    lastOutput = 0.0f;
}

void Synth::setSustain (bool down)
{
    if (down == sustainDown)
        return;
    sustainDown = down;
    if (! down)
        for (int n = 0; n < 128; ++n)
            if (sustained[(size_t) n])
            {
                sustained[(size_t) n] = false;
                if (! physicallyHeld[(size_t) n])
                    noteOff (n);
            }
}

void Synth::setPower (bool on)
{
    if (on == powered)
        return;
    powered = on;
    if (! on)
    {
        keys.clear();
        physicallyHeld.fill (false);
        sustained.fill (false);
        gate = false;
        filterContour.reset();
        loudness.reset();
        filter.reset();
        lastOutput = 0.0f;
        dcState = 0.0; // or the AC-coupling filter would leak a decaying offset after power-on
    }
}

void Synth::noteOn (int note, int velocity)
{
    if (note < 0 || note > 127 || ! powered)
        return;
    physicallyHeld[(size_t) note] = true;
    sustained[(size_t) note] = false;
    const auto r = keys.noteOn (note, patch.priority);
    targetPitch = r.note;
    if (! patch.glideOn)
        pitch = targetPitch; // with GLIDE on, the pitch lags toward every new note (an RC on the keyboard voltage)
    if (r.retrigger)
    {
        const float v = std::clamp (velocity, 1, 127) / 127.0f;
        velocityGain = 1.0f - patch.velocity + patch.velocity * v;
        filterContour.trigger();
        loudness.trigger();
    }
    gate = true;
}

void Synth::noteOff (int note)
{
    if (note < 0 || note > 127)
        return;
    if (physicallyHeld[(size_t) note])
    {
        physicallyHeld[(size_t) note] = false;
        if (sustainDown)
        {
            sustained[(size_t) note] = true; // the pedal keeps it down
            return;
        }
    }
    const auto r = keys.noteOff (note, patch.priority);
    if (r.gateOn)
    {
        targetPitch = r.note; // low/high/last note takes over, without retriggering
        if (! patch.glideOn)
            pitch = targetPitch;
        return;
    }
    gate = false;
    filterContour.release();
    loudness.release();
}

void Synth::allNotesOff()
{
    keys.clear();
    physicallyHeld.fill (false);
    sustained.fill (false);
    gate = false;
    filterContour.release();
    loudness.release();
}

void Synth::process (float* out, int numSamples)
{
    if (! powered)
    {
        std::fill (out, out + numSamples, 0.0f);
        return;
    }
    const auto& p = patch;
    // per-block constants
    const double glideTau = p.glideOn ? mapping::glideSeconds (p.glide) : 0.0;
    const double glideCoef = glideTau > 0.0 ? std::exp (-1.0 / (glideTau * sampleRate)) : 0.0;
    const double fA = mapping::attackSeconds (p.filterAttack), fD = mapping::decaySeconds (p.filterDecay);
    const double fS = mapping::sustainLevel (p.filterSustain);
    const double lA = mapping::attackSeconds (p.loudAttack), lD = mapping::decaySeconds (p.loudDecay);
    const double lS = mapping::sustainLevel (p.loudSustain);
    const double fRel = p.decayOn ? fD : mapping::kShortRelease, lRel = p.decayOn ? lD : mapping::kShortRelease;
    const double tracking = mapping::keyboardTracking (p.keyboardControl1, p.keyboardControl2);
    const double baseCutoffOct = std::log2 (mapping::cutoffHz (p.cutoff));
    const double contourOct = mapping::contourOctaves (p.contourAmount);
    const double feedback = mapping::emphasisFeedback (p.emphasis);
    const float modMix = std::clamp (p.modMix, 0.0f, 10.0f) / 10.0f;
    const float wheel = modWheel * modWheel; // finer control at the low end
    std::array<double, 3> gains {};
    for (int i = 0; i < 3; ++i)
        gains[(size_t) i] = p.oscOn[(size_t) i] ? mapping::mixerGain (p.oscVolume[(size_t) i]) : 0.0;
    const double noiseGain = p.noiseOn ? mapping::mixerGain (p.noiseVolume) * 0.8 : 0.0;
    // EXT INPUT with nothing plugged in: the (slightly attenuated) main output comes back in
    const double extGain = p.extOn ? mapping::mixerGain (p.extVolume) * 2.2 : 0.0;
    const double outGain = p.outputOn ? mapping::outputGain (p.volume) : 0.0;
    const double bend = pitchBend * p.bendRange;
    const double driftAmt = std::clamp (p.drift, 0.0f, 1.0f);
    const double dcCoef = std::exp (-2.0 * 3.141592653589793 * 8.0 / sampleRate);

    for (int n = 0; n < numSamples; ++n)
    {
        // ---- keyboard pitch with glide (an RC lag, like the hardware)
        pitch = glideCoef > 0.0 ? targetPitch + (pitch - targetPitch) * glideCoef : targetPitch;

        // ---- modulation: oscillator 3 and noise, mixed, scaled by the mod wheel
        const float nz = p.pinkNoise ? noise.pink() : noise.white();
        const float modSource = lastOsc3 * (1.0f - modMix) + nz * modMix;
        const double pitchMod = p.oscModulation ? modSource * wheel * mapping::kPitchModSemitones : 0.0;
        const double filterMod = p.filterModulation ? modSource * wheel * mapping::kFilterModOctaves : 0.0;

        // ---- oscillators (3 first: it's also the modulation source)
        double mix = 0.0;
        for (int i = 2; i >= 0; --i)
        {
            const auto& o = p.osc[(size_t) i];
            const bool keyboard = i < 2 || p.osc3Control;
            double note = (keyboard ? pitch : mapping::kOsc3FreeNote) + p.tune + bend + 12.0 * mapping::rangeOctaves (o.range);
            if (i > 0)
                note += std::clamp (o.frequency, -7.0f, 7.0f);
            if (i < 2)
                note += pitchMod;
            note += osc[(size_t) i].drift (driftAmt);
            const double hz = mapping::noteToHz (note);
            lastOscHz[(size_t) i] = hz;
            const float v = osc[(size_t) i].process (hz, o.wave, i == 2);
            if (i == 2)
                lastOsc3 = v;
            mix += v * gains[(size_t) i];
        }
        mix += nz * noiseGain;
        // EXTERNAL INPUT: a cable in the jack, or (nothing plugged in) the main output fed back
        mix += (extInput != nullptr ? extInput[n] * 2.0f : lastOutput) * extGain;

        // ---- mixer stage: drives into soft saturation; the lamp lights when it's pushed hard
        if (std::abs (mix) > 1.8)
            overloadHold = (int) (sampleRate * 0.15);
        else if (overloadHold > 0)
            --overloadHold;
        // (+ a whisper of circuit noise, ~-100 dB: it's what lets a silent, fully emphasized filter
        // start to self-oscillate, as the analog one does)
        const float driven = (float) std::tanh (mix * 0.8) * 1.15f + nz * 1.0e-5f;

        // ---- filter
        const double fEnv = filterContour.process (fA, fD, fS, fRel);
        const double cutoffOct = baseCutoffOct + fEnv * contourOct + tracking * (pitch - mapping::kTrackingPivotNote) / 12.0 + filterMod;
        const double cutoff = std::clamp (std::exp2 (cutoffOct), 10.0, sampleRate * 0.45);
        lastCutoffHz = cutoff;
        float y = filter.process (driven, cutoff, feedback);
        y *= 1.0f + 0.3f * (float) feedback; // a little make-up gain: high emphasis thins the sound

        // ---- loudness contour (VCA)
        const double amp = loudness.process (lA, lD, lS, lRel) * velocityGain;
        double s = y * amp;

        // ---- output: AC-coupled, then the volume. The EXT INPUT loop takes the main output, so the
        // volume knob changes how hard the feedback drives the mixer (as on the hardware).
        dcState = s + (dcState - s) * dcCoef;
        s -= dcState;
        double o = s * outGain;
        lastOutput = (float) o;
        if (p.a440)
        {
            o += 0.3 * std::sin (a440Phase);
            a440Phase += 2.0 * 3.141592653589793 * 440.0 / sampleRate;
            if (a440Phase > 6.283185307179586)
                a440Phase -= 6.283185307179586;
        }
        out[n] = (float) o;
    }
}
} // namespace fourpole
