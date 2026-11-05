#include "Dsp.h"
#include <algorithm>

namespace fourpole
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586;

    /** Polynomial band-limited step: smooths the jump of a discontinuity over one sample either side. */
    double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    uint32_t xorshift (uint32_t& s)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    double uniform (uint32_t& s) { return (xorshift (s) & 0xffffff) / 8388607.5 - 1.0; } // -1..1
} // namespace

// ------------------------------------------------------------------ oscillator

void Oscillator::prepare (double sr, uint32_t seed)
{
    sampleRate = sr > 0 ? sr : 48000.0;
    rng = seed * 2654435761u + 12345u;
    if (rng == 0)
        rng = 1;
    phase = (xorshift (rng) & 0xffff) / 65536.0; // free-running: each oscillator starts somewhere different
    driftState = driftTarget = 0.0;
    driftCounter = 0;
}

double Oscillator::drift (double amount)
{
    // a slow random walk: a new target every ~10 ms, smoothed with a ~0.3 s lag
    if (--driftCounter <= 0)
    {
        driftCounter = (int) (sampleRate * 0.01);
        driftTarget = juce::jlimit (-1.0, 1.0, driftTarget * 0.9 + uniform (rng) * 0.35);
    }
    driftState += (driftTarget - driftState) * (1.0 / (0.3 * sampleRate));
    return driftState * amount * 0.06; // up to about +/- 6 cents
}

float Oscillator::process (double hz, int wave, bool reverseSawSlot)
{
    const double dt = std::clamp (hz / sampleRate, 0.0, 0.45);
    const double t = phase;

    auto saw = [&] { return 2.0 * t - 1.0 - polyBlep (t, dt); };
    auto pulse = [&] (double width) {
        double v = t < width ? 1.0 : -1.0;
        v += polyBlep (t, dt);
        double t2 = t - width;
        if (t2 < 0.0)
            t2 += 1.0;
        v -= polyBlep (t2, dt);
        return v - (2.0 * width - 1.0); // AC-coupled: no DC
    };
    auto triangle = [&] { return t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t; };

    double out = 0.0;
    switch ((Wave) std::clamp (wave, 0, kNumWaves - 1))
    {
        case Wave::Triangle:    out = triangle(); break;
        case Wave::TriSaw:      out = reverseSawSlot ? -saw() : 0.5 * (triangle() + saw()); break;
        case Wave::Saw:         out = saw(); break;
        case Wave::Square:      out = pulse (0.5); break;
        case Wave::WidePulse:   out = pulse (0.32); break;
        case Wave::NarrowPulse: out = pulse (0.14); break;
    }

    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;
    return (float) out;
}

// ------------------------------------------------------------------ ladder

void LadderFilter::prepare (double sr)
{
    sampleRate = sr > 0 ? sr : 48000.0;
    reset();
}

void LadderFilter::reset()
{
    std::fill (std::begin (stage), std::end (stage), 0.0f);
    std::fill (std::begin (stageTanh), std::end (stageTanh), 0.0f);
    std::fill (std::begin (delay), std::end (delay), 0.0f);
}

float LadderFilter::tick (float input, float g, float k)
{
    // input minus the (half-sample averaged) output: the global feedback that makes the emphasis
    const float x = input - k * delay[5];
    delay[0] = stage[0] = delay[0] + g * (std::tanh (x) - stageTanh[0]);
    for (int i = 1; i < 4; ++i)
    {
        const float in = stage[i - 1];
        stageTanh[i - 1] = std::tanh (in);
        stage[i] = delay[i] + g * (stageTanh[i - 1] - (i != 3 ? stageTanh[i] : std::tanh (delay[i])));
        delay[i] = stage[i];
    }
    delay[5] = (stage[3] + delay[4]) * 0.5f;
    delay[4] = stage[3];
    return delay[5];
}

