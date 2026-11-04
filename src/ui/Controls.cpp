#include "ui/Controls.h"
#include "ui/Materials.h"

namespace fourpole
{
using namespace theme;

// ------------------------------------------------------------------ knob

PanelKnob::PanelKnob (const juce::String& n, double min, double max, double def, juce::StringArray s, bool big)
    : name (n), scale (std::move (s)), large (big)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (min, max, 0.0);
    slider.setValue (def, juce::dontSendNotification);
    slider.setDoubleClickReturnValue (true, def);
    slider.setRotaryParameters (juce::MathConstants<float>::twoPi + kStart, juce::MathConstants<float>::twoPi + kEnd, true);
    slider.setMouseDragSensitivity (large ? 320 : 220);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (true, true, nullptr); // the exact value, on hover and while turning
    slider.onValueChange = [this] {
        if (onChange)
            onChange (slider.getValue());
    };
    addAndMakeVisible (slider);
}

juce::Rectangle<float> PanelKnob::knobBounds() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (16.0f); // name
    const float d = std::floor (juce::jmin (r.getWidth(), r.getHeight()) * (large ? 0.66f : 0.58f));
    return juce::Rectangle<float> (d, d).withCentre (r.getCentre());
}

void PanelKnob::resized() { slider.setBounds (knobBounds().toNearestInt()); }

void PanelKnob::paint (juce::Graphics& g)
{
    const auto kb = knobBounds();
    const auto c = kb.getCentre();
    const float R = kb.getWidth() * 0.5f;
    // printed scale: ticks and numbers around the knob
    const int n = scale.size();
    const int ticks = juce::jmax (n, 2) * 2 - 1;
    g.setColour (colours::silk.withAlpha (0.85f));
    for (int i = 0; i < ticks; ++i)
    {
        const float a = kStart + (kEnd - kStart) * (float) i / (float) (ticks - 1);
        const bool major = i % 2 == 0;
        const float r0 = R * (major ? 1.10f : 1.13f), r1 = R * 1.24f;
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, major ? 1.3f : 0.8f);
    }
    g.setFont (panelFont (juce::jlimit (8.0f, 10.5f, R * 0.30f)));
    for (int i = 0; i < n; ++i)
    {
        const float a = kStart + (kEnd - kStart) * (float) i / (float) juce::jmax (1, n - 1);
        const float rr = R * 1.48f;
        auto box = juce::Rectangle<float> (22.0f, 11.0f).withCentre ({ c.x + std::sin (a) * rr, c.y - std::cos (a) * rr });
        g.drawText (scale[i], box, juce::Justification::centred, false);
    }
    drawPrint (g, name, getLocalBounds().toFloat().removeFromBottom (16.0f), juce::Justification::centred, 9.5f);
}

// ------------------------------------------------------------------ selector

namespace
{
    juce::StringArray positionsFor (Selector::Kind k)
    {
        if (k == Selector::Kind::Range)
            return { "LO", "32'", "16'", "8'", "4'", "2'" };
        return { "", "", "", "", "", "" };
    }

    /** Tiny printed waveform pictures for the WAVEFORM selector positions. */
    juce::Path waveGlyph (int index, bool reverseSaw, juce::Rectangle<float> r)
    {
        juce::Path p;
        const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom(), xm = r.getCentreX();
        switch (index)
        {
            case 0: p.startNewSubPath (x0, y1); p.lineTo (xm, y0); p.lineTo (x1, y1); break;               // triangle
            case 1:
                if (reverseSaw) { p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (x1, y1); p.lineTo (x1, y0); }  // reverse saw
                else { p.startNewSubPath (x0, y1); p.quadraticTo (xm - 1, y0 + 1, xm + 2, y0); p.lineTo (x1, y1); }      // tri-saw
                break;
            case 2: p.startNewSubPath (x0, y1); p.lineTo (x1, y0); p.lineTo (x1, y1); break;               // saw
            case 3: p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (xm, y0); p.lineTo (xm, y1); p.lineTo (x1, y1); p.lineTo (x1, y0); break;
            case 4: { const float w = x0 + (x1 - x0) * 0.32f;
                      p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (w, y0); p.lineTo (w, y1); p.lineTo (x1, y1); p.lineTo (x1, y0); break; }
            default: { const float w = x0 + (x1 - x0) * 0.14f;
                      p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (w, y0); p.lineTo (w, y1); p.lineTo (x1, y1); p.lineTo (x1, y0); break; }
        }
        return p;
    }
}

Selector::Selector (const juce::String& n, Kind k) : PanelKnob (n, 0, 5, k == Kind::Range ? 3 : 2, positionsFor (k)), kind (k)
{
    slider.setRange (0, 5, 1);
    slider.setMouseDragSensitivity (120);
    slider.setPopupDisplayEnabled (false, false, nullptr);
}

