#pragma once

// Fourpole as an audio processor, for hosts (the Tabletop studio, or a plug-in host): the same engine and the
// same panel as the app, without an audio device of its own. Its jacks become the processor's buses:
//   input  0      EXTERNAL INPUT (AUDIO IN jack)
//   output 0      OUTPUT (mono)
//   MIDI in       MIDI IN jack (notes, pitch bend, mod wheel, sustain, panel CCs, program changes)

#include "../engine/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace fourpole
{
class FourpoleProcessor : public juce::AudioProcessor
{
public:
    FourpoleProcessor();
    ~FourpoleProcessor() override;

    /** The panel's natural size, for hosts that show it at a fixed size. */
    static constexpr int kPanelWidth = 1480, kPanelHeight = 860;

    Engine& getEngine() { return engine; }

    const juce::String getName() const override { return "Fourpole"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    /** The patch on the panel, the POWER switch and the EXTERNAL INPUT setting (JSON). */
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    Engine engine { true };
    juce::AudioBuffer<float> inputCopy;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourpoleProcessor)
};
} // namespace fourpole
