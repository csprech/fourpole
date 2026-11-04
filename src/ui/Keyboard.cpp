#include "ui/Keyboard.h"

namespace fourpole
{
bool Keyboard::isBlack (int note)
{
    const int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

juce::Rectangle<float> Keyboard::keyBounds (int note) const
{
    // count white keys from the lowest (F)
    int whiteIndex = 0;
    for (int n = kLowest; n < note; ++n)
        if (! isBlack (n))
            ++whiteIndex;
    int numWhite = 0;
    for (int n = kLowest; n <= kHighest; ++n)
        if (! isBlack (n))
            ++numWhite;
    const float ww = getWidth() / (float) numWhite, h = (float) getHeight();
    if (! isBlack (note))
        return { whiteIndex * ww, 0.0f, ww, h };
    const float bw = ww * 0.58f;
    // black keys sit between whites, nudged like a real keyboard (C# left, D# right, etc.)
    static constexpr float nudge[12] = { 0, -0.10f, 0, 0.10f, 0, 0, -0.12f, 0, 0, 0, 0.12f, 0 };
    return { whiteIndex * ww - bw * 0.5f + nudge[note % 12] * ww, 0.0f, bw, h * 0.62f };
}

bool Keyboard::isDown (int note) const
{
    return note == mouseNote || computerDown[(size_t) note] || externalDown[(size_t) note];
}

void Keyboard::setKeyDown (int note, bool down)
{
    if (note < 0 || note > 127 || computerDown[(size_t) note] == down)
        return;
    computerDown[(size_t) note] = down;
    if (note >= kLowest && note <= kHighest)
        repaint (keyBounds (note).expanded (2.0f).toNearestInt());
}

void Keyboard::refreshExternal()
{
    if (! isExternallyHeld)
        return;
    for (int n = kLowest; n <= kHighest; ++n)
    {
        const bool d = isExternallyHeld (n);
        if (d != externalDown[(size_t) n])
        {
            externalDown[(size_t) n] = d;
            repaint (keyBounds (n).expanded (2.0f).toNearestInt());
        }
    }
}

void Keyboard::drawKey (juce::Graphics& g, int note, bool down)
{
    auto r = keyBounds (note);
    if (! isBlack (note))
    {
        auto k = r.reduced (0.8f, 0.0f).withTrimmedBottom (down ? 1.0f : 3.0f);
        // ivory: warm, slightly yellowed, darker toward the back where it disappears under the rail
        juce::ColourGradient ivory (juce::Colour (0xffcfc8b6), 0, k.getY(), juce::Colour (0xfff2ecdd), 0, k.getY() + k.getHeight() * 0.25f, false);
        ivory.addColour (0.85, juce::Colour (0xffece5d4));
        ivory.addColour (1.0, juce::Colour (down ? 0xffcfc7b4 : 0xffd9d1bf));
        g.setGradientFill (ivory);
        g.fillRoundedRectangle (k, 2.5f);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.fillRoundedRectangle (k, 2.5f);
        }
        // the front lip of the key
        g.setColour (juce::Colours::black.withAlpha (down ? 0.18f : 0.28f));
        g.fillRect (k.withTop (k.getBottom() - (down ? 2.0f : 4.0f)));
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawVerticalLine ((int) r.getRight(), r.getY(), r.getBottom());
    }
    else
    {
        auto k = r.withTrimmedBottom (down ? 0.0f : 0.0f);
        // drop shadow on the whites
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (k.translated (2.0f, 2.0f), 2.0f);
        juce::ColourGradient ebony (juce::Colour (0xff050505), 0, k.getY(), juce::Colour (0xff1d1d1f), 0, k.getBottom(), false);
        g.setGradientFill (ebony);
        g.fillRoundedRectangle (k, 2.0f);
        // the top face and its sheen
        auto top = k.reduced (k.getWidth() * 0.12f, 0.0f).withTrimmedBottom (k.getHeight() * (down ? 0.05f : 0.12f));
        juce::ColourGradient face (juce::Colour (0xff262628), top.getX(), 0, juce::Colour (0xff3a3a3d), top.getRight(), 0, false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (top, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (down ? 0.06f : 0.14f));
        g.fillRect (top.withWidth (top.getWidth() * 0.35f).withTrimmedTop (4.0f).withTrimmedBottom (top.getHeight() * 0.3f));
    }
}

void Keyboard::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (getLocalBounds());
    for (int n = kLowest; n <= kHighest; ++n)
        if (! isBlack (n))
            drawKey (g, n, isDown (n));
    for (int n = kLowest; n <= kHighest; ++n)
        if (isBlack (n))
            drawKey (g, n, isDown (n));
    // shadow cast by the rail above the keys
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0, 0, juce::Colours::transparentBlack, 0, 16.0f, false));
    g.fillRect (getLocalBounds().withHeight (16));
}

int Keyboard::noteAt (juce::Point<float> p, int& velocity) const
{
    for (int n = kLowest; n <= kHighest; ++n)
        if (isBlack (n) && keyBounds (n).contains (p))
        {
            velocity = juce::jlimit (1, 127, (int) (40 + 87 * p.y / keyBounds (n).getHeight()));
            return n;
        }
    for (int n = kLowest; n <= kHighest; ++n)
        if (! isBlack (n) && keyBounds (n).contains (p))
        {
            velocity = juce::jlimit (1, 127, (int) (40 + 87 * p.y / (float) getHeight()));
            return n;
        }
    return -1;
}

void Keyboard::mouseDown (const juce::MouseEvent& e)
{
    int vel = 100;
    const int n = noteAt (e.position, vel);
    if (n < 0)
        return;
    mouseNote = n;
    if (onNoteOn)
        onNoteOn (n, vel);
    repaint();
}

void Keyboard::mouseDrag (const juce::MouseEvent& e)
{
    int vel = 100;
    const int n = noteAt (e.position, vel);
    if (n == mouseNote || n < 0)
        return;
    // glissando: the new key goes down before the old one comes up, so it plays legato
    const int old = mouseNote;
    mouseNote = n;
    if (onNoteOn)
        onNoteOn (n, vel);
    if (old >= 0 && onNoteOff)
        onNoteOff (old);
    repaint();
}

void Keyboard::mouseUp (const juce::MouseEvent&)
{
    if (mouseNote >= 0 && onNoteOff)
        onNoteOff (mouseNote);
    mouseNote = -1;
    repaint();
}
} // namespace fourpole
