// The GPUI front end's way in: the engine and the JUCE panel, off-screen. The panel stays the controller (its
// knobs and rockers set the patch, with undo, MIDI learn and saving exactly as in the app) and renders the parts
// the GPUI window composites.

#include "FourpoleBridge.h"
#include "../src/engine/Engine.h"
#include "../src/ui/MainComponent.h"

namespace fourpole
{
struct GpuiBridge
{
    struct Part
    {
        juce::String kind, id;
        juce::Component* comp = nullptr;
        PanelKnob* knob = nullptr;
        Rocker* rocker = nullptr;
        Lamp* lamp = nullptr;
        Wheel* wheel = nullptr;
        Keyboard* keyboard = nullptr;
        float min = 0, max = 1;
        int steps = 0; // (selectors: positions)
    };

    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juce;
    std::unique_ptr<Engine> engine;
    std::unique_ptr<MainComponent> panel;
    std::vector<Part> parts;
    std::string parts_json, state_json;
    std::array<bool, 128> computerNotes {};

    GpuiBridge (int width, int height)
    {
        juce = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        engine = std::make_unique<Engine>();
        panel = std::make_unique<MainComponent> (*engine);
        panel->setSize (width, height);
        collectParts();
    }

    ~GpuiBridge()
    {
        if (panel != nullptr)
            panel->prepareToQuit();
        panel.reset();
        engine.reset();
    }

    void collectParts()
    {
        auto& mc = *panel;
        for (auto& [id, ref] : mc.controls)
        {
            Part p;
            p.id = id;
            if (ref.knob != nullptr)
            {
                p.comp = ref.knob;
                p.knob = ref.knob;
                p.kind = dynamic_cast<Selector*> (ref.knob) != nullptr ? "selector" : "knob";
                p.min = (float) ref.knob->slider.getMinimum();
                p.max = (float) ref.knob->slider.getMaximum();
                if (p.kind == "selector")
                    p.steps = juce::roundToInt (p.max - p.min) + 1;
            }
            else
            {
                p.comp = ref.rocker;
                p.rocker = ref.rocker;
                p.kind = "rocker";
            }
            parts.push_back (p);
        }
        auto add = [this] (const char* kind, const char* id, juce::Component& c) {
            Part p;
            p.kind = kind;
            p.id = id;
            p.comp = &c;
            p.lamp = dynamic_cast<Lamp*> (&c);
            p.wheel = dynamic_cast<Wheel*> (&c);
            p.keyboard = dynamic_cast<Keyboard*> (&c);
            if (p.wheel != nullptr)
                p.min = juce::String (id) == "pitchWheel" ? -1.0f : 0.0f;
            parts.push_back (p);
        };
        add ("lamp", "overload", mc.overload);
        add ("lamp", "pilot", mc.pilot);
        add ("wheel", "pitchWheel", mc.pitchWheel);
        add ("wheel", "modWheel", mc.modWheel);
        add ("keyboard", "keyboard", mc.keyboard);
        // (the power rocker isn't in the patch table: it's the panel's own)
        add ("rocker", "power", mc.powerSw);
        parts.back().rocker = &mc.powerSw;
    }

    // ---------------------------------------------------------------- drawing

    static void copyOut (const juce::Image& img, uint8_t* bgra, int w, int h)
    {
        // JUCE's pixels are premultiplied; GPUI's images aren't
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
        {
            const auto* src = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            auto* dst = bgra + (size_t) y * (size_t) w * 4;
            for (int x = 0; x < w; ++x)
            {
                auto px = src[x];
                px.unpremultiply();
                dst[x * 4 + 0] = px.getBlue();
                dst[x * 4 + 1] = px.getGreen();
                dst[x * 4 + 2] = px.getRed();
                dst[x * 4 + 3] = px.getAlpha();
            }
        }
    }