float LadderFilter::process (float input, double cutoffHz, double feedback)
{
    // Huovilainen's tuning and resonance corrections for the 2x-oversampled model
    const double fc = std::clamp (cutoffHz / sampleRate, 1.0e-5, 0.45);
    const double f = fc * 0.5;
    const double fc2 = fc * fc, fc3 = fc2 * fc;
    const double fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
    const double acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
    const float g = (float) (1.0 - std::exp (-kTwoPi * f * fcr));
    const float k = (float) (feedback * acr);
    tick (input, g, k);
    return tick (input, g, k);
}

// ------------------------------------------------------------------ contour

double Contour::process (double attack, double decay, double sustain, double releaseTime)
{
    auto coef = [this] (double tau) { return std::exp (-1.0 / (std::max (1.0e-5, tau) * sr)); };
    switch (stage)
    {
        case Stage::Idle:
            break;
        case Stage::Attack:
        {
            // an RC charging toward 1.3: reaches the peak (1.0) after `attack` seconds
            constexpr double overshoot = 1.3;
            const double c = coef (attack / 1.4663);
            level = overshoot + (level - overshoot) * c;
            if (level >= 1.0)
            {
                level = 1.0;
                stage = Stage::Decay;
            }
            break;
        }
        case Stage::Decay:
            // falls toward the sustain level; ~99 % of the way there after `decay` seconds
            level = sustain + (level - sustain) * coef (decay / 4.6);
            break;
        case Stage::Release:
            level *= coef (releaseTime / 4.6);
            if (level < 1.0e-5)
            {
                level = 0.0;
                stage = Stage::Idle;
            }
            break;
    }
    return level;
}

// ------------------------------------------------------------------ noise

float Noise::white() { return (float) uniform (state); }

float Noise::pink()
{
    const float w = white();
    b[0] = 0.99886f * b[0] + w * 0.0555179f;
    b[1] = 0.99332f * b[1] + w * 0.0750759f;
    b[2] = 0.96900f * b[2] + w * 0.1538520f;
    b[3] = 0.86650f * b[3] + w * 0.3104856f;
    b[4] = 0.55000f * b[4] + w * 0.5329522f;
    b[5] = -0.7616f * b[5] - w * 0.0168980f;
    const float p = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f;
    b[6] = w * 0.115926f;
    return p * 0.2f;
}

// ------------------------------------------------------------------ keyboard

int KeyAssigner::choose (NotePriority p) const
{
    int best = -1;
    for (int n = 0; n < 128; ++n)
    {
        if (! held[(size_t) n])
            continue;
        if (best < 0)
            best = n;
        else if (p == NotePriority::High)
            best = n; // keep the highest
        else if (p == NotePriority::Last && order[(size_t) n] > order[(size_t) best])
            best = n;
        // Low: the first one found stays
    }
    return best;
}

KeyAssigner::Result KeyAssigner::noteOn (int note, NotePriority p)
{
    Result r;
    if (note < 0 || note > 127)
        return r;
    if (! held[(size_t) note])
    {
        held[(size_t) note] = true;
        ++count;
    }
    order[(size_t) note] = ++counter;
    const int prev = current;
    current = choose (p);
    r.note = current;
    r.gateOn = true;
    r.retrigger = (count == 1); // single triggering: only the first key of a phrase restarts the contours
    r.pitchChanged = current != prev;
    return r;
}

KeyAssigner::Result KeyAssigner::noteOff (int note, NotePriority p)
{
    Result r;
    if (note >= 0 && note < 128 && held[(size_t) note])
    {
        held[(size_t) note] = false;
        --count;
    }
    const int prev = current;
    current = count > 0 ? choose (p) : -1;
    r.note = current;
    r.gateOn = count > 0;
    r.pitchChanged = current != prev && current >= 0;
    return r;
}

void KeyAssigner::clear()
{
    held.fill (false);
    count = 0;
    current = -1;
}

} // namespace fourpole
