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

void PanelKnob::drawKnobShadow (juce::Graphics& g) const
{
    const auto kb = knobBounds();
    // the knob stands about half its radius proud of the panel; the light is top-left
    materials::roundShadow (g, kb.getCentre(), kb.getWidth() * 0.47f, kb.getWidth() * 0.21f);
}

void PanelKnob::paint (juce::Graphics& g)
{
    drawKnobShadow (g);
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
    drawKnobShadow (g);
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
    if (label.isNotEmpty())
        drawPrint (g, label, r.removeFromTop (12.0f), juce::Justification::centred, 8.5f);
    const float d = juce::jmin (lens, r.getWidth() * 0.4f, r.getHeight() * 0.4f);
    auto lensR = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    // the glow: as big as the component allows, so its edge always fades out before it's clipped
    const float glow = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    if (on)
    {
        juce::ColourGradient halo (colour.withAlpha (0.5f), lensR.getCentreX(), lensR.getCentreY(), colour.withAlpha (0.0f),
                                   lensR.getCentreX() + glow, lensR.getCentreY(), true);
        halo.addColour (0.3, colour.withAlpha (0.16f));
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (glow * 2.0f, glow * 2.0f).withCentre (lensR.getCentre()));
    }
    // a chrome bezel ring around the jewel
    g.setColour (juce::Colour (0xff8a8a8e));
    g.drawEllipse (lensR.expanded (2.2f), 1.6f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (lensR.expanded (3.4f), 0.8f);
    const float sc = materials::scaleOf (g);
    const int px = (int) std::ceil (lensR.getWidth() * 1.4f * sc);
    auto dest = lensR.expanded (lensR.getWidth() * 0.2f);
    const auto key = juce::String (px) + colour.toString();
    materials::drawAt (g, materials::cached ("lamp0:" + key, [px, c = colour] { return materials::led (px, c, false); }), dest);
    if (on)
        materials::drawAt (g, materials::cached ("lamp1:" + key, [px, c = colour] { return materials::led (px, c, true); }), dest);
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
    // the slot in the keyboard bed: dark, with the lit far edge and an inner shadow
    auto well = slot.expanded (4.0f, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (well.translated (0, 1.0f), 5.0f, 1.0f);
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (well, 5.0f);

    // the wheel is a cylinder turning about a horizontal axis: we see its curved face from above
    auto w = slot.reduced (1.5f, 0.0f);
    const float h = w.getHeight();
    auto shadeAt = [] (float t) {
        // t: -1 (top edge) .. 1 (bottom edge); light from above and slightly behind
        const float nz = std::sqrt (std::max (0.0f, 1.0f - t * t)); // surface normal facing us
        const float lit = 0.25f + 0.75f * std::max (0.0f, 0.55f * nz - 0.45f * t);
        return lit;
    };
    const int bands = (int) h;
    for (int i = 0; i < bands; ++i)
    {
        const float t = (i + 0.5f) / (float) bands * 2.0f - 1.0f;
        const float l = shadeAt (t);
        // satin rubber: a broad sheen where the surface faces the light (around a third of the way down)
        const float sheen = 0.20f * std::exp (-std::pow ((t + 0.35f) / 0.22f, 2.0f));
        const float v = 0.06f + 0.30f * l + sheen;
        g.setColour (juce::Colour::fromFloatRGBA (v, v, v * 1.03f, 1.0f));
        g.fillRect (juce::Rectangle<float> (w.getX(), w.getY() + (float) i, w.getWidth(), 1.0f));
    }
    // ribs around the circumference: they roll with the wheel and bunch up toward the edges
    const float turn = (spring ? value * 0.5f : value - 0.5f) * 2.6f; // radians of rotation shown
    const int ribs = 28;
    for (int k = 0; k < ribs; ++k)
    {
        const float phi = std::fmod ((float) k / ribs * juce::MathConstants<float>::twoPi - turn + 100.0f * juce::MathConstants<float>::twoPi,
                                     juce::MathConstants<float>::twoPi) - juce::MathConstants<float>::pi;
        if (std::abs (phi) > 1.35f)
            continue; // round the back
        const float y = w.getCentreY() + std::sin (phi) * h * 0.5f;
        const float facing = std::cos (phi);
        // each rib: a dark groove with a lit ridge just behind it
        g.setColour (juce::Colours::black.withAlpha (0.65f * facing));
        g.fillRect (juce::Rectangle<float> (w.getX() + 1.0f, y - 0.8f, w.getWidth() - 2.0f, 1.6f * facing + 0.4f));
        g.setColour (juce::Colours::white.withAlpha (0.16f * facing * facing));
        g.fillRect (juce::Rectangle<float> (w.getX() + 1.0f, y + 1.0f, w.getWidth() - 2.0f, 1.0f));
    }
    // the side walls of the slot shade the wheel's edges
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), w.getX(), 0, juce::Colours::transparentBlack, w.getX() + 6.0f, 0, false));
    g.fillRect (w.withWidth (6.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, w.getRight() - 6.0f, 0, juce::Colours::black.withAlpha (0.55f), w.getRight(), 0, false));
    g.fillRect (w.withTrimmedLeft (w.getWidth() - 6.0f));

    // the pitch wheel's white centre line rolls with it
    if (spring)
    {
        const float phi = value * 1.3f;
        const float my = w.getCentreY() - std::sin (phi) * h * 0.5f;
        g.setColour (theme::colours::silk.withAlpha (0.9f * std::cos (phi)));
        g.fillRect (juce::Rectangle<float> (w.getX() + 2.0f, my - 1.2f, w.getWidth() - 4.0f, 2.4f));
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
