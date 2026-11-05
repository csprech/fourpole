#pragma once

// Audio + MIDI around the core Synth.
//  * The patch is copied to the audio thread under a short SpinLock once per block when it changes.
//  * Notes, bends and the mod wheel from the on-screen keyboard and from MIDI controllers go through a
//    MidiMessageCollector, so they land sample-accurately inside the next block.
//  * MIDI inputs can be plugged in any time (all enabled inputs are heard; new ones are switched on).

#include "core/MidiMap.h"
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
    void setSustain (bool down);

    /** POWER switch: off silences the instrument. */
    void setPower (bool on) { power = on; }
    bool isPowered() const { return power.load(); }

    /** EXTERNAL INPUT source: false = nothing plugged in (the output feeds back, like the original),
        true = audio input 1 of the audio interface (opens the input; macOS asks for microphone access). */
    void setExternalAudioInput (bool useAudioInput);
    bool isUsingExternalAudioInput() const { return useInput.load(); }
    float getInputLevel() const { return inputLevel.load(); }

    /** Recording what you play to a WAV file (written on a background thread). */
    bool startRecording (const juce::File& file);
    juce::File stopRecording(); // returns the file, or none
    bool isRecording() const { return activeWriter.load() != nullptr; }
    double getRecordedSeconds() const { return recordedSamples.load() / juce::jmax (1.0, sampleRate.load()); }

    /** MIDI controller CCs for the panel (everything but mod wheel / sustain / all-notes-off),
        picked up on the message thread. Program changes come through here too, as
        cc == kProgramChange with the program number as the value. False when there are none left. */
    bool nextControllerMessage (int& cc, int& value);
    static constexpr int kProgramChange = 1000;

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
    void pushController (int cc, int value); // MIDI threads -> the message thread's fifo

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

    std::atomic<bool> power { true }, useInput { false };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<double> sampleRate { 48000.0 };

    // panel CCs from MIDI threads -> message thread
    juce::AbstractFifo ccFifo { 512 };
    std::array<std::pair<int, int>, 512> ccItems {};
    juce::CriticalSection ccWriteLock; // several MIDI inputs may call in at once

    // recording: ThreadedWriter keeps disk writes off the audio thread
    juce::TimeSliceThread writerThread { "Fourpole recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<juce::int64> recordedSamples { 0 };
    juce::File recordingFile;

    juce::Array<juce::MidiDeviceInfo> knownMidiInputs;
    juce::MidiDeviceListConnection midiDeviceListConnection;
};
} // namespace fourpole
