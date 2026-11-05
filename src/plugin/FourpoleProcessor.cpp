#include "FourpoleProcessor.h"
#include "../ui/MainComponent.h"

namespace fourpole
{
namespace
{
    class Editor : public juce::AudioProcessorEditor
    {
    public:
        explicit Editor (FourpoleProcessor& p) : juce::AudioProcessorEditor (p), panel (p.getEngine())
        {
            addAndMakeVisible (panel);
            setSize (FourpoleProcessor::kPanelWidth, FourpoleProcessor::kPanelHeight);
        }
        void resized() override { panel.setBounds (getLocalBounds()); }

    private:
        MainComponent panel;
    };
} // namespace

FourpoleProcessor::FourpoleProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("External In", juce::AudioChannelSet::mono(), true)
                                .withOutput ("Output", juce::AudioChannelSet::mono(), true))
{
}

FourpoleProcessor::~FourpoleProcessor() = default;

bool FourpoleProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
           && (layout.getMainInputChannelSet().isDisabled() || layout.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void FourpoleProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    inputCopy.setSize (1, juce::jmax (1, maxBlock));
    engine.prepareHosted (sampleRate, maxBlock);
}

void FourpoleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    const bool haveInput = getTotalNumInputChannels() > 0 && n <= inputCopy.getNumSamples();
    if (haveInput)
        inputCopy.copyFrom (0, 0, buffer, 0, 0, n); // (the input and output share channel 0)
    engine.processHosted (haveInput ? inputCopy.getReadPointer (0) : nullptr, buffer.getWritePointer (0), n, midi);
    for (int c = 1; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

juce::AudioProcessorEditor* FourpoleProcessor::createEditor() { return new Editor (*this); }

void FourpoleProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "fourpole-state");
    o->setProperty ("patch", engine.getPatch().toJson());
    o->setProperty ("power", engine.isPowered());
    o->setProperty ("externalInput", engine.isUsingExternalAudioInput());
    const auto text = juce::JSON::toString (juce::var (o), true);
    dest.replaceAll (text.toRawUTF8(), text.getNumBytesAsUTF8());
}

void FourpoleProcessor::setStateInformation (const void* data, int size)
{
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), size));
    if (json.getProperty ("format", {}).toString() != "fourpole-state")
        return;
    engine.setPatch (Patch::fromJson (json["patch"]));
    engine.setPower ((bool) json.getProperty ("power", true));
    engine.setExternalAudioInput ((bool) json.getProperty ("externalInput", false));
}
} // namespace fourpole