    /** Paints a component (and its children) into exactly w x h pixels at `scale`, from `origin` in its space. */
    static void paintInto (juce::Component& c, juce::Point<float> origin, float scale, uint8_t* bgra, int w, int h)
    {
        juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::translation (-origin.x, -origin.y).scaled (scale));
            c.paintEntireComponent (g, true);
        }
        copyOut (img, bgra, w, h);
    }

    int renderBackground (float scale, uint8_t* bgra, int w, int h)
    {
        std::vector<std::pair<juce::Component*, bool>> shown;
        for (auto* c : panel->getChildren())
        {
            shown.push_back ({ c, c->isVisible() });
            c->setVisible (false);
        }
        const auto keepMessage = panel->message;
        panel->message = " "; // (the name plate is drawn by the front end)
        paintInto (*panel, {}, scale, bgra, w, h);
        panel->message = keepMessage;
        for (auto& [c, v] : shown)
            c->setVisible (v);
        return 0;
    }

    int renderPart (int index, float value, float scale, uint8_t* bgra, int w, int h)
    {
        if (index < 0 || index >= (int) parts.size())
            return 1;
        auto& p = parts[(size_t) index];
        if (p.knob != nullptr)
        {
            const double keep = p.knob->slider.getValue();
            p.knob->slider.setValue (value, juce::dontSendNotification);
            paintInto (*p.comp, {}, scale, bgra, w, h);
            p.knob->slider.setValue (keep, juce::dontSendNotification);
        }
        else if (p.rocker != nullptr)
        {
            const bool keep = p.rocker->getToggleState();
            p.rocker->setToggleState (value > 0.5f, juce::dontSendNotification);
            paintInto (*p.comp, {}, scale, bgra, w, h);
            p.rocker->setToggleState (keep, juce::dontSendNotification);
        }
        else if (p.lamp != nullptr)
        {
            const bool keep = p.lamp->on;
            p.lamp->setOn (value > 0.5f);
            paintInto (*p.comp, {}, scale, bgra, w, h);
            p.lamp->setOn (keep);
        }
        else if (p.wheel != nullptr)
        {
            const float keep = p.wheel->getValue();
            p.wheel->value = value;
            paintInto (*p.comp, {}, scale, bgra, w, h);
            p.wheel->value = keep;
        }
        else if (p.keyboard != nullptr)
        {
            auto& kb = *p.keyboard;
            const auto keepComputer = kb.computerDown, keepExternal = kb.externalDown;
            const int keepMouse = kb.mouseNote;
            kb.computerDown.fill (false);
            kb.externalDown.fill (false);
            kb.mouseNote = -1;
            const int note = juce::roundToInt (value);
            if (note >= Keyboard::kLowest && note <= Keyboard::kHighest)
            {
                kb.computerDown[(size_t) note] = true;
                kb.background = {};
                paintInto (kb, keyRect (note).getTopLeft().toFloat(), scale, bgra, w, h);
            }
            else
            {
                kb.background = {};
                paintInto (kb, {}, scale, bgra, w, h);
            }
            kb.computerDown = keepComputer;
            kb.externalDown = keepExternal;
            kb.mouseNote = keepMouse;
            kb.background = {};
        }
        return 0;
    }

    static bool isBlackKey (int note) { return Keyboard::isBlack (note); }
    juce::Rectangle<int> keyRect (int note) const { return panel->keyboard.keyBounds (note).getSmallestIntegerContainer(); }

    // ---------------------------------------------------------------- the state

    const char* partsJson()
    {
        juce::Array<juce::var> list;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const auto& p = parts[i];
            auto* o = new juce::DynamicObject();
            const auto b = p.comp->getBounds();
            o->setProperty ("index", (int) i);
            o->setProperty ("kind", p.kind);
            o->setProperty ("id", p.id);
            o->setProperty ("x", b.getX());
            o->setProperty ("y", b.getY());
            o->setProperty ("w", b.getWidth());
            o->setProperty ("h", b.getHeight());
            o->setProperty ("min", p.min);
            o->setProperty ("max", p.max);
            o->setProperty ("steps", p.steps);
            if (const auto* def = panel::find (p.id))
                o->setProperty ("name", def->name);
            list.add (juce::var (o));
        }
        auto* root = new juce::DynamicObject();
        root->setProperty ("parts", list);
        auto rect = [] (juce::Rectangle<int> r) {
            auto* o = new juce::DynamicObject();
            o->setProperty ("x", r.getX());
            o->setProperty ("y", r.getY());
            o->setProperty ("w", r.getWidth());
            o->setProperty ("h", r.getHeight());
            return juce::var (o);
        };
        root->setProperty ("namePlate", rect (panel->namePlate));
        root->setProperty ("bar", rect (panel->barArea));
        juce::Array<juce::var> buttons;
        const std::pair<const char*, juce::Button*> btns[] = { { "prev", &panel->prevBtn }, { "next", &panel->nextBtn }, { "save", &panel->saveBtn },
                                                                { "saveAs", &panel->saveAsBtn }, { "load", &panel->loadBtn }, { "rec", &panel->recBtn },
                                                                { "setup", &panel->setupBtn }, { "audio", &panel->audioBtn }, { "help", &panel->helpBtn } };
        for (auto& [name, b] : btns)
        {
            auto v = rect (b->getBounds());
            v.getDynamicObject()->setProperty ("command", name);
            v.getDynamicObject()->setProperty ("label", b->getButtonText());
            buttons.add (v);
        }
        root->setProperty ("buttons", buttons);
        root->setProperty ("width", panel->getWidth());
        root->setProperty ("height", panel->getHeight());
        parts_json = juce::JSON::toString (juce::var (root), true).toStdString();
        return parts_json.c_str();
    }

    const char* stateJson()
    {
        auto& mc = *panel;
        auto* o = new juce::DynamicObject();
        const auto title = mc.patch.name + (mc.edited ? " *" : "") + (mc.currentFile.existsAsFile() ? juce::String() : juce::String ("  (factory)"));
        o->setProperty ("title", title);
        o->setProperty ("message", mc.message);
        o->setProperty ("overload", engine->isOverloaded());
        o->setProperty ("power", engine->isPowered());
        o->setProperty ("recording", engine->isRecording());
        o->setProperty ("pitchWheel", mc.pitchWheel.isBeingDragged() ? mc.pitchWheel.getValue() : engine->getMidiPitchBend());
        o->setProperty ("modWheel", mc.modWheel.getValue());
        o->setProperty ("level", engine->getOutputLevel());
        juce::Array<juce::var> held;
        for (int n = Keyboard::kLowest; n <= Keyboard::kHighest; ++n)
            if (computerNotes[(size_t) n] || engine->isMidiKeyDown (n))
                held.add (n);
        o->setProperty ("notes", held);
        juce::DynamicObject::Ptr values = new juce::DynamicObject();
        for (auto& p : parts)
        {
            if (p.knob != nullptr)
                values->setProperty (p.id, p.knob->slider.getValue());
            else if (p.rocker != nullptr)
                values->setProperty (p.id, p.rocker->getToggleState() ? 1 : 0);
        }
        o->setProperty ("values", juce::var (values.get()));
        state_json = juce::JSON::toString (juce::var (o), true).toStdString();
        return state_json.c_str();
    }

    Part* find (const char* id)
    {
        for (auto& p : parts)
            if (p.id == juce::String (id))
                return &p;
        return nullptr;
    }

    float get (const char* id)
    {
        if (auto* p = find (id))
        {
            if (p->knob != nullptr)
                return (float) p->knob->slider.getValue();
            if (p->rocker != nullptr)
                return p->rocker->getToggleState() ? 1.0f : 0.0f;
            if (p->wheel != nullptr)
                return p->wheel->getValue();
        }
        return 0.0f;
    }

    void set (const char* id, float v)
    {
        auto* p = find (id);
        if (p == nullptr)
            return;
        if (p->knob != nullptr)
            p->knob->slider.setValue (v, juce::sendNotificationSync); // (the panel's own path: undo, the patch, saving)
        else if (p->rocker != nullptr && p->rocker->getToggleState() != (v > 0.5f))
            p->rocker->setToggleState (v > 0.5f, juce::sendNotificationSync); // (as a click: the panel's own toggle)
        else if (p->wheel != nullptr)
        {
            p->wheel->setValue (v);
            if (p->wheel->onChange)
                p->wheel->onChange (p->wheel->getValue());
        }
    }

    void note (int n, int velocity, bool on)
    {
        if (n < 0 || n > 127)
            return;
        computerNotes[(size_t) n] = on;
        if (on)
            engine->noteOn (n, velocity);
        else
            engine->noteOff (n);
    }

    int keyEvent (const char* chars, bool down, bool command, bool shift)
    {
        auto& mc = *panel;
        const juce::String s = juce::String::fromUTF8 (chars).toLowerCase();
        if (s.isEmpty())
            return 0;
        const auto c = s[0];
        if (command)
        {
            if (! down)
                return 0;
            if (c == 's') { mc.savePatch (shift); return 1; }
            if (c == 'o') { mc.loadPatchFile(); return 1; }
            if (c == 'z') { shift ? mc.redo() : mc.undo(); return 1; }
            return 0;
        }
        static const char keys[18] = { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', ';', '\'' };
        for (int i = 0; i < 18; ++i)
            if (c == keys[i])
            {
                if (down == mc.keyHeld[(size_t) i])
                    return 1; // (key repeat)
                mc.keyHeld[(size_t) i] = down;
                if (down)
                    mc.keyNote[(size_t) i] = mc.octaveBase + i;
                note (mc.keyNote[(size_t) i], 100, down);
                return 1;
            }
        if (! down)
            return 0;
        if (c == 'z') { mc.octaveBase = juce::jmax (24, mc.octaveBase - 12); mc.flash ("Computer keyboard: C" + juce::String (mc.octaveBase / 12 - 1)); return 1; }
        if (c == 'x') { mc.octaveBase = juce::jmin (84, mc.octaveBase + 12); mc.flash ("Computer keyboard: C" + juce::String (mc.octaveBase / 12 - 1)); return 1; }
        if (s == "left") { mc.stepPatch (-1); return 1; }
        if (s == "right") { mc.stepPatch (1); return 1; }
        if (s == "?") { mc.showHelp(); return 1; }
        if (s == "escape")
        {
            for (int i = 0; i < 18; ++i)
                if (mc.keyHeld[(size_t) i])
                {
                    mc.keyHeld[(size_t) i] = false;
                    note (mc.keyNote[(size_t) i], 0, false);
                }
            computerNotes.fill (false);
            mc.panic();
            return 1;
        }
        return 0;
    }

    void command (const char* name)
    {
        auto& mc = *panel;
        const juce::String n (name);
        if (n == "prev") mc.stepPatch (-1);
        else if (n == "next") mc.stepPatch (1);
        else if (n == "save") mc.savePatch (false);
        else if (n == "saveAs") mc.savePatch (true);
        else if (n == "load") mc.loadPatchFile();
        else if (n == "setup") mc.showSetupMenu();
        else if (n == "audio") mc.showAudioSettings();
        else if (n == "help") mc.showHelp();
        else if (n == "rec") mc.toggleRecording();
    }
};
} // namespace fourpole

