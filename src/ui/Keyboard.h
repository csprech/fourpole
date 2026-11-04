#pragma once

// The 44-key keyboard (F to C, 3 1/2 octaves): play with the mouse (drag for a glissando), the
// computer keyboard, or MIDI. Keys held over MIDI are shown pressed too.

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

namespace fourpole
{
class Keyboard : public juce::Component
{
public:
    static constexpr int kLowest = 41, kHighest = 84; // F2 .. C6

    std::function<void (int note, int velocity)> onNoteOn;
    std::function<void (int note)> onNoteOff;
    /** Returns true for keys held elsewhere (MIDI), so they're drawn pressed. */
    std::function<bool (int note)> isExternallyHeld;

    void setKeyDown (int note, bool down); // computer keyboard
    void refreshExternal();                // call from a timer

    void paint (juce::Graphics&) override;
    void resized() override { background = {}; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    static bool isBlack (int note);
    juce::Rectangle<float> keyBounds (int note) const;
    int noteAt (juce::Point<float> p, int& velocity) const;
    bool isDown (int note) const;
    void drawKey (juce::Graphics&, int note, bool down);

    std::array<bool, 128> computerDown {}, externalDown {};
    int mouseNote = -1;
    juce::Image background;
};
} // namespace fourpole
