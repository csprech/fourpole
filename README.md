# Fourpole

A desktop three-oscillator monophonic synthesizer built for research. It reproduces the architecture, controls and playing behaviour of the classic 1970 portable monosynth: three VCOs, a mixer, the 24 dB/octave transistor-ladder filter, two contours and a 44-key low-note-priority keyboard. It uses an original name and panel design; it doesn't use any manufacturer's names, logos, artwork or firmware.

C++20 / JUCE 8, macOS (Apple Silicon tested). The DSP core is platform-neutral and unit-tested.

![Fourpole playing the Overdrive Growl patch (OVERLOAD lamp lit)](docs/fourpole.png)

The research behind it, with sources, is in [docs/research.md](docs/research.md).

## Build & run

### Requirements (macOS)

- **Xcode Command Line Tools** (compiler and git): `xcode-select --install`
- **CMake 3.22+ and Ninja**, e.g. with [Homebrew](https://brew.sh): `brew install cmake ninja`

### Build

From the project folder, run these one at a time:

```sh
bash scripts/bootstrap.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
open build/Fourpole_artefacts/Release/Fourpole.app
```

- `bootstrap.sh` downloads JUCE 8.0.15 into `external/`. It isn't committed to git.
- The first build compiles JUCE too, so it takes a few minutes.
- To run the DSP tests: `./build/FourpoleTests_artefacts/Release/FourpoleTests`

## Playing it

- **Keyboard:** click the keys (drag for a glissando).
- **Computer keyboard:** `A W S E D F T G Y H U J K O L P ; '` play C up to F. `Z` / `X` move down / up an octave.
- **MIDI controller:** plug it in at any time, before or after starting Fourpole. Notes, pitch bend and the mod wheel work. Keys you play over MIDI light up on the screen keyboard.
- **Wheels:** PITCH springs back to the centre; MOD stays where you leave it.
- **Patches:** `<` `>` (or the arrow keys) step through the factory patches, then your own. **Save** / **Save as** write `.fourpole` files (plain JSON) to `~/Music/Fourpole Patches`. **Load** opens one from anywhere.
- **Setup:** conveniences the original didn't have:
  - pitch-wheel range;
  - note priority: low (the original), high or last;
  - velocity to loudness (off on the original);
  - oscillator drift: off, subtle or vintage.
- **Help** (or `?`): a quick guide.

## The panel

It reads left to right in signal-flow order, like the original:

| Section | Controls | Behaviour |
|---|---|---|
| **Controllers** | TUNE, GLIDE, MODULATION MIX, OSC. MODULATION, OSC. 3 CONTROL | Master tune (±2.5 semitones). Glide is an RC lag on the keyboard voltage, switched by GLIDE by the wheels. The modulation mix blends oscillator 3 (periodic) with noise (random), and the MOD wheel sets how much reaches its destinations. OSC. MODULATION sends it to pitch. OSC. 3 CONTROL off frees oscillator 3 from the keyboard: an LFO in its LO range, a drone otherwise. |
| **Oscillator bank** | RANGE, FREQUENCY, WAVEFORM ×3 | **Range:** LO, 32′, 16′, 8′, 4′, 2′ (octaves; LO is sub-audio). **Frequency:** ±7 semitones on oscillators 2 and 3; oscillator 1 is the reference. **Waveforms:** triangle, tri-saw, saw, square, wide pulse, narrow pulse. Oscillator 3 has a reverse saw instead of the tri-saw. All band-limited (polyBLEP), free-running (no phase reset), with gentle analog drift. |
| **Mixer** | levels and on/off for oscillators 1–3, EXTERNAL INPUT and NOISE (white or pink), OVERLOAD lamp | The mixer saturates softly; past roughly 6 on several sources it drives the filter, and the lamp lights. **EXTERNAL INPUT** has nothing plugged in, so it hears the main output, like the original. Turn it up for the feedback overdrive trick (VOLUME changes how hard it drives). |
| **Modifiers** | CUTOFF FREQUENCY (−5…+5, an octave per step), EMPHASIS, AMOUNT OF CONTOUR (0–4 octaves), KEYBOARD CONTROL 1 / 2 (⅓, ⅔, both = full tracking), FILTER MODULATION; filter and loudness contours (ATTACK, DECAY, SUSTAIN) | **Filter:** a nonlinear transistor-ladder model (4 saturating stages in a feedback loop, after Huovilainen, run at 2× oversampling). Emphasis at 10 self-oscillates at the cutoff frequency; with both keyboard-control switches on, the filter plays in tune as a sine oscillator. **Contours:** attack 1 ms–10 s, decay 4 ms–35 s, RC-shaped segments. There's no release knob: **DECAY** by the wheels on = notes release at the decay time, off = almost instantly. |
| **Output** | VOLUME, MAIN OUTPUT on/off, A-440 | A-440 adds a 440 Hz reference tone for tuning other instruments. |

**Keyboard behaviour:**
- 44 keys, F to C.
- **Low-note priority:** the lowest held key plays.
- **Single triggering:** playing legato changes the pitch without restarting the contours. They restart only when all keys have been released first.

## Factory patches

| Patch | What it shows |
|---|---|
| Init | A starting point. |
| Fat Bass | Three detuned oscillators an octave apart. |
| Plucked Funk | A narrow pulse with a short, resonant filter decay. |
| Lead Saw | Two saws, glide, oscillator 3 as a vibrato LFO on the mod wheel. |
| Brass | A slow filter attack. |
| Whistle (Self-Osc) | No oscillators: the filter itself, emphasis 10, full keyboard tracking. |
| Overdrive Growl | The EXTERNAL INPUT output-feedback trick. |
| Sub Wobble | Oscillator 3 LFO on the filter, via the mod wheel. |
| Noise Sweep | Noise through a resonant filter sweep. |
| Trill Lead | Oscillator 3 square LFO on pitch, for trills. |
| Tri-Saw Sweep Bass | The tri-saw waveform with a resonant sweep. |

## Architecture

```
src/core/     platform-neutral DSP (unit-tested, no GUI)
  Patch       the patch in panel units (0-10, -5..+5, footages), JSON, factory patches
  Mapping     panel units -> Hz / seconds / octaves (ranges from docs/research.md)
  Dsp         polyBLEP VCO with drift, nonlinear ladder filter, RC contours, white/pink noise,
              low/high/last-note key assigner with single triggering
  Synth       the voice: VCOs + noise + output feedback -> saturating mixer -> ladder -> VCA
src/engine/   Engine: audio + MIDI I/O (hot-plug MIDI inputs), sample-accurate note timing
src/ui/       panel: Materials (lit renderer for knobs, rockers, lamps, walnut), controls,
              44-key keyboard, MainComponent (layout, patches)
tests/        JUCE UnitTest suites for all of core/
```

**Real-time rules:**
- The audio thread doesn't allocate.
- Patches reach it under a short SpinLock, once per block when they change.
- Notes and controllers from the screen and from MIDI are merged and placed sample-accurately within each block.

**What the tests check (105 checks):**
- Every waveform hits its frequency; pulse waves have no DC.
- The ladder rolls off at 24 dB/octave (−73 dB three octaves above cutoff) and self-oscillates within 8% of the cutoff at full emphasis.
- Contour times and the DECAY switch.
- Low / high / last priority and single triggering.
- Glide time constants; keyboard tracking ⅓, ⅔ and full; 4 octaves of contour sweep.
- OSC. 3 CONTROL, vibrato, mixer overload, external-input feedback, A-440.
- Every factory patch round-trips through JSON and plays.

**Testing aid:** `FOURPOLE_CAPTURE_WAV=/path/out.wav open Fourpole.app` records the first 20 s of the app's output to a WAV when it quits. That's how the running app was checked: notes from the screen keyboard and from a MIDI controller plugged in after launch both came out at the right pitch.

## Not done yet

- Nobody has listened to it against a real instrument yet. The voice follows published measurements and circuit models, but the exact tone, filter curve and contour shapes haven't been compared with recordings of the hardware.
- Real external audio input (the EXTERNAL INPUT jack is always the output feedback here).
- MIDI CC mapping for the panel knobs, MIDI learn, MIDI out.
- Oscillator hard sync and other later-model additions; polyphony and paraphony (it's a monosynth by design).
- A plugin (AU/VST3) version.
