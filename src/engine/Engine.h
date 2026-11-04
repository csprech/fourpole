#pragma once

// Audio + MIDI around the core Synth.
//  * The patch is copied to the audio thread under a short SpinLock once per block when it changes.
//  * Notes, bends and the mod wheel from the on-screen keyboard and from MIDI controllers go through a
//    MidiMessageCollector, so they land sample-accurately inside the next block.
//  * MIDI inputs can be plugged in any time (all enabled inputs are heard; new ones are switched on).

#include "core/Synth.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace fourpole
{
class Engine : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    Engine();
    ~Engine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    void setPatch (const Patch& p);
    Patch getPatch() const;

    // from the UI (message thread)
    void noteOn (int note, int velocity = 100);
    void noteOff (int note);
    void setPitchBend (float bend);  // -1..1
    void setModWheel (float amount); // 0..1
    void allNotesOff();

    // feedback for the UI
    bool isOverloaded() const { return overloaded.load(); }
    float getOutputLevel() const { return outputLevel.load(); }
    /** Keys held from MIDI input (so the on-screen keyboard can show them). */
    bool isMidiKeyDown (int note) const { return note >= 0 && note < 128 && midiKeys[(size_t) note].load(); }
    float getMidiPitchBend() const { return midiBend.load(); }
    float getMidiModWheel() const { return midiMod.load(); }
    float getCpuLoad() const { return (float) deviceManager.getCpuUsage(); }

    std::function<void (const juce::String&)> onMidiDeviceMessage;
    juce::StringArray getActiveMidiInputNames() const;

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numIn, float* const* outputs, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void midiDevicesChanged();
    void post (const juce::MidiMessage& m);

    juce::AudioDeviceManager deviceManager;
    juce::MidiMessageCollector collector;
    juce::MidiBuffer midiBlock;
    Synth synth;

    juce::SpinLock patchLock;
    Patch pendingPatch;
    std::atomic<bool> patchChanged { true };
    Patch uiPatch; // message thread copy

    std::atomic<bool> overloaded { false };
    std::atomic<float> outputLevel { 0.0f }, midiBend { 0.0f }, midiMod { 0.0f };
    std::array<std::atomic<bool>, 128> midiKeys {};

    // testing aid: FOURPOLE_CAPTURE_WAV=/path.wav records the first 20 s of output, written on exit
    juce::File captureFile;
    juce::AudioBuffer<float> capture;
    std::atomic<int> captured { 0 };
    double captureRate = 48000.0;

    juce::Array<juce::MidiDeviceInfo> knownMidiInputs;
    juce::MidiDeviceListConnection midiDeviceListConnection;
};
} // namespace fourpole
