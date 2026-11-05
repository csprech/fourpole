#pragma once

// Panel controls: knobs on printed scales, rotary selectors, rocker switches, the overload lamp,
// and the pitch / modulation wheels.

#include "Theme.h"

namespace fourpole
{
/** A slider that reports right-clicks (for MIDI learn) instead of starting a drag. */
class KnobSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick)
        {
            rightClicked = true;
            return onRightClick();
        }
        rightClicked = false;
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! rightClicked) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! rightClicked) juce::Slider::mouseUp (e); }

private:
    bool rightClicked = false;
};

/** A knob with its scale and name printed on the panel around it. */
class PanelKnob : public juce::Component
{
public:
    /** scaleLabels: printed numbers spread evenly from min to max (e.g. 0..10, -5..+5). */
    PanelKnob (const juce::String& name, double min, double max, double defaultValue, juce::StringArray scaleLabels, bool large = false);

    KnobSlider slider;
    std::function<void (double)> onChange;
    const juce::String& getName() const { return name; }
    void setValue (double v) { slider.setValue (v, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    juce::Rectangle<float> knobBounds() const;
    /** The knob's shadow on the panel: drawn here, behind the knob, so it isn't clipped to the knob's square. */
    void drawKnobShadow (juce::Graphics&) const;
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
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        rightClicked = e.mods.isPopupMenu() && onRightClick;
        if (rightClicked)
            return onRightClick();
        juce::Button::mouseDown (e);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (! rightClicked) juce::Button::mouseUp (e); }

private:
    void clicked() override;
    juce::Colour capColour;
    juce::String onText, offText;
    bool rightClicked = false;
};

/** Indicator lamp (e.g. OVERLOAD). */
class Lamp : public juce::Component
{
    friend struct GpuiBridge;

public:
    explicit Lamp (const juce::String& text, juce::Colour c = theme::colours::lampRed) : label (text), colour (c)
    {
        setInterceptsMouseClicks (false, false);
    }
    void setOn (bool b) { if (b != on) { on = b; repaint(); } }
    /** Lens diameter in px. Give the component about 5x this so the glow isn't clipped. */
    void setLensSize (float d) { lens = d; repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    juce::Colour colour;
    float lens = 14.0f;
    bool on = false;
};

/** Pitch or modulation wheel: a ribbed wheel in a slot; drag up/down. The pitch wheel springs back to centre. */
class Wheel : public juce::Component
{
    friend struct GpuiBridge;

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
