#include "ui/Theme.h"
#include "ui/Materials.h"
#include <map>

namespace fourpole::theme
{
juce::Font panelFont (float h, bool bold)
{
    static const auto names = juce::Font::findAllTypefaceNames();
    for (auto* n : { "Helvetica Neue", "Helvetica", "Arial" })
        if (names.contains (n))
            return juce::Font (juce::FontOptions (n, h, bold ? juce::Font::bold : juce::Font::plain));
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

void drawPrint (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float h, bool dim)
{
    g.setFont (panelFont (h));
    g.setColour ((dim ? colours::silkDim : colours::silk).withAlpha (0.94f));
    g.drawFittedText (text, r.toNearestInt(), j, 3, 0.85f);
}

PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel);
    setColour (juce::TextButton::textColourOffId, colours::silk);
    setColour (juce::TextButton::textColourOnId, colours::silk);
    setColour (juce::Label::textColourId, colours::silk);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202024));
    setColour (juce::PopupMenu::textColourId, colours::silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::rockerOrange.darker (0.3f));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202024));
    setColour (juce::ComboBox::textColourId, colours::silk);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a3e));
    setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff202024));
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff202024));
    setColour (juce::TextEditor::textColourId, colours::silk);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1d1d20));
    setColour (juce::AlertWindow::textColourId, colours::silk);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xfff4f1e6));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xff202020));
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    // a small black square-ish push button in a bezel
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.expanded (1.5f), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r.expanded (1.5f).translated (0, 0.8f), 3.0f, 0.8f);
    const float sc = materials::scaleOf (g);
    const int w = (int) std::ceil (r.getWidth() * sc), h = (int) std::ceil (r.getHeight() * sc);
    const auto& img = materials::cached ("btn:" + juce::String (w) + "x" + juce::String (h), [=] {
        materials::Shading sh { 0.40f, 0.62f, 0.25f, 22.0f, 0.02f, 0.05f, 1.2f };
        return materials::pillow (w, h, 2.5f * sc, juce::jmin (w, h) * 0.18f, 0.6f * sc, juce::Colour (0xff2b2b2e), sh, 5);
    });
    materials::drawAt (g, img, r.translated (0, down ? 1.0f : 0.0f));
    if (over && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, 2.5f);
    }
}

void PanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (colours::silk.withAlpha (b.isEnabled() ? 0.92f : 0.4f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 2).translated (0, down ? 1 : 0),
                      juce::Justification::centred, 1);
}

juce::Font PanelLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return panelFont (juce::jlimit (9.0f, 12.0f, h * 0.42f)); }

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                         juce::Slider& s)
{
    // knurled black knob; the scale is printed on the panel by the control around it
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto kb = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());
    const float angle = start + pos * (end - start);
    materials::roundShadow (g, kb.getCentre(), d * 0.47f, d * 0.24f);
    const float sc = materials::scaleOf (g);
    const int px = (int) std::ceil (d * sc);
    struct LastRender { float angle = -100.0f; int size = 0; juce::Image image; };
    static std::map<const juce::Slider*, LastRender> renders;
    auto& last = renders[&s];
    if (std::abs (last.angle - angle) > 0.002f || last.size != px)
        last = { angle, px, materials::knob (px, angle) };
    materials::drawAt (g, last.image, kb);
}

} // namespace fourpole::theme
