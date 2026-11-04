#pragma once

// Look: a 1970s portable monosynth. Walnut end cheeks and keyboard rail, a satin black aluminium
// panel with white silkscreen, black knurled knobs on printed scales, coloured rocker switches.

#include <juce_gui_basics/juce_gui_basics.h>

namespace fourpole::theme
{
namespace colours
{
    const juce::Colour panel      { 0xff17171a };
    const juce::Colour silk       { 0xffeeeae0 }; // white panel print
    const juce::Colour silkDim    { 0xffa9a59b };
    const juce::Colour rockerBlue { 0xff3f6fa8 };
    const juce::Colour rockerOrange { 0xffd8712b };
    const juce::Colour rockerWhite { 0xffe8e4d8 };
    const juce::Colour rockerBlack { 0xff2a2a2c };
    const juce::Colour lampRed    { 0xffff3a2c };
    const juce::Colour plate      { 0xff0e0e10 };
}

/** Panel print: a bold grotesque, like the period's Helvetica-style silkscreens. */
juce::Font panelFont (float height, bool bold = true);
/** Silkscreen text on the black panel. */
void drawPrint (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r,
                juce::Justification j = juce::Justification::centred, float height = 10.5f, bool dim = false);

/** Look-and-feel for the panel's small buttons (patch bar) and dialogs. */
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override {}
};

} // namespace fourpole::theme
