#pragma once

// Panel controls: knobs on printed scales, rotary selectors, rocker switches, the overload lamp,
// and the pitch / modulation wheels.

#include "ui/Theme.h"

namespace fourpole
{
/** A knob with its scale and name printed on the panel around it. */
class PanelKnob : public juce::Component
{
public:
    /** scaleLabels: printed numbers spread evenly from min to max (e.g. 0..10, -5..+5). */
    PanelKnob (const juce::String& name, double min, double max, double defaultValue, juce::StringArray scaleLabels, bool large = false);

    juce::Slider slider;
    std::function<void (double)> onChange;
    void setValue (double v) { slider.setValue (v, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    juce::Rectangle<float> knobBounds() const;
    juce::String name;
    juce::StringArray scale;
    bool large;
    static constexpr float kStart = -2.5f, kEnd = 2.5f; // radians either side of up
};

/** A rotary selector with labelled positions (footages, or waveform pictures). */
class Selector : public PanelKnob
{
public:
    enum class Kind { Range, Waveform, ReverseSawWaveform };
    Selector (const juce::String& name, Kind kind);
    int getIndex() const { return (int) slider.getValue(); }
    void paint (juce::Graphics&) override;

private:
    Kind kind;
};

/** A rocker switch with its function printed above it. */
class Rocker : public juce::Button
{
public:
    Rocker (const juce::String& label, juce::Colour cap, const juce::String& onText = "ON", const juce::String& offText = "OFF");
    void paintButton (juce::Graphics&, bool over, bool down) override;
    std::function<void (bool)> onToggle;

private:
    void clicked() override;
    juce::Colour capColour;
    juce::String onText, offText;
};

/** Indicator lamp (e.g. OVERLOAD). */
class Lamp : public juce::Component
{
public:
    explicit Lamp (const juce::String& text) : label (text) { setInterceptsMouseClicks (false, false); }
    void setOn (bool b) { if (b != on) { on = b; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    bool on = false;
};

/** Pitch or modulation wheel: a ribbed wheel in a slot; drag up/down. The pitch wheel springs back to centre. */
class Wheel : public juce::Component
{
public:
    Wheel (const juce::String& text, bool bipolarSpring) : label (text), spring (bipolarSpring) {}
    std::function<void (float)> onChange;
    void setValue (float v); // pitch -1..1, mod 0..1
    float getValue() const { return value; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isBeingDragged() const { return dragging; }

private:
    juce::Rectangle<float> slotBounds() const;
    juce::String label;
    bool spring;
    float value = 0.0f, dragStartValue = 0.0f;
    bool dragging = false;
};

} // namespace fourpole