using fourpole::GpuiBridge;
struct FpPanel
{
    GpuiBridge b;
    FpPanel (int w, int h) : b (w, h) {}
};

extern "C" {
FpPanel* fp_create (int width, int height) { return new FpPanel (width, height); }
void fp_destroy (FpPanel* p) { delete p; }
const char* fp_parts_json (FpPanel* p) { return p->b.partsJson(); }
int fp_render_background (FpPanel* p, float scale, uint8_t* bgra, int w, int h) { return p->b.renderBackground (scale, bgra, w, h); }
int fp_render_part (FpPanel* p, int index, float value, float scale, uint8_t* bgra, int w, int h) { return p->b.renderPart (index, value, scale, bgra, w, h); }
void fp_key_rect (FpPanel* p, int note, float* x, float* y, float* w, float* h)
{
    const auto r = p->b.keyRect (note);
    *x = (float) r.getX();
    *y = (float) r.getY();
    *w = (float) r.getWidth();
    *h = (float) r.getHeight();
}
int fp_key_is_black (int note) { return GpuiBridge::isBlackKey (note) ? 1 : 0; }
float fp_get (FpPanel* p, const char* id) { return p->b.get (id); }
void fp_set (FpPanel* p, const char* id, float v) { p->b.set (id, v); }
void fp_note_on (FpPanel* p, int note, int velocity) { p->b.note (note, velocity, true); }
void fp_note_off (FpPanel* p, int note) { p->b.note (note, 0, false); }
void fp_set_wheel (FpPanel* p, int which, float v) { p->b.set (which == 0 ? "pitchWheel" : "modWheel", v); }
int fp_key_event (FpPanel* p, const char* chars, int down, int command, int shift) { return p->b.keyEvent (chars, down != 0, command != 0, shift != 0); }
const char* fp_state_json (FpPanel* p) { return p->b.stateJson(); }
void fp_command (FpPanel* p, const char* name) { p->b.command (name); }
}
