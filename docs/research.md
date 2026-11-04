# Research: the classic three-oscillator monosynth (Minimoog Model D)

This document collects what makes the Minimoog Model D work and sound the way it does. It's the reference for **Fourpole**, an original desktop instrument built for research that reproduces its architecture, controls and behaviour. Fourpole doesn't use Moog's names, logos, panel artwork or firmware. "Minimoog" and "Moog" are trademarks of Moog Music Inc. They're used here only to identify the instrument being studied.

Where sources disagree, usually the 1970s original versus the 2022 reissue, both are given.

## 1. What it is

- A **monophonic, fully analog, normalled (pre-wired) synthesizer**, released by R.A. Moog in 1970 and reissued in 2016 and 2022.
- **Normalled** means the signal path is fixed: three oscillators and noise go into a mixer, then a ladder filter, then a voltage-controlled amplifier. You shape sounds with knobs and switches rather than patch cables. That's why it became the first widely used performance synthesizer.
- The panel reads left to right in signal-flow order: **Controllers**, **Oscillator Bank**, **Mixer**, **Modifiers**, **Output**.
- The keyboard and two wheels sit below the panel, and the panel tilts up.

## 2. Keyboard and controllers

| Item | Behaviour |
|---|---|
| Keyboard | 44 keys, about 3½ octaves (the reissue lists F to C). |
| Note priority | **Low-note priority** on the original: the lowest held key sets the pitch of the oscillators and the filter's keyboard tracking. The 2022 reissue makes low, high or last note selectable. |
| Triggering | **Single trigger**: playing legato (a new key before the last is released) changes pitch without restarting the envelopes. The envelopes restart only when every key has been released first. |
| Glide | Portamento, continuously variable. The original manual gives 1 ms to 1 s per octave; the reissue lists 1 ms to 10 s. Switched on and off with the GLIDE rocker by the wheels. |
| Decay switch | A rocker by the wheels. On: when you let go of the key, the contours release at their DECAY time. Off: they release almost instantly (an emulation reference uses about 10 ms). This is the Model D's "release" control. |
| Pitch wheel | Bends all oscillators. It's sprung to centre on the reissue and unsprung (centre detent) on early units. The range is set inside the instrument; a few semitones up to a fifth is typical. |
| Modulation wheel | Sets **how much** of the modulation mix reaches its destinations. |
| Tune | Master fine tune for all oscillators, about ±2½ semitones. |
| Modulation mix | Blends the two modulation sources: **Oscillator 3** (periodic) and **Noise** (random). |
| Osc. modulation switch | Sends the modulation mix (scaled by the mod wheel) to the **pitch** of the oscillators: vibrato, trills. |
| Osc. 3 control switch | Off: Oscillator 3 stops following the keyboard. In its LO range it then becomes a free-running LFO, or a fixed drone in the higher ranges. |

## 3. Oscillator bank

| Item | Behaviour |
|---|---|
| Count | 3 voltage-controlled oscillators (VCOs). |
| Range | Six positions: **LO, 32′, 16′, 8′, 4′, 2′** (organ-stop footages, each an octave apart). Overall coverage is about 0.1 Hz to 20 kHz. **LO** drops the oscillator into the sub-audio range, used for LFO effects. |
| Osc. 1 waveforms | Triangle, **tri-saw** (a triangle/sawtooth blend), sawtooth, square, wide pulse, narrow pulse. |
| Osc. 2 waveforms | Same as Oscillator 1. |
| Osc. 3 waveforms | Triangle, **reverse sawtooth** (instead of tri-saw), sawtooth, square, wide pulse, narrow pulse. The reverse saw is the "ramp-up" LFO shape. |
| Frequency | Oscillators 2 and 3 have a FREQUENCY knob (about ±7 semitones) for detuning or tuning to intervals. Oscillator 1 has no tuning knob of its own; it's the reference. |
| Stability | Analog VCOs drift slightly; the reissue specifies short-term stability better than 0.25 %. That slow, uncorrelated wander between three oscillators is a big part of the "fat" sound when they're slightly detuned. |
| Phase | The oscillators run freely: no reset on key press, so every note starts at a different phase relationship. |

## 4. Mixer