void Selector::paint (juce::Graphics& g)
{
    if (kind == Kind::Range)
        return PanelKnob::paint (g);
    // waveform pictures instead of numbers
    const auto kb = knobBounds();
    const auto c = kb.getCentre();
    const float R = kb.getWidth() * 0.5f;
    g.setColour (colours::silk.withAlpha (0.85f));
    for (int i = 0; i < 6; ++i)
    {
        const float a = kStart + (kEnd - kStart) * (float) i / 5.0f;
        g.drawLine (c.x + std::sin (a) * R * 1.10f, c.y - std::cos (a) * R * 1.10f, c.x + std::sin (a) * R * 1.24f,
                    c.y - std::cos (a) * R * 1.24f, 1.3f);
        const float rr = R * 1.52f;
        auto box = juce::Rectangle<float> (14.0f, 8.0f).withCentre ({ c.x + std::sin (a) * rr, c.y - std::cos (a) * rr });
        g.strokePath (waveGlyph (i, kind == Kind::ReverseSawWaveform, box), juce::PathStrokeType (1.2f));
    }
    drawPrint (g, name, getLocalBounds().toFloat().removeFromBottom (16.0f), juce::Justification::centred, 9.5f);
}

// ------------------------------------------------------------------ rocker

Rocker::Rocker (const juce::String& label, juce::Colour cap, const juce::String& on, const juce::String& off)
    : juce::Button (label), capColour (cap), onText (on), offText (off)
{
    setClickingTogglesState (true);
}

void Rocker::clicked()
{
    if (onToggle)
        onToggle (getToggleState());
}

void Rocker::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat();
    const auto label = getButtonText();
    if (label.isNotEmpty())
        drawPrint (g, label, r.removeFromTop (label.containsChar ('\n') ? 24.0f : 13.0f), juce::Justification::centred, 9.0f);
    // ON above, OFF below, printed beside the switch
    auto body = r.reduced (2.0f);
    const float capW = juce::jmin (body.getWidth() * 0.55f, 26.0f), capH = juce::jmin (body.getHeight() - 4.0f, capW * 1.55f);
    // centre the cap and its ON / OFF legend together, so the legend always fits
    const float textW = juce::jmax (juce::GlyphArrangement::getStringWidth (panelFont (7.5f, true), onText),
                                    juce::GlyphArrangement::getStringWidth (panelFont (7.5f, true), offText)) + 3.0f;
    const float groupW = capW + 2.0f + textW;
    auto cap = juce::Rectangle<float> (capW, capH).withCentre (body.getCentre())
                   .withX (std::round (body.getCentreX() - groupW * 0.5f));
    g.setFont (panelFont (7.5f));
    g.setColour (colours::silkDim);
    g.drawText (onText, juce::Rectangle<float> (cap.getRight() + 2.0f, cap.getY() - 1.0f, textW, 10.0f), juce::Justification::centredLeft);
    g.drawText (offText, juce::Rectangle<float> (cap.getRight() + 2.0f, cap.getBottom() - 9.0f, textW, 10.0f), juce::Justification::centredLeft);

    // the bezel cut into the panel
    auto bezel = cap.expanded (2.5f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bezel, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (bezel.translated (0, 0.8f), 2.5f, 0.8f);

    const float sc = materials::scaleOf (g);
    const int w = (int) std::ceil (cap.getWidth() * sc), h = (int) std::ceil (cap.getHeight() * sc);
    const auto& img = materials::cached ("rocker:" + juce::String (w) + "x" + juce::String (h) + capColour.toString(),
                                         [w, h, sc, colour = capColour] {
        materials::Shading sh { 0.42f, 0.62f, 0.32f, 26.0f, 0.02f, 0.04f, 1.2f };
        return materials::pillow (w, h, 2.0f * sc, juce::jmin (w, h) * 0.22f, 1.0f * sc, colour, sh, 13);
    });
    materials::drawAt (g, img, cap);
    // rocked: the pressed half sinks into the bezel (dark), the raised half stands up and catches the light
    const bool on = getToggleState();
    auto top = cap.withHeight (cap.getHeight() * 0.5f), bottom = cap.withTrimmedTop (cap.getHeight() * 0.5f);
    auto sunk = on ? top : bottom, raised = on ? bottom : top;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (sunk, 1.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.38f), raised.getCentreX(), on ? raised.getBottom() : raised.getY(),
                                             juce::Colours::white.withAlpha (0.0f), raised.getCentreX(), raised.getCentreY(), false));
    g.fillRect (raised);
    // the raised end's lip throws a short shadow across the hinge
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (juce::Rectangle<float> (cap.getX() + 1.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 2.0f, 2.0f));
    // the ON / OFF legend beside the active position lights up
    g.setFont (panelFont (7.5f, true));
    g.setColour (colours::silk);
    g.drawText (on ? onText : offText, juce::Rectangle<float> (cap.getRight() + 2.0f, on ? cap.getY() - 1.0f : cap.getBottom() - 9.0f, textW, 10.0f),
                juce::Justification::centredLeft);
    if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (cap, 2.0f);
    }
}

