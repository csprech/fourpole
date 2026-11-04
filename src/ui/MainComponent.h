#pragma once

#include "engine/Engine.h"
#include "ui/Controls.h"
#include "ui/Keyboard.h"
#include "ui/Theme.h"

namespace fourpole
{
/** The whole instrument: walnut case, the tilted-up panel (controllers, oscillator bank, mixer,
    modifiers, output), the patch bar on the top rail, the wheels and the 44-key keyboard. */
class MainComponent : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent (Engine& engine);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    void timerCallback() override;
    void renderBackground (juce::Graphics&, float scale);
    void loadPatchIntoControls();
    void patchEdited();
    void flash (const juce::String& m);
    void stepPatch (int dir);
    void savePatch (bool askName);
    void loadPatchFile();
    void showSetupMenu();
    void showHelp();
    void showAudioSettings();
    juce::File patchFolder() const;
    void toggleRecording();
    void registerControl (const juce::String& id, PanelKnob& k);
    void registerControl (const juce::String& id, Rocker& r);
    void showControlMenu (const juce::String& id);
    void applyController (int cc, int value);
    void saveSettings();
    void loadSettings();
    juce::String controlName (const juce::String& id) const;
    void updateTooltips();

    Engine& engine;
    Patch patch;
    theme::PanelLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    // ---- controllers
    PanelKnob tune { "TUNE", -2.5, 2.5, 0.0, { "-", "", "0", "", "+" } };
    PanelKnob glide { "GLIDE", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob modMix { "MODULATION MIX", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    Rocker oscMod { "OSC.\nMODULATION", theme::colours::rockerOrange }, osc3Control { "OSC. 3\nCONTROL", theme::colours::rockerOrange };

    // ---- oscillator bank
    std::array<std::unique_ptr<Selector>, 3> range, wave;
    std::array<std::unique_ptr<PanelKnob>, 3> freq; // [0] unused: oscillator 1 has no frequency knob

    // ---- mixer
    std::array<std::unique_ptr<PanelKnob>, 3> oscVol;
    std::array<std::unique_ptr<Rocker>, 3> oscOn;
    PanelKnob extVol { "EXTERNAL INPUT", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob noiseVol { "NOISE", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    Rocker extOn { {}, theme::colours::rockerBlue }, noiseOn { {}, theme::colours::rockerBlue },
        pink { "NOISE", theme::colours::rockerBlue, "PINK", "WHITE" };
    Lamp overload { "OVERLOAD" };

    // ---- modifiers
    PanelKnob cutoff { "CUTOFF FREQUENCY", -5, 5, 0, { "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5" }, true };
    PanelKnob emphasis { "EMPHASIS", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob contourAmt { "AMOUNT OF CONTOUR", 0, 10, 5, { "0", "2", "4", "6", "8", "10" } };
    Rocker filterMod { "FILTER\nMODULATION", theme::colours::rockerOrange },
        kb1 { "KEYBOARD\nCONTROL 1", theme::colours::rockerOrange }, kb2 { "KEYBOARD\nCONTROL 2", theme::colours::rockerOrange };
    PanelKnob fAttack { "ATTACK TIME", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob fDecay { "DECAY TIME", 0, 10, 5, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob fSustain { "SUSTAIN LEVEL", 0, 10, 5, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob lAttack { "ATTACK TIME", 0, 10, 0, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob lDecay { "DECAY TIME", 0, 10, 5, { "0", "2", "4", "6", "8", "10" } };
    PanelKnob lSustain { "SUSTAIN LEVEL", 0, 10, 8, { "0", "2", "4", "6", "8", "10" } };

    // ---- output
    PanelKnob volume { "VOLUME", 0, 10, 6, { "0", "2", "4", "6", "8", "10" } };
    Rocker outputOn { "MAIN\nOUTPUT", theme::colours::rockerBlack }, a440 { "A-440", theme::colours::rockerWhite },
        powerSw { "POWER", theme::colours::rockerBlack };
    Lamp pilot { "", juce::Colour (0xffffb43a) };

    // ---- keyboard bed
    Rocker glideOn { "GLIDE", theme::colours::rockerWhite }, decayOn { "DECAY", theme::colours::rockerWhite };
    Wheel pitchWheel { "PITCH", true }, modWheel { "MOD.", false };
    Keyboard keyboard;

    // ---- patch bar
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, saveAsBtn { "Save as" }, loadBtn { "Load" },
        setupBtn { "Setup" }, audioBtn { "Audio/MIDI" }, helpBtn { "Help" }, recBtn { "Rec" };
    juce::Rectangle<int> barArea, namePlate, panelArea, bedArea, controllerBlock, keysArea, leftCheek, rightCheek, topRail, midRail,
        bottomRail, wordmarkArea;
    std::array<juce::Rectangle<int>, 5> sections;
    juce::String message;
    uint32_t messageUntil = 0;
    int patchIndex = 0;
    juce::File currentFile;

    std::unique_ptr<juce::FileChooser> chooser;
    int octaveBase = 48; // computer keyboard: A = C3
    std::array<bool, 18> keyHeld {};
    std::array<int, 18> keyNote {};
    bool loading = false;
    juce::Image background;

    // MIDI learn: panel controls by ID, the CC map, and the control waiting to learn a CC
    struct ControlRef
    {
        PanelKnob* knob = nullptr;
        Rocker* rocker = nullptr;
    };
    std::map<juce::String, ControlRef> controls;
    MidiMap midiMap;
    juce::String learnTarget;
    std::unique_ptr<juce::PropertiesFile> settings;
};
} // namespace fourpole