- Five inputs, each with a **volume knob (0–10) and an on/off rocker**: Oscillator 1, Oscillator 2, Oscillator 3, External Input, and Noise.
- **Noise:** a switch selects **white or pink** noise.
- **Overload:** the mixer can be driven into **overload**, and an OVERLOAD lamp lights when it is. Setting oscillator levels above roughly 5–6 starts to drive the filter's input stage, which is one source of the Model D's warmth.
- **The feedback trick.** With nothing plugged into EXT INPUT, the reissue manual says *"the main audio output … is attenuated slightly and then normalled to the External Input signal path. In this case, the Main Output signal is sent back to the input of the mixer. By increasing the External Input volume far enough, the Mixer can overload, introducing varying levels of overdrive or distortion. When this occurs, the Overload indicator lamp will light."* The MAIN OUTPUT VOLUME also changes how much overload you get. On original units players did the same thing by patching the headphone or line output back into EXT INPUT. With high EMPHASIS this feedback loop can also self-oscillate.

## 5. Modifiers

### 5.1 Filter

- **The 24 dB/octave transistor ladder low-pass,** patented by Robert Moog in the 1960s. Four one-pole stages are built from the base-emitter behaviour of a ladder of bipolar transistors, inside a global feedback loop.
- **Cutoff frequency:** a large knob marked −5 to +5. The original sweeps roughly 40 Hz to 20 kHz; the reissue lists 10 Hz to 20 kHz.
- **Emphasis (resonance):** feedback around the ladder. At high settings the filter **self-oscillates**, producing a sine wave at the cutoff frequency (from the service manual).
- **Character:** the transistors' nonlinearity makes resonance soften and the passband thin out as emphasis rises. It also saturates gracefully when driven from the mixer.
- **Amount of contour:** how far the filter contour sweeps the cutoff, 0 to 4 octaves on the original.
- **Keyboard control 1 and 2:** two switches that make the cutoff track the keyboard. One is usually described as **⅓ tracking**, the other **⅔**, and both together give **full (1:1) tracking**. Full tracking lets a self-oscillating filter be played as an extra oscillator.
- **Filter modulation:** a switch that sends the modulation mix (scaled by the mod wheel) to the cutoff, for wah, growl and sample-and-hold-like noise effects.

### 5.2 Contours (envelopes)

- **Two:** the **filter contour** (it sweeps the cutoff by AMOUNT OF CONTOUR) and the **loudness contour** (it drives the VCA).
- **Three knobs each:** ATTACK TIME, DECAY TIME and SUSTAIN LEVEL.
- **Times:** attack and decay run from about 10 ms to 10 s on the original. The reissue lists attack 1 ms–10 s and decay 4 ms to more than 35 s. Sustain runs from 0 to 100 % of the contour's peak.
- **Release:** there's no separate release knob. The release is either the DECAY time (DECAY switch on) or very short (off).
- **Shape:** the segments are RC (exponential) curves, as on any analog envelope generator.

## 6. Output

- **Main output:** volume knob and on/off rocker, plus a headphone level and output.
- **A-440:** a switch that outputs a reference **A = 440 Hz** tone for tuning to other instruments.
- **Power** switch and pilot lamp.

## 7. The sounds people know it for

| Patch type | How it's made |
|---|---|
| Fat bass | Two or three sawtooth/square oscillators an octave apart and slightly detuned. Cutoff around −3, little emphasis, contour amount 2–4 with fast attack and a moderate decay. Decay switch often on. |
| Lead | Saws with a gentle detune and some glide. Filter half open with medium emphasis and keyboard control on, so higher notes stay bright. A touch of vibrato from Oscillator 3 (LO range, triangle) on the mod wheel. |
| Brass | A slow-ish filter attack (contour amount high), sustain around 6, sawtooth waves. |
| "Funk" bass | A square or narrow pulse with a very short decay and high emphasis: that "plucked" ladder sound. |
| Self-oscillating whistle | Emphasis at 10 and full keyboard tracking. The filter plays as a sine oscillator. |
| Overdriven | Mixer levels high and/or EXT INPUT feedback, for a growling, saturated tone. |

## 8. Implications for a digital reproduction

These are the decisions Fourpole is built on:

1. **Oscillators.** Use band-limited (polyBLEP) waveforms so high notes don't alias. Synthesize all six waveshapes per oscillator, including **tri-saw** and **reverse saw**. Each oscillator gets slow random pitch **drift** and a free-running phase.
2. **Filter.** A **nonlinear transistor-ladder model**: four tanh-saturated one-pole stages in a feedback loop, after Huovilainen (DAFx 2004), which D'Angelo and Välimäki describe as the best previous white-box model of the Moog ladder. Run it **oversampled** (2×) for stable self-oscillation and less aliasing. Cutoff tracks exponentially, so 1 V/octave becomes 1 octave per unit.
3. **Mixer.** A soft-saturating mixer stage with an overload indicator. **Feedback from the output into EXT INPUT** is modelled as a one-sample-delayed loop, so the overdrive trick works with nothing plugged in.
4. **Contours.** Model-D-style ADS with the decay switch: exponential RC segments, and attack and decay times on log scales from about 1–10 ms up to 10 s and beyond.
5. **Keyboard logic.** Low-note priority by default (high and last optional, as on the reissue), **single triggering** when playing legato, and glide in constant time per octave.
6. **Modulation.** The modulation mix of Oscillator 3 and noise, scaled by the mod wheel, goes to oscillator pitch and/or filter cutoff. Osc. 3 control off turns Oscillator 3 into a free LFO (in LO) or a drone.
7. **Panel fidelity.** Controls use panel units (0–10, −5 to +5, footages), so a patch sheet written for the hardware can be dialled in directly. Each knob maps to physical values through the ranges above.
8. **Additions the hardware didn't have,** offered as conveniences rather than changes to the sound:
   - Patch memory (the Model D has none).
   - MIDI note, pitch-bend and mod-wheel input, with controllers hot-pluggable.
   - Optional velocity.
   - A computer-keyboard and mouse-playable keyboard.

## Sources

- Model D specifications (oscillator ranges, waveforms, keyboard, note priority, stability): [vintagedigital.com.au listing](https://www.vintagedigital.com.au/?p=210662), [Andertons 2022 reissue](https://www.andertons.co.uk/moog/moog-minimoog-model-d-2022-reissue-extremely-limited-stock), [Woodbrass 2022 edition](https://www.woodbrass.com/en-gb/analog-synths-moog-minimoog-model-d-2022-edition-p370204.html), [Analogue Haven](https://analoguehaven.com/moog-music/minimoog-model-d)
- Contour and glide ranges, the decay switch: [Faug (open-source Model D emulation)](https://github.com/t2techno/Faug), [MusicRadar: Master the Minimoog Model D](https://www.musicradar.com/news/master-the-minimoog-model-d-create-classic-bass&quot), [Guitarguitar 2022 edition](https://www.guitarguitar.co.uk/product/240214419544027--moog-minimoog-model-d-2022-edition)
- Mixer overload and the external-input feedback normalling: [Moog forum: Model D overdrive trick](https://forum.moogmusic.com/t/minimoog-model-d-overdrive-trick/9412), [Moog forum: feed the output into the filter input](https://forum.moogmusic.com/t/minimoog-tip-feedback-the-output-into-the-filter-input/112), [Model D manual (manualshive)](https://manualshive.com/en/manual/moog/minimoog-model-d/1831668?page=27)
- Keyboard control, Osc. 3 control, modulation mix: [Minimoog operation manual (manualslib)](https://www.manualslib.com/manual/797384/Moog-Minimoog.html), [Product manual PDF](https://cf3.zzounds.com/media/Product_Manual-b228c1fe51c081d2ae8fa9d2c6adcaa7.pdf), [manualmachine user manual](https://manualmachine.com/moog/minimoogd/5413056-user-manual)
- Filter specs, emphasis self-oscillation, A-440: [Minimoog 204D service manual (lost-manuals)](https://www.lost-manuals.com/pdf/moog-music--minimoog-204d--service--ID9864.pdf), [Rubadub 2022 reissue](https://rubadub.co.uk/products/moog-model-d-re-issue)
- Ladder filter modelling: [Huovilainen, "Non-Linear Digital Implementation of the Moog Ladder Filter", DAFx 2004](https://dafx.de/paper-archive/2004/P_061.PDF), [ddiakopoulos/MoogLadders (collection of ladder models)](https://github.com/ddiakopoulos/moogladders), [Csound moogladder2](https://csound.com/manual/opcodes/moogladder2/), [Välimäki & Huovilainen, oscillator and filter algorithms for virtual analog](https://www.scribd.com/document/627673628/VAS-Valimaki2006)