// ------------------------------------------------------------------ lamp

void Lamp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPrint (g, label, r.removeFromTop (12.0f), juce::Justification::centred, 8.5f);
    const float d = juce::jmin (r.getWidth(), r.getHeight()) * 0.55f;
    auto lens = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const float sc = materials::scaleOf (g);
    const int px = (int) std::ceil (lens.getWidth() * 1.4f * sc);
    auto dest = lens.expanded (lens.getWidth() * 0.2f);
    materials::drawAt (g, materials::cached ("lamp0:" + juce::String (px), [=] { return materials::led (px, colours::lampRed, false); }), dest);
    if (on)
    {
        materials::drawAt (g, materials::cached ("lamp1:" + juce::String (px), [=] { return materials::led (px, colours::lampRed, true); }), dest);
        juce::ColourGradient halo (colours::lampRed.withAlpha (0.55f), lens.getCentreX(), lens.getCentreY(), colours::lampRed.withAlpha (0.0f),
                                   lens.getCentreX() + d * 1.8f, lens.getCentreY(), true);
        g.setGradientFill (halo);
        g.fillEllipse (lens.expanded (d * 1.8f));
    }
}

// ------------------------------------------------------------------ wheel

juce::Rectangle<float> Wheel::slotBounds() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (16.0f);
    return r.withSizeKeepingCentre (juce::jmin (r.getWidth() * 0.62f, 30.0f), r.getHeight() - 6.0f);
}

void Wheel::setValue (float v)
{
    v = juce::jlimit (spring ? -1.0f : 0.0f, 1.0f, v);
    if (! juce::approximatelyEqual (v, value))
    {
        value = v;
        repaint();
    }
}

void Wheel::paint (juce::Graphics& g)
{
    auto slot = slotBounds();
    // the slot in the keyboard bed
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (slot.expanded (3.0f), 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (slot.expanded (3.0f).translated (0, 1.0f), 4.0f, 1.0f);

    // the wheel: a cylinder seen side-on; ribs move as it turns
    auto w = slot.reduced (2.0f, 1.0f);
    juce::ColourGradient body (juce::Colour (0xff1b1b1d), w.getX(), 0, juce::Colour (0xff1b1b1d), w.getRight(), 0, false);
    body.addColour (0.35, juce::Colour (0xff4a4a4e));
    body.addColour (0.55, juce::Colour (0xff2c2c2f));
    g.setGradientFill (body);
    g.fillRect (w);
    // shading top/bottom: the cylinder curves away
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.75f), 0, w.getY(), juce::Colours::transparentBlack, 0, w.getY() + w.getHeight() * 0.3f, false));
    g.fillRect (w);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0, w.getBottom() - w.getHeight() * 0.3f, juce::Colours::black.withAlpha (0.75f), 0, w.getBottom(), false));
    g.fillRect (w);
    const float pitch = 6.0f, travel = (spring ? (value + 1.0f) * 0.5f : value) * w.getHeight() * 0.9f;
    for (float y = std::fmod (travel, pitch); y < w.getHeight(); y += pitch)
    {
        // ribs bunch up toward the edges (foreshortening)
        const float t = (y / w.getHeight()) * 2.0f - 1.0f;
        const float yy = w.getCentreY() + std::sin (t * 1.2f) / std::sin (1.2f) * w.getHeight() * 0.5f;
        const float a = 0.55f * (1.0f - std::abs (t));
        g.setColour (juce::Colours::white.withAlpha (0.10f * a));
        g.drawHorizontalLine ((int) yy, w.getX(), w.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.6f * a));
        g.drawHorizontalLine ((int) yy + 1, w.getX(), w.getRight());
    }
    // centre mark on the pitch wheel
    if (spring)
    {
        const float my = w.getCentreY() - value * w.getHeight() * 0.42f;
        g.setColour (colours::silk.withAlpha (0.85f * (1.0f - std::abs (value) * 0.6f)));
        g.fillRect (juce::Rectangle<float> (w.getX() + 2.0f, my - 1.0f, w.getWidth() - 4.0f, 2.0f));
    }
    drawPrint (g, label, getLocalBounds().toFloat().removeFromBottom (16.0f), juce::Justification::centred, 9.0f);
}

void Wheel::mouseDown (const juce::MouseEvent&)
{
    dragging = true;
    dragStartValue = value;
}

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    const float range = slotBounds().getHeight() * (spring ? 0.5f : 1.0f);
    setValue (dragStartValue - e.getDistanceFromDragStartY() / range);
    if (onChange)
        onChange (value);
}

void Wheel::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (spring)
    {
        setValue (0.0f); // pitch wheel springs back to centre
        if (onChange)
            onChange (0.0f);
    }
}
} // namespace fourpole
