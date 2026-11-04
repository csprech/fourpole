#include "engine/Engine.h"

namespace fourpole
{
Engine::Engine()
{
    uiPatch = factoryPatches()[1]; // "Fat Bass": something good the moment you play
    pendingPatch = uiPatch;
    pendingPatch.name = {};

    if (auto* path = std::getenv ("FOURPOLE_CAPTURE_WAV"))
        captureFile = juce::File (path);
    deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (this);

    // MIDI in: one callback for every enabled input; switch on what's connected now and anything later
    deviceManager.addMidiInputDeviceCallback ({}, this);
    knownMidiInputs = juce::MidiInput::getAvailableDevices();
    for (auto& d : knownMidiInputs)
        deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
    midiDeviceListConnection = juce::MidiDeviceListConnection::make ([this] { midiDevicesChanged(); });
}

Engine::~Engine()
{
    midiDeviceListConnection.reset();
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    if (captureFile != juce::File() && captured.load() > 0)
    {
        captureFile.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (captureFile.createOutputStream().release());
        const auto opts = juce::AudioFormatWriterOptions().withSampleRate (captureRate).withNumChannels (1).withBitsPerSample (24);
        if (os != nullptr)
            if (auto writer = wav.createWriterFor (os, opts))
                writer->writeFromAudioSampleBuffer (capture, 0, captured.load());
    }
}

void Engine::setPatch (const Patch& p)
{
    uiPatch = p;
    auto forAudio = p;
    forAudio.name = {}; // no string the audio thread might end up freeing
    const juce::SpinLock::ScopedLockType sl (patchLock);
    pendingPatch = forAudio;
    patchChanged = true;
}

Patch Engine::getPatch() const { return uiPatch; }

void Engine::post (const juce::MidiMessage& m)
{
    auto msg = m;
    msg.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    collector.addMessageToQueue (msg);
}

void Engine::noteOn (int note, int velocity) { post (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity))); }
void Engine::noteOff (int note) { post (juce::MidiMessage::noteOff (1, note)); }
void Engine::setPitchBend (float b) { post (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) std::lround ((b + 1.0f) * 8191.5f)))); }
void Engine::setModWheel (float m) { post (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) std::lround (m * 127.0f)))); }
void Engine::allNotesOff() { post (juce::MidiMessage::allNotesOff (1)); }

void Engine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        midiKeys[(size_t) m.getNoteNumber()] = true;
    else if (m.isNoteOff())
        midiKeys[(size_t) m.getNoteNumber()] = false;
    else if (m.isPitchWheel())
        midiBend = (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isControllerOfType (1))
        midiMod = m.getControllerValue() / 127.0f;
    collector.addMessageToQueue (m);
}

void Engine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double sr = device->getCurrentSampleRate();
    collector.reset (sr);
    synth.prepare (sr);
    if (captureFile != juce::File())
    {
        captureRate = sr;
        capture.setSize (1, (int) (sr * 20.0));
        capture.clear();
        captured = 0;
    }
    patchChanged = true;
}

void Engine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOut, int numSamples,
                                               const juce::AudioIODeviceCallbackContext&)
{
    if (patchChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (patchLock);
        if (sl.isLocked())
        {
            synth.setPatch (pendingPatch);
            patchChanged = false;
        }
    }

    midiBlock.clear();
    collector.removeNextBlockOfMessages (midiBlock, numSamples);

    float* out = numOut > 0 ? outputs[0] : nullptr;
    if (out == nullptr)
        return;

    int pos = 0;
    auto renderTo = [&] (int end) {
        if (end > pos)
        {
            synth.process (out + pos, end - pos);
            pos = end;
        }
    };
    for (const auto meta : midiBlock)
    {
        renderTo (juce::jlimit (0, numSamples, meta.samplePosition));
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            synth.noteOn (m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff())
            synth.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())
            synth.setPitchBend ((m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isControllerOfType (1))
            synth.setModWheel (m.getControllerValue() / 127.0f);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            synth.allNotesOff();
    }
    renderTo (numSamples);

    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        out[i] = juce::jlimit (-1.0f, 1.0f, out[i]);
        peak = std::max (peak, std::abs (out[i]));
    }
    for (int c = 1; c < numOut; ++c)
        if (outputs[c] != nullptr)
            juce::FloatVectorOperations::copy (outputs[c], out, numSamples); // mono voice to every output
    if (capture.getNumSamples() > 0)
    {
        const int n = juce::jmin (numSamples, capture.getNumSamples() - captured.load());
        if (n > 0)
        {
            capture.copyFrom (0, captured.load(), out, n);
            captured += n;
        }
    }
    overloaded = synth.isOverloaded();
    outputLevel = peak;
}

void Engine::midiDevicesChanged()
{
    const auto now = juce::MidiInput::getAvailableDevices();
    auto contains = [] (const juce::Array<juce::MidiDeviceInfo>& list, const juce::MidiDeviceInfo& d) {
        for (auto& x : list)
            if (x.identifier == d.identifier)
                return true;
        return false;
    };
    juce::StringArray messages;
    for (auto& d : now)
        if (! contains (knownMidiInputs, d))
        {
            deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            messages.add ("MIDI IN: " + d.name + " connected");
        }
    for (auto& d : knownMidiInputs)
        if (! contains (now, d))
            messages.add ("MIDI IN: " + d.name + " disconnected");
    knownMidiInputs = now;
    if (onMidiDeviceMessage)
        for (auto& m : messages)
            onMidiDeviceMessage (m);
}

juce::StringArray Engine::getActiveMidiInputNames() const
{
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled (d.identifier))
            names.add (d.name);
    return names;
}
} // namespace fourpole
