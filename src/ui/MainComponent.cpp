#include "ui/MainComponent.h"
#include "ui/Materials.h"
#include "core/Mapping.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace fourpole
{
namespace
{
// computer keyboard: a piano layout on the home row (A = C), W E T Y U O P = black keys
constexpr char kKeys[18] = { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', ';', '\'' };
const juce::StringArray kTen { "0", "2", "4", "6", "8", "10" };

juce::String formatSeconds (double s)
{
    if (s < 1.0)
        return (s < 0.1 ? juce::String (s * 1000.0, 1) : juce::String (juce::roundToInt (s * 1000.0))) + " ms";
    return juce::String (s, s < 10.0 ? 2 : 1) + " s";
}

juce::String formatHz (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";
    return (hz < 10.0 ? juce::String (hz, 2) : hz < 100.0 ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz))) + " Hz";
}
} // namespace

MainComponent::MainComponent (Engine& e) : engine (e)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);
    patch = engine.getPatch();
    for (size_t i = 0; i < factoryPatches().size(); ++i)
        if (factoryPatches()[i].name == patch.name)
            patchIndex = (int) i;

    // ---- build the oscillator bank and mixer rows
    for (int i = 0; i < 3; ++i)
    {
        range[(size_t) i] = std::make_unique<Selector> ("RANGE", Selector::Kind::Range);
        wave[(size_t) i] = std::make_unique<Selector> ("WAVEFORM", i == 2 ? Selector::Kind::ReverseSawWaveform : Selector::Kind::Waveform);
        if (i > 0)
            freq[(size_t) i] = std::make_unique<PanelKnob> ("FREQUENCY", -7, 7, 0, juce::StringArray { "-7", "", "", "0", "", "", "+7" });
        oscVol[(size_t) i] = std::make_unique<PanelKnob> ("OSCILLATOR " + juce::String (i + 1), 0, 10, 0, kTen);
        oscOn[(size_t) i] = std::make_unique<Rocker> (juce::String(), theme::colours::rockerBlue);
    }

    // ---- bind every control to its patch field, by ID (the panel table: also the MIDI learn names)
    bind ("tune", tune);
    bind ("glide", glide);
    bind ("modMix", modMix);
    bind ("oscModulation", oscMod);
    bind ("osc3Control", osc3Control);
    for (int i = 0; i < 3; ++i)
    {
        const auto n = "osc" + juce::String (i + 1);
        bind (n + ".range", *range[(size_t) i]);
        bind (n + ".wave", *wave[(size_t) i]);
        if (freq[(size_t) i])
            bind (n + ".frequency", *freq[(size_t) i]);
        bind (n + ".volume", *oscVol[(size_t) i]);
        bind (n + ".on", *oscOn[(size_t) i]);
    }
    bind ("extVolume", extVol);
    bind ("extOn", extOn);
    bind ("noiseVolume", noiseVol);
    bind ("noiseOn", noiseOn);
    bind ("pinkNoise", pink);
    bind ("cutoff", cutoff);
    bind ("emphasis", emphasis);
    bind ("contourAmount", contourAmt);
    bind ("filterModulation", filterMod);
    bind ("keyboardControl1", kb1);
    bind ("keyboardControl2", kb2);
    bind ("filterAttack", fAttack);
    bind ("filterDecay", fDecay);
    bind ("filterSustain", fSustain);
    bind ("loudAttack", lAttack);
    bind ("loudDecay", lDecay);
    bind ("loudSustain", lSustain);
    bind ("volume", volume);
    bind ("outputOn", outputOn);
    bind ("a440", a440);
    bind ("glideOn", glideOn);
    bind ("decayOn", decayOn);
    jassert (controls.size() == panel::controls().size()); // every control in the table has its widget
    addAndMakeVisible (overload);
    extOn.setTooltip ("With nothing plugged in, EXTERNAL INPUT hears the main output: turn it up for feedback overdrive");
    kb1.setTooltip ("Filter follows the keyboard: 1/3 (with 2: full tracking)");
    kb2.setTooltip ("Filter follows the keyboard: 2/3 (with 1: full tracking)");
    osc3Control.setTooltip ("Off: oscillator 3 ignores the keyboard and its FREQUENCY knob spans +/-3 octaves (an LFO in LO)");
    decayOn.setTooltip ("On: notes release at the contours' DECAY time. Off: they stop almost at once");

    // ---- wheels and keyboard
    pitchWheel.onChange = [this] (float v) { engine.setPitchBend (v); };
    modWheel.onChange = [this] (float v) { engine.setModWheel (v); };
    addAndMakeVisible (pitchWheel);
    addAndMakeVisible (modWheel);
    keyboard.onNoteOn = [this] (int n, int v) { engine.noteOn (n, v); };
    keyboard.onNoteOff = [this] (int n) { engine.noteOff (n); };
    keyboard.isExternallyHeld = [this] (int n) { return engine.isMidiKeyDown (n); };
    addAndMakeVisible (keyboard);

    // ---- patch bar
    prevBtn.onClick = [this] { stepPatch (-1); };
    nextBtn.onClick = [this] { stepPatch (1); };
    saveBtn.onClick = [this] { savePatch (false); };
    saveAsBtn.onClick = [this] { savePatch (true); };
    loadBtn.onClick = [this] { loadPatchFile(); };
    setupBtn.onClick = [this] { showSetupMenu(); };
    audioBtn.onClick = [this] { showAudioSettings(); };
    helpBtn.onClick = [this] { showHelp(); };
    recBtn.setClickingTogglesState (false);
    recBtn.setTooltip ("Record what you play to a WAV file (Music/Fourpole Recordings)");
    recBtn.onClick = [this] { toggleRecording(); };
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &saveAsBtn, &loadBtn, &setupBtn, &audioBtn, &helpBtn, &recBtn })
        addAndMakeVisible (b);

    engine.onMidiDeviceMessage = [this] (const juce::String& m) { flash (m); };

    // ---- POWER and its pilot lamp (not part of the patch, like the real switch)
    powerSw.setToggleState (true, juce::dontSendNotification);
    powerSw.onToggle = [this] (bool on) {
        engine.setPower (on);
        pilot.setOn (on);
        flash (on ? "Power on" : "Power off");
    };
    pilot.setOn (true);
    pilot.setLensSize (10.0f);
    overload.setLensSize (13.0f);
    addAndMakeVisible (powerSw);
    addAndMakeVisible (pilot);

    // ---- readouts in real units (hover a knob, or turn it)
    auto unitText = [] (PanelKnob& k, std::function<juce::String (double)> f) { k.slider.textFromValueFunction = std::move (f); };
    unitText (tune, [] (double v) { return (v >= 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0)) + " cents"; });
    unitText (glide, [] (double v) { return v <= 0.0 ? juce::String ("off") : formatSeconds (mapping::glideSeconds ((float) v)); });
    unitText (modMix, [] (double v) { return juce::String (juce::roundToInt (100.0 - v * 10.0)) + "% osc 3 / " + juce::String (juce::roundToInt (v * 10.0)) + "% noise"; });
    for (int i = 0; i < 3; ++i)
    {
        range[(size_t) i]->slider.textFromValueFunction = [] (double v) {
            static const char* names[] = { "LO (sub-audio)", "32'", "16'", "8'", "4'", "2'" };
            return juce::String (names[juce::jlimit (0, 5, (int) v)]);
        };
        wave[(size_t) i]->slider.textFromValueFunction = [i] (double v) {
            static const char* names[] = { "triangle", "tri-saw", "sawtooth", "square", "wide pulse", "narrow pulse" };
            const int w = juce::jlimit (0, 5, (int) v);
            return juce::String (i == 2 && w == 1 ? "reverse saw" : names[w]);
        };
        range[(size_t) i]->slider.setPopupDisplayEnabled (true, true, nullptr);
        wave[(size_t) i]->slider.setPopupDisplayEnabled (true, true, nullptr);
        if (freq[(size_t) i])
            unitText (*freq[(size_t) i], [this, i] (double v) {
                if (i == 2 && ! patch.osc3Control)
                {
                    // free-running: the knob spans +/-3 octaves; show where oscillator 3 sits (its LFO rate in LO)
                    const double st = mapping::frequencySemitones (2, (float) v, false);
                    const double hz = mapping::noteToHz (mapping::kOsc3FreeNote + patch.tune + 12.0 * mapping::rangeOctaves (patch.osc[2].range) + st);
                    return (st >= 0 ? "+" : "") + juce::String (st / 12.0, 2) + " octaves  (" + formatHz (hz) + ")";
                }
                return (v >= 0 ? "+" : "") + juce::String (v, 2) + " semitones";
            });
        unitText (*oscVol[(size_t) i], [] (double v) { return juce::String (v, 1); });
    }
    unitText (cutoff, [] (double v) { return formatHz (mapping::cutoffHz ((float) v)); });
    unitText (emphasis, [] (double v) { return juce::String (v, 1) + (mapping::emphasisFeedback ((float) v) >= 4.0 ? "  (self-oscillates)" : ""); });
    unitText (contourAmt, [] (double v) { return juce::String (mapping::contourOctaves ((float) v), 1) + " octaves"; });
    for (auto* k : { &fAttack, &lAttack })
        unitText (*k, [] (double v) { return formatSeconds (mapping::attackSeconds ((float) v)); });
    for (auto* k : { &fDecay, &lDecay })
        unitText (*k, [] (double v) { return formatSeconds (mapping::decaySeconds ((float) v)); });
    for (auto* k : { &fSustain, &lSustain })
        unitText (*k, [] (double v) { return juce::String (juce::roundToInt (v * 10.0)) + "%"; });
    unitText (volume, [] (double v) { return v <= 0.0 ? juce::String ("silent") : juce::String (juce::Decibels::gainToDecibels (mapping::outputGain ((float) v)), 1) + " dB"; });
    for (auto* k : { &extVol, &noiseVol })
        unitText (*k, [] (double v) { return juce::String (v, 1); });

    loadSettings();
    updateTooltips();

    // keys belong to the keyboard: nothing else takes keyboard focus
    std::function<void (juce::Component&)> noFocus = [&noFocus] (juce::Component& c) {
        for (auto* child : c.getChildren())
        {
            child->setWantsKeyboardFocus (false);
            child->setMouseClickGrabsKeyboardFocus (false);
            noFocus (*child);
        }
    };
    noFocus (*this);
    // ...and a click anywhere (a knob, a key, a switch) gives the keys back to the keyboard
    addMouseListener (this, true);

    loadPatchIntoControls();
    auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1600, 1000);
    setSize (juce::jlimit (1240, 1640, (int) (area.getWidth() * 0.94f)), juce::jlimit (720, 900, (int) (area.getHeight() * 0.9f)));
    if (auto* forced = std::getenv ("FOURPOLE_WINDOW_SIZE")) // e.g. "1280x760", for testing layouts
    {
        const auto wh = juce::StringArray::fromTokens (forced, "x", {});
        if (wh.size() == 2)
            setSize (juce::jmax (1240, wh[0].getIntValue()), juce::jmax (720, wh[1].getIntValue()));
    }
    startTimerHz (30);

    if (auto* reportPath = std::getenv ("FOURPOLE_SELFTEST"))
    {
        juce::Component::SafePointer<MainComponent> safe (this);
        const juce::File report (reportPath);
        juce::Timer::callAfterDelay (1500, [safe, report] {
            if (safe != nullptr)
                safe->runSelfTest (report);
        });
    }
}

MainComponent::~MainComponent()
{
    if (engine.isRecording())
        engine.stopRecording();
    saveSettings (false); // (the window is already going: its place was saved by prepareToQuit)
    stopTimer();
    engine.onMidiDeviceMessage = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MainComponent::parentHierarchyChanged()
{
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::MessageManager::callAsync ([safe] {
        if (safe == nullptr)
            return;
        if (! safe->windowRestored && safe->isShowing())
        {
            // the window's size and place from last time (unless a test asked for a size)
            safe->windowRestored = true;
            if (auto* w = dynamic_cast<juce::ResizableWindow*> (safe->getTopLevelComponent()))
                if (safe->settings != nullptr && safe->settings->containsKey ("window") && std::getenv ("FOURPOLE_WINDOW_SIZE") == nullptr)
                    w->restoreWindowStateFromString (safe->settings->getValue ("window"));
        }
        if (safe->isShowing())
            safe->grabKeyboardFocus();
    });
}

// ---------------------------------------------------------------- patch <-> controls

void MainComponent::loadPatchIntoControls()
{
    const juce::ScopedValueSetter<bool> svs (loading, true);
    for (auto& [id, ref] : controls)
    {
        const float v = ref.def->get (patch);
        if (ref.knob != nullptr)
            ref.knob->setValue (v);
        else
            ref.rocker->setToggleState (v >= 0.5f, juce::dontSendNotification);
    }
    repaint (namePlate);
}

void MainComponent::markSettingsDirty()
{
    if (! settingsDirty)
        settingsDirtySince = juce::Time::getMillisecondCounter();
    settingsDirty = true;
}

void MainComponent::patchEdited()
{
    markSettingsDirty();
    edited = true;
    engine.setPatch (patch);
    repaint (namePlate);
}

void MainComponent::flash (const juce::String& m)
{
    message = m;
    messageUntil = juce::Time::getMillisecondCounter() + 2500;
    repaint (namePlate);
}

juce::File MainComponent::patchFolder() const
{
    if (auto* dir = std::getenv ("FOURPOLE_PATCH_DIR")) // testing: keep test patches out of ~/Music
        return juce::File (dir);
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fourpole Patches");
}

void MainComponent::stepPatch (int dir)
{
    // factory patches, then the ones saved in the patch folder (re-read: files may have come or gone)
    library.rescan();
    if (currentFile != juce::File())
        if (const int i = library.indexOf (currentFile); i >= 0)
            patchIndex = i; // step from the file on the panel, wherever it now sits in the list
    selectPatch (library.step (patchIndex, dir));
}

void MainComponent::selectPatch (int index)
{
    if (index < 0 || index >= library.size())
        return flash ("No patch " + juce::String (index + 1) + " (there are " + juce::String (library.size()) + ")");
    Patch p;
    if (! library.load (index, p))
        return flash ("Couldn't read " + library.fileAt (index).getFileName());
    remember ("patch change");
    patch = p;
    patchIndex = index;
    currentFile = library.fileAt (index);
    edited = false;
    loadPatchIntoControls();
    engine.setPatch (patch);
    markSettingsDirty();
}

void MainComponent::savePatch (bool askName)
{
    auto write = [this] (const juce::File& f) {
        patch.name = f.getFileNameWithoutExtension();
        if (f.replaceWithText (juce::JSON::toString (patch.toJson())))
        {
            currentFile = f;
            edited = false;
            library.rescan();
            if (const int i = library.indexOf (f); i >= 0)
                patchIndex = i; // < > now step on from here
            engine.setPatch (patch);
            markSettingsDirty();
            flash ("Saved " + f.getFileName());
        }
        else
            flash ("Couldn't save " + f.getFileName());
        repaint (namePlate);
    };
    patchFolder().createDirectory();
    if (! askName && currentFile.existsAsFile())
        return write (currentFile);
    chooser = std::make_unique<juce::FileChooser> ("Save patch", patchFolder().getChildFile (patch.name + ".fourpole"), "*.fourpole");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [write] (const juce::FileChooser& fc) {
                              auto f = fc.getResult();
                              if (f != juce::File())
                                  write (f.withFileExtension ("fourpole"));
                          });
}

void MainComponent::loadPatchFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load patch", patchFolder(), "*.fourpole");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) {
                              if (auto f = fc.getResult(); f.existsAsFile())
                                  openPatchFile (f);
                          });
}

void MainComponent::openPatchFile (const juce::File& f)
{
    Patch p;
    if (! PatchLibrary::loadFile (f, p))
        return flash (f.getFileName() + " isn't a Fourpole patch");
    remember ("load " + f.getFileName());
    patch = p;
    currentFile = f;
    edited = false;
    library.rescan();
    if (const int i = library.indexOf (f); i >= 0)
        patchIndex = i;
    loadPatchIntoControls();
    engine.setPatch (patch);
    markSettingsDirty();
    flash ("Loaded " + f.getFileName());
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("fourpole"))
            return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("fourpole"))
            return openPatchFile (juce::File (f)); // the first patch dropped
}

// ---------------------------------------------------------------- undo

PanelState MainComponent::currentState() const
{
    return { patch, currentFile.getFullPathName(), patchIndex, edited };
}

void MainComponent::restoreState (const PanelState& st)
{
    patch = st.patch;
    currentFile = st.file.isNotEmpty() ? juce::File (st.file) : juce::File();
    patchIndex = st.patchIndex;
    edited = st.edited;
    loadPatchIntoControls();
    engine.setPatch (patch);
    markSettingsDirty();
}

void MainComponent::remember (const juce::String& what)
{
    history.remember (currentState(), what, juce::Time::getMillisecondCounter());
}

void MainComponent::undo()
{
    juce::String what;
    if (auto st = history.undo (currentState(), what))
    {
        restoreState (*st);
        flash ("Undo " + controlName (what));
    }
    else
        flash ("Nothing to undo");
}

void MainComponent::redo()
{
    juce::String what;
    if (auto st = history.redo (currentState(), what))
    {
        restoreState (*st);
        flash ("Redo " + controlName (what));
    }
    else
        flash ("Nothing to redo");
}

void MainComponent::showSetupMenu()
{
    // conveniences the hardware didn't have
    juce::PopupMenu m, bend, prio, vel, drift;
    for (int st : { 1, 2, 3, 5, 7, 12 })
        bend.addItem ("+/- " + juce::String (st) + " semitones", true, juce::approximatelyEqual (patch.bendRange, (float) st),
                      [this, st] { remember ("bendRange"); patch.bendRange = (float) st; patchEdited(); });
    const char* prios[] = { "Low note (original)", "High note", "Last note" };
    for (int i = 0; i < 3; ++i)
        prio.addItem (prios[i], true, (int) patch.priority == i, [this, i] { remember ("priority"); patch.priority = (NotePriority) i; patchEdited(); });
    for (auto [label, amt] : { std::pair { "Off (original)", 0.0f }, std::pair { "Light", 0.35f }, std::pair { "Full", 1.0f } })
        vel.addItem (label, true, juce::approximatelyEqual (patch.velocity, amt), [this, a = amt] { remember ("velocity"); patch.velocity = a; patchEdited(); });
    for (auto [label, amt] : { std::pair { "Off (perfectly stable)", 0.0f }, std::pair { "Subtle", 0.5f }, std::pair { "Vintage", 1.0f } })
        drift.addItem (label, true, juce::approximatelyEqual (patch.drift, amt), [this, a = amt] { remember ("drift"); patch.drift = a; patchEdited(); });
    m.addSubMenu ("Pitch wheel range", bend);
    m.addSubMenu ("Note priority", prio);
    m.addSubMenu ("Velocity to loudness", vel);
    m.addSubMenu ("Oscillator drift", drift);
    m.addSeparator();
    juce::PopupMenu ext;
    ext.addItem ("Nothing plugged in (output feedback, like the original)", true, ! engine.isUsingExternalAudioInput(),
                 [this] { engine.setExternalAudioInput (false); saveSettings(); flash ("EXTERNAL INPUT: output feedback"); });
    ext.addItem ("Audio input 1 (process any sound through the filter)", true, engine.isUsingExternalAudioInput(),
                 [this] { engine.setExternalAudioInput (true); saveSettings(); flash ("EXTERNAL INPUT: audio input 1 - turn up EXTERNAL INPUT in the mixer"); });
    m.addSubMenu ("External input jack", ext);
    juce::PopupMenu midi;
    for (auto& [cc, id] : midiMap.all())
        midi.addItem ("CC " + juce::String (cc) + "  ->  " + controlName (id), false, false, nullptr);
    midi.addSeparator();
    midi.addItem ("Reset to defaults (CC 70-77 = an 8-knob controller)", [this] { midiMap.resetToDefaults(); saveSettings(); updateTooltips(); flash ("MIDI assignments reset"); });
    midi.addItem ("Clear all", [this] { midiMap.fromJson (juce::var (new juce::DynamicObject())); saveSettings(); updateTooltips(); flash ("MIDI assignments cleared"); });
    m.addSubMenu ("MIDI controller assignments (right-click a knob to learn)", midi);
    m.addItem ("Show recordings folder", [] {
        auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fourpole Recordings");
        dir.createDirectory();
        dir.revealToUser();
    });
    m.addSeparator();
    m.addItem ("Init patch", [this] { selectPatch (0); });
    m.addItem ("Undo  (Cmd-Z)", history.canUndo(), false, [this] { undo(); });
    m.addItem ("Redo  (Cmd-Shift-Z)", history.canRedo(), false, [this] { redo(); });
    m.addItem ("All notes off  (Esc)", [this] { panic(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&setupBtn));
}

void MainComponent::showHelp()
{
    const auto inputs = engine.getActiveMidiInputNames();
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::NoIcon, "Fourpole - quick guide",
        "PLAY: click the keys (drag for a glissando), use the computer keyboard (A W S E D F T G Y H U J K O L P ; ' = C to F, "
        "Z / X = octave down / up), or plug in a MIDI keyboard any time (notes, pitch bend, mod wheel).\n\n"
        "SIGNAL FLOW, left to right: three OSCILLATORS (range in feet, waveform, frequency) -> MIXER (levels and on/off; "
        "push it hot to drive the filter - the OVERLOAD lamp lights) -> the ladder FILTER (cutoff, emphasis = resonance, "
        "amount of filter contour, keyboard tracking 1/3, 2/3 or both) -> LOUDNESS contour -> OUTPUT.\n\n"
        "CONTOURS: attack, decay, sustain. There's no release knob: DECAY (by the wheels) on = notes release at the decay time.\n\n"
        "MODULATION: OSC. 3 CONTROL off + LO range turns oscillator 3 into an LFO. MODULATION MIX blends it with noise; "
        "the MOD. wheel sends that to the oscillators (OSC. MODULATION) and/or the filter (FILTER MODULATION).\n\n"
        "TRICKS: EMPHASIS at 10 + both KEYBOARD CONTROL switches = the filter plays as a sine oscillator. "
        "EXTERNAL INPUT on and up = output feedback overdrive.\n\n"
        "PATCHES: < > step through factory and saved patches (or send a MIDI program change: 1 = the first). Save goes to "
        "Music/Fourpole Patches; drop a .fourpole file on the window to load it. A * after the name means it's been changed. "
        "Cmd-Z / Cmd-Shift-Z undo and redo knob moves, switches and patch changes. The panel is remembered when you quit, "
        "like the hardware's knobs.\n\n"
        "SETUP: pitch wheel range, note priority, velocity, drift, the external input jack, MIDI assignments. "
        "Esc = all notes off.\n\n"
        "MIDI CONTROLLERS: knobs on CC 70-77 (e.g. an MPK Mini's K1-K8) control contour amount, emphasis, loudness decay "
        "and attack, cutoff, filter decay, glide and oscillator 2 frequency. RIGHT-CLICK any knob or switch and choose MIDI learn "
        "to assign it to whatever you move next. A sustain pedal holds notes.\n\n"
        "REC records what you play to Music/Fourpole Recordings. Hover a knob to read its value in Hz, ms or semitones.\n\n"
        "MIDI IN: " + (inputs.isEmpty() ? juce::String ("no controllers connected") : inputs.joinIntoString (", ")) + ".");
}

void MainComponent::showAudioSettings()
{
    // inputs 0..1: pick the interface input EXTERNAL INPUT hears (Setup > External input jack)
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(), 0, 1, 1, 2, true, false, true, false);
    selector->setSize (560, 540);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio / MIDI settings";
    o.dialogBackgroundColour = juce::Colour (0xff1d1d20);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

// ---------------------------------------------------------------- recording

void MainComponent::toggleRecording()
{
    if (engine.isRecording())
    {
        const auto f = engine.stopRecording();
        recBtn.setToggleState (false, juce::dontSendNotification);
        flash ("Saved " + f.getFileName());
        return;
    }
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fourpole Recordings");
    const auto f = dir.getChildFile ("Fourpole " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav");
    if (! engine.startRecording (f))
        flash ("Couldn't start recording in " + dir.getFullPathName());
}

// ---------------------------------------------------------------- MIDI learn

void MainComponent::bind (const juce::String& id, PanelKnob& k)
{
    const auto* def = panel::find (id);
    jassert (def != nullptr && ! def->isSwitch());
    if (def == nullptr)
        return;
    controls[id] = { &k, nullptr, def };
    k.onChange = [this, def] (double v) {
        if (loading)
            return;
        remember (def->id);
        def->set (patch, (float) v);
        patchEdited();
    };
    k.slider.onRightClick = [this, id] { showControlMenu (id); };
    addAndMakeVisible (k);
}

void MainComponent::bind (const juce::String& id, Rocker& r)
{
    const auto* def = panel::find (id);
    jassert (def != nullptr && def->isSwitch());
    if (def == nullptr)
        return;
    controls[id] = { nullptr, &r, def };
    r.onToggle = [this, def] (bool on) {
        if (loading)
            return;
        remember (def->id);
        def->set (patch, on ? 1.0f : 0.0f);
        patchEdited();
    };
    r.onRightClick = [this, id] { showControlMenu (id); };
    addAndMakeVisible (r);
}

juce::String MainComponent::controlName (const juce::String& id) const
{
    const auto* def = panel::find (id);
    return def != nullptr ? def->name : id;
}

void MainComponent::updateTooltips()
{
    for (auto& [id, ref] : controls)
    {
        const int cc = midiMap.ccFor (id);
        const auto learn = juce::String (cc >= 0 ? "MIDI CC " + juce::String (cc) + ". " : juce::String()) + "Right-click: MIDI learn";
        if (ref.knob != nullptr)
            continue; // knobs show their value bubble on hover instead (a tooltip would cover it)
        else
        {
            // keep a switch's own explanation, with the MIDI line after it
            auto tip = ref.rocker->getTooltip().upToFirstOccurrenceOf ("\n", false, false);
            if (tip.contains ("MIDI learn"))
                tip.clear();
            ref.rocker->setTooltip (tip.isEmpty() ? learn : tip + "\n" + learn);
        }
    }
}

void MainComponent::showControlMenu (const juce::String& id)
{
    juce::PopupMenu m;
    m.addSectionHeader (controlName (id));
    const int cc = midiMap.ccFor (id);
    m.addItem ("MIDI learn...", [this, id] {
        learnTarget = id;
        message = "MIDI LEARN: move a knob or button on your controller for " + controlName (id) + "  (Esc cancels)";
        messageUntil = juce::Time::getMillisecondCounter() + 15000;
        repaint (namePlate);
    });
    if (cc >= 0)
        m.addItem ("Forget CC " + juce::String (cc), [this, id] {
            midiMap.forgetControl (id);
            saveSettings();
            updateTooltips();
            flash ("Forgot the MIDI assignment");
        });
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void MainComponent::applyController (int cc, int value)
{
    if (cc == Engine::kProgramChange)
    {
        library.rescan();
        selectPatch (value); // program 1 = the first patch, as < > count them
        return;
    }
    if (learnTarget.isNotEmpty())
    {
        midiMap.assign (cc, learnTarget);
        flash ("CC " + juce::String (cc) + " now controls " + controlName (learnTarget));
        learnTarget.clear();
        saveSettings();
        updateTooltips();
    }
    auto it = controls.find (midiMap.controlFor (cc));
    if (it == controls.end())
        return;
    if (auto* k = it->second.knob)
    {
        auto& s = k->slider;
        const auto span = s.getRange();
        double v = span.getStart() + span.getLength() * value / 127.0;
        if (s.getInterval() >= 1.0)
            v = std::round (v);
        s.setValue (v, juce::sendNotificationSync); // moves the knob and edits the patch
    }
    else if (auto* r = it->second.rocker)
    {
        const bool on = value >= 64;
        if (on != r->getToggleState())
        {
            r->setToggleState (on, juce::dontSendNotification);
            if (r->onToggle)
                r->onToggle (on);
        }
    }
}

void MainComponent::loadSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Fourpole";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.folderName = "Fourpole";
    settings = std::make_unique<juce::PropertiesFile> (o);
    if (settings->containsKey ("midiMap"))
        midiMap.fromJson (juce::JSON::parse (settings->getValue ("midiMap")));
    // the audio interface chosen in AUDIO/MIDI (falls back to the default if it isn't there now)
    if (auto xml = settings->getXmlValue ("audioDevice"))
        engine.getDeviceManager().initialise (0, 2, xml.get(), true);
    if (settings->getBoolValue ("externalAudioInput", false))
        engine.setExternalAudioInput (true);
    // the panel as you left it, like the hardware's knobs (edits and all)
    if (settings->containsKey ("panel"))
    {
        const auto json = juce::JSON::parse (settings->getValue ("panel"));
        if (json.getProperty ("format", {}).toString() == "fourpole-patch")
        {
            patch = Patch::fromJson (json);
            const juce::File f (settings->getValue ("panelFile"));
            currentFile = settings->getValue ("panelFile").isNotEmpty() && f.existsAsFile() ? f : juce::File();
            patchIndex = juce::jlimit (0, library.size() - 1, settings->getIntValue ("panelIndex", 0));
            edited = settings->getBoolValue ("panelEdited", false);
            engine.setPatch (patch);
        }
    }
}

void MainComponent::prepareToQuit()
{
    if (engine.isRecording())
        engine.stopRecording();
    saveSettings (true); // while the window is still there to ask where it is
}

void MainComponent::saveSettings (bool includeWindow)
{
    if (settings == nullptr)
        return;
    settings->setValue ("midiMap", juce::JSON::toString (midiMap.toJson(), true));
    settings->setValue ("externalAudioInput", engine.isUsingExternalAudioInput());
    if (auto xml = engine.getDeviceManager().createStateXml())
        settings->setValue ("audioDevice", xml.get());
    settings->setValue ("panel", juce::JSON::toString (patch.toJson(), true));
    settings->setValue ("panelFile", currentFile.getFullPathName());
    settings->setValue ("panelIndex", patchIndex);
    settings->setValue ("panelEdited", edited);
    if (includeWindow)
        if (auto* w = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()); w != nullptr && w->isOnDesktop())
            settings->setValue ("window", w->getWindowStateAsString());
    settings->saveIfNeeded();
    settingsDirty = false;
}

// ---------------------------------------------------------------- timer / keys

void MainComponent::timerCallback()
{
    overload.setOn (engine.isOverloaded());
    // keep the remembered panel up to date (a second after the last change), so a crash or a forced
    // quit doesn't lose it
    if (settingsDirty && juce::Time::getMillisecondCounter() - settingsDirtySince > 1000)
        saveSettings();
    for (int cc = 0, value = 0, n = 0; n < 64 && engine.nextControllerMessage (cc, value); ++n)
        applyController (cc, value);
    if (engine.isRecording())
    {
        recBtn.setToggleState ((juce::Time::getMillisecondCounter() / 500) % 2 == 0, juce::dontSendNotification);
        const int secs = (int) engine.getRecordedSeconds();
        const auto text = "RECORDING  " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2) + "  (press REC to stop)";
        if (message != text)
        {
            message = text;
            messageUntil = juce::Time::getMillisecondCounter() + 1500;
            repaint (namePlate);
        }
    }
    keyboard.refreshExternal();
    if (! pitchWheel.isBeingDragged())
        pitchWheel.setValue (engine.getMidiPitchBend());
    const float midiMod = engine.getMidiModWheel();
    if (! juce::exactlyEqual (midiMod, lastMidiMod)) // a controller's wheel moved (all the way back to 0 too)
    {
        lastMidiMod = midiMod;
        if (! modWheel.isBeingDragged())
            modWheel.setValue (midiMod);
    }
    if (messageUntil != 0 && juce::Time::getMillisecondCounter() > messageUntil)
    {
        messageUntil = 0;
        message.clear();
        repaint (namePlate);
    }
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (k.getModifiers().isCommandDown())
    {
        if (c == 's') { savePatch (k.getModifiers().isShiftDown()); return true; }
        if (c == 'o') { loadPatchFile(); return true; }
        if (c == 'z' || k.getKeyCode() == 'Z')
        {
            if (k.getModifiers().isShiftDown())
                redo();
            else
                undo();
            return true;
        }
        return false;
    }
    if (c == 'z') { octaveBase = juce::jmax (24, octaveBase - 12); flash ("Computer keyboard: C" + juce::String (octaveBase / 12 - 1)); return true; }
    if (c == 'x') { octaveBase = juce::jmin (84, octaveBase + 12); flash ("Computer keyboard: C" + juce::String (octaveBase / 12 - 1)); return true; }
    if (k == juce::KeyPress::leftKey) { stepPatch (-1); return true; }
    if (k == juce::KeyPress::rightKey) { stepPatch (1); return true; }
    if (k.getTextCharacter() == '?') { showHelp(); return true; }
    if (k == juce::KeyPress::escapeKey)
    {
        if (learnTarget.isNotEmpty())
        {
            learnTarget.clear();
            flash ("MIDI learn cancelled");
        }
        else
            panic();
        return true;
    }
    for (auto key : kKeys)
        if (c == (juce::juce_wchar) key)
            return true; // handled in keyStateChanged
    return false;
}

void MainComponent::releaseComputerKeys()
{
    // the window lost focus with keys held: their key-ups will never arrive here
    for (int i = 0; i < 18; ++i)
        if (keyHeld[(size_t) i])
        {
            keyHeld[(size_t) i] = false;
            engine.noteOff (keyNote[(size_t) i]);
            keyboard.setKeyDown (keyNote[(size_t) i], false);
        }
}

void MainComponent::panic()
{
    releaseComputerKeys();
    engine.allNotesOff();
    flash ("All notes off");
}

bool MainComponent::keyStateChanged (bool)
{
    if (juce::ModifierKeys::currentModifiers.isCommandDown())
        return false;
    bool handled = false;
    for (int i = 0; i < 18; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (kKeys[i]);
        if (down == keyHeld[(size_t) i])
            continue;
        keyHeld[(size_t) i] = down;
        handled = true;
        if (down)
        {
            keyNote[(size_t) i] = octaveBase + i;
            engine.noteOn (keyNote[(size_t) i], 100);
            keyboard.setKeyDown (keyNote[(size_t) i], true);
        }
        else
        {
            engine.noteOff (keyNote[(size_t) i]);
            keyboard.setKeyDown (keyNote[(size_t) i], false);
        }
    }
    return handled;
}

// ---------------------------------------------------------------- self-test

void MainComponent::runSelfTest (const juce::File& reportFile)
{
    settings.reset(); // nothing the test does is saved into the user's settings
    auto lines = std::make_shared<juce::StringArray>();
    auto failures = std::make_shared<int> (0);
    auto log = [lines] (const juce::String& m) { lines->add (m); };
    auto fail = [lines, failures] (const juce::String& m) { lines->add ("FAIL  " + m); ++*failures; };
    auto near = [] (float a, float b) { return std::abs (a - b) < 1.0e-3f; };
    // turn / flip a widget through its own callbacks (the ones a drag or a click runs)
    auto move = [] (const ControlRef& ref, float v) {
        if (ref.knob != nullptr)
            ref.knob->slider.setValue (v, juce::sendNotificationSync);
        else
            ref.rocker->setToggleState (v >= 0.5f, juce::sendNotificationSync);
    };
    auto shown = [] (const ControlRef& ref) {
        return ref.knob != nullptr ? (float) ref.knob->slider.getValue() : (ref.rocker->getToggleState() ? 1.0f : 0.0f);
    };

    log ("-- every control in the panel table has a widget");
    for (auto& c : panel::controls())
        if (controls.count (c.id) == 0)
            fail (c.id + ": no widget");

    log ("-- turning each widget changes its own field in the engine's patch, and nothing else");
    for (auto& c : panel::controls())
    {
        auto it = controls.find (c.id);
        if (it == controls.end())
            continue;
        const auto& ref = it->second;
        const Patch before = engine.getPatch();
        const float was = c.get (before);
        const float target = was <= (c.min + c.max) * 0.5f ? c.max : c.min;
        move (ref, target);
        const Patch after = engine.getPatch();
        if (! near (c.get (after), target))
            fail (c.id + ": widget set to " + juce::String (target) + ", engine patch has " + juce::String (c.get (after)));
        for (auto& other : panel::controls())
            if (&other != &c && ! near (other.get (after), other.get (before)))
                fail (c.id + ": also changed " + other.id);
        const auto readout = ref.knob != nullptr ? ref.knob->slider.getTextFromValue (target) : juce::String (target > 0.5f ? "ON" : "OFF");
        log ("ok    " + c.id.paddedRight (' ', 18) + juce::String (was, 2) + " -> " + juce::String (target, 2) + "    " + readout);
        move (ref, was);
        if (! near (c.get (engine.getPatch()), was))
            fail (c.id + ": didn't go back to " + juce::String (was));
    }

    log ("-- MIDI learn: each control takes a CC and follows it");
    {
        const auto savedMap = midiMap;
        constexpr int testCC = 102;
        for (auto& c : panel::controls())
        {
            auto it = controls.find (c.id);
            if (it == controls.end())
                continue;
            const float was = c.get (engine.getPatch());
            const bool up = was <= (c.min + c.max) * 0.5f;
            learnTarget = c.id;
            applyController (testCC, up ? 127 : 0);
            const float now = c.get (engine.getPatch());
            if (midiMap.controlFor (testCC) != c.id)
                fail (c.id + ": CC " + juce::String (testCC) + " wasn't assigned");
            if (! near (now, up ? c.max : c.min))
                fail (c.id + ": CC value " + juce::String (up ? 127 : 0) + " set it to " + juce::String (now));
            if (! near (shown (it->second), now))
                fail (c.id + ": the widget doesn't show the CC's value");
            move (it->second, was);
        }
        midiMap = savedMap;
        updateTooltips();
        log ("ok    " + juce::String ((int) panel::controls().size()) + " controls");
    }

    log ("-- < and > load every factory patch into the engine and onto every widget");
    {
        const auto savedPatch = patch;
        const int savedIndex = patchIndex;
        const auto savedFile = currentFile;
        for (int k = 0; k < (int) factoryPatches().size(); ++k)
        {
            patchIndex = k - 1;
            stepPatch (1);
            const auto& fp = factoryPatches()[(size_t) k];
            if (juce::JSON::toString (engine.getPatch().toJson()) != juce::JSON::toString (fp.toJson()))
                fail (fp.name + ": the engine didn't get the patch");
            for (auto& [id, ref] : controls)
                if (! near (shown (ref), ref.def->get (fp)))
                    fail (fp.name + ": " + id + " shows " + juce::String (shown (ref)) + ", patch has " + juce::String (ref.def->get (fp)));
        }
        patch = savedPatch;
        patchIndex = savedIndex;
        currentFile = savedFile;
        loadPatchIntoControls();
        engine.setPatch (patch);
        log ("ok    " + juce::String ((int) factoryPatches().size()) + " patches");
    }

    log ("-- undo / redo, the edited mark");
    {
        const auto saved = currentState();
        history.clear();
        auto& cut = controls["cutoff"];
        edited = false;
        const float was = cut.def->get (patch);
        // one knob turn (several moves in a row) is one undo step
        for (float v : { -2.0f, -1.0f, 0.5f })
            move (cut, v);
        if (! edited)
            fail ("turning a knob didn't mark the patch edited (*)");
        undo();
        if (! near (cut.def->get (engine.getPatch()), was) || ! near (shown (cut), was) || edited)
            fail ("undo didn't put the cutoff back where it was (or left it marked edited)");
        redo();
        if (! near (cut.def->get (engine.getPatch()), 0.5f) || ! near (shown (cut), 0.5f))
            fail ("redo didn't bring the cutoff turn back");
        undo();
        // undo reaches a patch change too: browsing away doesn't lose your tweaks
        move (cut, 2.0f);
        stepPatch (1);
        undo();
        if (! near (cut.def->get (engine.getPatch()), 2.0f))
            fail ("undo after > didn't bring the tweaked patch back");
        restoreState (saved);
        history.clear();
        log ("ok    knob turns, redo, and undoing a patch change");
    }

    log ("-- MIDI program change and Init");
    {
        const auto saved = currentState();
        applyController (Engine::kProgramChange, 2);
        if (engine.getPatch().name != factoryPatches()[2].name)
            fail ("program change 3 didn't select " + factoryPatches()[2].name + " (got " + engine.getPatch().name + ")");
        applyController (Engine::kProgramChange, 127);
        if (engine.getPatch().name != factoryPatches()[2].name)
            fail ("a program change past the last patch changed the patch");
        restoreState (saved);
        history.clear();
        log ("ok    program change selects patches; one past the end is ignored");
    }

    if (auto* dirPath = std::getenv ("FOURPOLE_PATCH_DIR"))
    {
        log ("-- saving and loading (in " + juce::String (dirPath) + ")");
        const auto saved = currentState();
        const juce::File dir (dirPath);
        dir.createDirectory();
        auto writePatch = [] (const juce::File& f, float cutoffValue) {
            Patch p;
            p.cutoff = cutoffValue;
            f.replaceWithText (juce::JSON::toString (p.toJson()));
        };
        writePatch (dir.getChildFile ("A first.fourpole"), 1.0f);
        writePatch (dir.getChildFile ("C third.fourpole"), 3.0f);
        // SAVE over a file in the folder: < > then step on from that file
        const auto b = dir.getChildFile ("B second.fourpole");
        writePatch (b, 0.0f);
        selectPatch (1); // a factory patch...
        move (controls["cutoff"], 2.0f);
        currentFile = b; // ...saved as "B second"
        savePatch (false);
        Patch back;
        if (! PatchLibrary::loadFile (b, back) || ! near (back.cutoff, 2.0f) || back.name != "B second")
            fail ("SAVE didn't write the panel into B second.fourpole");
        if (edited)
            fail ("still marked edited after saving");
        stepPatch (1);
        if (engine.getPatch().name != "C third")
            fail ("> after saving B didn't step to C (got " + engine.getPatch().name + ")");
        stepPatch (-1);
        stepPatch (-1);
        if (engine.getPatch().name != "A first")
            fail ("< < didn't step back to A (got " + engine.getPatch().name + ")");
        // dropping a patch file on the window loads it
        const auto dropped = juce::File::createTempFile ("dropped.fourpole");
        writePatch (dropped, -4.0f);
        if (! isInterestedInFileDrag ({ dropped.getFullPathName() }) || isInterestedInFileDrag ({ "/tmp/song.wav" }))
            fail ("the window should take .fourpole files (and only those)");
        filesDropped ({ dropped.getFullPathName() }, 10, 10);
        if (! near (engine.getPatch().cutoff, -4.0f) || ! near (shown (controls["cutoff"]), -4.0f))
            fail ("dropping a patch file didn't load it");
        dropped.deleteFile();
        restoreState (saved);
        history.clear();
        library.rescan();
        log ("ok    SAVE, then < > from the saved file; dropping a file loads it");
    }

    log ("-- POWER and REC");
    powerSw.setToggleState (false, juce::sendNotificationSync);
    if (engine.isPowered())
        fail ("POWER off didn't reach the engine");
    powerSw.setToggleState (true, juce::sendNotificationSync);
    if (! engine.isPowered())
        fail ("POWER on didn't reach the engine");
    const auto recordings = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fourpole Recordings");
    const bool recordingsExisted = recordings.isDirectory();
    toggleRecording();
    if (! engine.isRecording())
        fail ("REC didn't start a recording");

    // ---- then by ear (well, by meter): the audio thread hears the widgets
    struct Step
    {
        int waitMs;
        std::function<void()> fn;
    };
    auto steps = std::make_shared<std::vector<Step>>();
    auto level = [this] { return engine.getOutputLevel(); };
    auto expectLevel = [level, log, fail] (const juce::String& what, bool sounding) {
        const float l = level();
        if (sounding ? l > 0.01f : l < 1.0e-4f)
            log ("ok    " + what + "  (peak " + juce::String (l, 4) + ")");
        else
            fail (what + "  (peak " + juce::String (l, 4) + ")");
    };
    auto& vol = controls["volume"];
    auto& out = controls["outputOn"];
    auto& ref440 = controls["a440"];
    const float volumeWas = (float) vol.knob->slider.getValue();
    steps->push_back ({ 0, [this, log] { log ("-- sound"); engine.noteOn (48, 100); } });
    steps->push_back ({ 500, [=] { expectLevel ("a held key sounds", true); move (vol, 0.0f); } });
    steps->push_back ({ 300, [=] { expectLevel ("VOLUME at 0 is silent", false); move (vol, volumeWas); } });
    steps->push_back ({ 300, [=] { expectLevel ("VOLUME back up sounds", true); move (out, 0.0f); } });
    steps->push_back ({ 300, [=] { expectLevel ("MAIN OUTPUT off is silent", false); move (out, 1.0f); } });
    steps->push_back ({ 300, [=, this] { expectLevel ("MAIN OUTPUT on sounds", true); powerSw.setToggleState (false, juce::sendNotificationSync); } });
    steps->push_back ({ 300, [=, this] { expectLevel ("POWER off is silent", false); powerSw.setToggleState (true, juce::sendNotificationSync); } });
    steps->push_back ({ 300, [=] { expectLevel ("POWER on again clears the held key", false); move (ref440, 1.0f); } });
    steps->push_back ({ 300, [=, this] { expectLevel ("A-440 sounds with no key held", true); move (ref440, 0.0f); engine.noteOff (48); } });
    steps->push_back ({ 300, [=, this] {
        const auto f = engine.stopRecording();
        recBtn.setToggleState (false, juce::dontSendNotification);
        if (! f.existsAsFile() || f.getSize() < 10000)
            fail ("REC didn't write a recording");
        else
            log ("ok    REC wrote " + juce::String (f.getSize() / 1024) + " KB");
        f.deleteFile();
        if (! recordingsExisted && recordings.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
            recordings.deleteFile(); // leave ~/Music as we found it
        lines->add ("");
        lines->add (*failures == 0 ? juce::String ("ALL PASSED") : juce::String (*failures) + " FAILED");
        reportFile.replaceWithText (lines->joinIntoString ("\n") + "\n");
        juce::JUCEApplicationBase::quit();
    } });

    // each step runs after its wait, then schedules the next
    struct Runner
    {
        static void next (std::shared_ptr<std::vector<Step>> steps, size_t i, juce::Component::SafePointer<MainComponent> safe)
        {
            if (safe == nullptr || i >= steps->size())
                return;
            juce::Timer::callAfterDelay (juce::jmax (1, (*steps)[i].waitMs), [steps, i, safe] {
                if (safe == nullptr)
                    return;
                (*steps)[i].fn();
                next (steps, i + 1, safe);
            });
        }
    };
    Runner::next (steps, 0, this);
}

// ---------------------------------------------------------------- layout

void MainComponent::resized()
{
    auto r = getLocalBounds();
    const int cheek = juce::jlimit (40, 64, (int) (getWidth() * 0.034f));
    leftCheek = r.removeFromLeft (cheek);
    rightCheek = r.removeFromRight (cheek);
    topRail = r.removeFromTop (44);
    barArea = topRail.reduced (10, 7);
    bottomRail = r.removeFromBottom (20);
    bedArea = r.removeFromBottom ((int) (r.getHeight() * 0.36f));
    midRail = r.removeFromBottom (14);
    panelArea = r;

    // ---- patch bar on the top rail
    {
        auto b = barArea;
        auto right = b.removeFromRight (290);
        for (auto* btn : { &helpBtn, &audioBtn, &setupBtn })
        {
            btn->setBounds (right.removeFromRight (btn == &audioBtn ? 100 : 76));
            right.removeFromRight (6);
        }
        b.removeFromRight (16);
        recBtn.setBounds (b.removeFromRight (64));
        prevBtn.setBounds (b.removeFromLeft (30));
        b.removeFromLeft (4);
        namePlate = b.removeFromLeft (juce::jmin (380, b.getWidth() / 2));
        b.removeFromLeft (4);
        nextBtn.setBounds (b.removeFromLeft (30));
        b.removeFromLeft (12);
        for (auto* btn : { &saveBtn, &saveAsBtn, &loadBtn })
        {
            btn->setBounds (b.removeFromLeft (btn == &saveAsBtn ? 80 : 64));
            b.removeFromLeft (6);
        }
    }

    // ---- the panel: five sections, wordmark strip along the bottom
    auto p = panelArea.reduced (16, 12);
    wordmarkArea = p.removeFromBottom (34);
    p.removeFromBottom (4);
    const float fr[5] = { 0.105f, 0.255f, 0.17f, 0.355f, 0.115f };
    const int W = p.getWidth();
    for (int i = 0; i < 5; ++i)
        sections[(size_t) i] = i == 4 ? p : p.removeFromLeft ((int) (W * fr[i]));
    const int head = 22;

    auto grid = [] (juce::Rectangle<int> area, int rows, int row) {
        const int h = area.getHeight() / rows;
        return area.withY (area.getY() + row * h).withHeight (h);
    };

    // controllers: three knobs, then the two modulation rockers
    {
        auto s = sections[0].withTrimmedTop (head).reduced (4, 0);
        auto rows = s.removeFromBottom (78);
        tune.setBounds (grid (s, 3, 0));
        glide.setBounds (grid (s, 3, 1));
        modMix.setBounds (grid (s, 3, 2));
        oscMod.setBounds (rows.removeFromLeft (rows.getWidth() / 2));
        osc3Control.setBounds (rows);
    }
    // oscillator bank: a row per oscillator: range, frequency, waveform
    {
        auto s = sections[1].withTrimmedTop (head).reduced (4, 0);
        for (int i = 0; i < 3; ++i)
        {
            auto row = grid (s, 3, i);
            row.removeFromLeft (18); // "1 2 3" printed down the left edge
            const int cw = row.getWidth() / 3;
            range[(size_t) i]->setBounds (row.removeFromLeft (cw));
            auto mid = row.removeFromLeft (cw);
            if (freq[(size_t) i])
                freq[(size_t) i]->setBounds (mid);
            wave[(size_t) i]->setBounds (row);
        }
    }
    // mixer: knob + on/off per source, top to bottom: osc 1, external, osc 2, noise, osc 3
    {
        auto s = sections[2].withTrimmedTop (head).reduced (4, 0);
        PanelKnob* knobs[5] = { oscVol[0].get(), &extVol, oscVol[1].get(), &noiseVol, oscVol[2].get() };
        Rocker* rocks[5] = { oscOn[0].get(), &extOn, oscOn[1].get(), &noiseOn, oscOn[2].get() };
        for (int i = 0; i < 5; ++i)
        {
            auto row = grid (s, 5, i);
            auto extra = row.removeFromRight (row.getWidth() / 4);
            knobs[i]->setBounds (row.removeFromLeft (row.getWidth() * 2 / 3));
            rocks[i]->setBounds (row.withSizeKeepingCentre (row.getWidth(), juce::jmin (row.getHeight(), 52)));
            if (i == 1)
                overload.setBounds (extra.withSizeKeepingCentre (extra.getWidth() + 20, juce::jmin (row.getHeight() + 10, 70)));
            if (i == 3)
                pink.setBounds (extra.withSizeKeepingCentre (extra.getWidth(), juce::jmin (row.getHeight(), 58)));
        }
    }
    // modifiers: filter (cutoff, emphasis, contour amount + three rockers), filter contour, loudness contour
    {
        auto s = sections[3].withTrimmedTop (head).reduced (4, 0);
        auto rowF = grid (s, 3, 0), rowFC = grid (s, 3, 1), rowL = grid (s, 3, 2);
        auto rockCol = rowF.removeFromRight (rowF.getWidth() / 4);
        const int cw = rowF.getWidth() / 3;
        cutoff.setBounds (rowF.removeFromLeft (cw).expanded (0, 6));
        emphasis.setBounds (rowF.removeFromLeft (cw));
        contourAmt.setBounds (rowF);
        filterMod.setBounds (grid (rockCol, 3, 0));
        kb1.setBounds (grid (rockCol, 3, 1));
        kb2.setBounds (grid (rockCol, 3, 2));
        rowFC.removeFromTop (14);
        rowFC.removeFromRight (rockCol.getWidth());
        rowL.removeFromTop (14);
        rowL.removeFromRight (rockCol.getWidth());
        const int cw2 = rowFC.getWidth() / 3;
        fAttack.setBounds (rowFC.removeFromLeft (cw2));
        fDecay.setBounds (rowFC.removeFromLeft (cw2));
        fSustain.setBounds (rowFC);
        lAttack.setBounds (rowL.removeFromLeft (cw2));
        lDecay.setBounds (rowL.removeFromLeft (cw2));
        lSustain.setBounds (rowL);
    }
    // output: volume, main output on/off, A-440
    {
        auto s = sections[4].withTrimmedTop (head).reduced (4, 0);
        volume.setBounds (grid (s, 3, 0));
        auto mid = grid (s, 3, 1);
        outputOn.setBounds (mid.removeFromLeft (mid.getWidth() / 2).withSizeKeepingCentre (mid.getWidth() / 2, 70));
        a440.setBounds (mid.withSizeKeepingCentre (mid.getWidth(), 70));
        auto bottom = grid (s, 3, 2);
        powerSw.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withSizeKeepingCentre (bottom.getWidth() / 2, 70));
        pilot.setBounds (bottom.withSizeKeepingCentre (60, 60).translated (0, 4)); // a small jewel lamp, with room for its glow
    }

    // ---- keyboard bed: wheels and glide/decay on the left, then the keys
    {
        auto bed = bedArea;
        controllerBlock = bed.removeFromLeft (juce::jlimit (170, 230, getWidth() / 7));
        keysArea = bed.reduced (6, 0).withTrimmedBottom (2);
        keyboard.setBounds (keysArea);
        auto cb = controllerBlock.reduced (12, 8);
        auto rockers = cb.removeFromTop (cb.getHeight() / 3);
        glideOn.setBounds (rockers.removeFromLeft (rockers.getWidth() / 2));
        decayOn.setBounds (rockers);
        cb.removeFromTop (4);
        pitchWheel.setBounds (cb.removeFromLeft (cb.getWidth() / 2));
        modWheel.setBounds (cb);
    }
    background = {};
}

// ---------------------------------------------------------------- painting

void MainComponent::paint (juce::Graphics& g)
{
    const float sc = materials::scaleOf (g);
    const int w = (int) std::ceil (getWidth() * sc), h = (int) std::ceil (getHeight() * sc);
    if (background.getWidth() != w || background.getHeight() != h)
    {
        background = juce::Image (juce::Image::ARGB, std::max (1, w), std::max (1, h), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (sc));
        renderBackground (bg, sc);
    }
    g.drawImageTransformed (background, juce::AffineTransform::scale (1.0f / sc));

    // patch name plate (changes, so it's drawn live)
    auto plate = namePlate.toFloat();
    g.setColour (theme::colours::plate);
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (plate.translated (0, 0.8f), 3.0f, 0.8f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (plate, 3.0f, 1.0f);
    const bool showMsg = message.isNotEmpty();
    const auto title = patch.name + (edited ? " *" : "") + (currentFile.existsAsFile() ? juce::String() : juce::String ("  (factory)"));
    theme::drawPrint (g, showMsg ? message : title,
                      plate.reduced (10.0f, 2.0f), juce::Justification::centredLeft, 13.0f, showMsg);
}

void MainComponent::renderBackground (juce::Graphics& g, float sc)
{
    using namespace theme;
    const auto full = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0b0b0b));

    // ---- walnut: end cheeks (grain runs vertically) and the rails (grain runs across)
    auto drawWood = [&] (juce::Rectangle<int> a, bool horizontal, uint32_t seed, bool lacquerTop) {
        const int ww = (int) std::ceil (a.getWidth() * sc), hh = (int) std::ceil (a.getHeight() * sc);
        const auto key = "wood:" + juce::String (ww) + "x" + juce::String (hh) + (horizontal ? "h" : "v") + juce::String ((int) seed);
        const auto& img = materials::cached (key, [=] { return materials::woodTile (ww, hh, seed, horizontal); });
        g.drawImageTransformed (img, juce::AffineTransform::scale (1.0f / sc).translated ((float) a.getX(), (float) a.getY()));
        // lacquer: a soft sheen and darker edges
        auto af = a.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (lacquerTop ? 0.12f : 0.07f), af.getX(), af.getY(),
                                                 juce::Colours::transparentWhite, af.getX(), af.getY() + af.getHeight() * 0.5f, false));
        g.fillRect (af);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRect (af, 1.0f);
    };
    drawWood (leftCheek, false, 3, true);
    drawWood (rightCheek, false, 7, true);
    drawWood (topRail, true, 11, true);
    drawWood (bottomRail, true, 13, false);
    // the cheeks are rounded: shade their outer and inner edges
    for (auto cheek : { leftCheek, rightCheek })
    {
        auto c = cheek.toFloat();
        const bool left = cheek == leftCheek;
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), left ? c.getX() : c.getRight(), 0,
                                                 juce::Colours::transparentBlack, c.getCentreX(), 0, false));
        g.fillRect (c);
    }

    // ---- the panel: satin black aluminium
    auto pa = panelArea.toFloat();
    {
        const int size = 240;
        const auto& tile = materials::cached ("paint:panel", [=] { return materials::paintTile (colours::panel, size, 0.6f, 5); });
        g.setFillType (juce::FillType (tile, juce::AffineTransform::scale (1.0f / sc)));
        g.fillRect (pa);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), pa.getX(), pa.getY(), juce::Colours::transparentWhite,
                                                 pa.getX(), pa.getBottom(), false));
        g.fillRect (pa);
        // the panel edge catches the light; a shadow where it meets the rail
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.drawHorizontalLine ((int) pa.getY(), pa.getX(), pa.getRight());
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0, pa.getY(), juce::Colours::transparentBlack, 0, pa.getY() + 10.0f, false));
        g.fillRect (pa.withHeight (10.0f));
        for (auto pt : { pa.getTopLeft().translated (10, 10), pa.getTopRight().translated (-10, 10), pa.getBottomLeft().translated (10, -10),
                         pa.getBottomRight().translated (-10, -10) })
        {
            const float d = 9.0f;
            const int px = (int) std::ceil (d * sc);
            materials::drawAt (g, materials::cached ("screw:" + juce::String (px), [=] { return materials::screw (px, 0.7f); }),
                               juce::Rectangle<float> (d, d).withCentre (pt));
        }
    }

    // ---- silkscreen: section titles with rules, dividers, sub-headings
    const char* titles[5] = { "CONTROLLERS", "OSCILLATOR BANK", "MIXER", "MODIFIERS", "OUTPUT" };
    for (int i = 0; i < 5; ++i)
    {
        auto s = sections[(size_t) i].toFloat();
        auto head = s.removeFromTop (18.0f);
        drawPrint (g, titles[i], head, juce::Justification::centred, 11.0f);
        g.setColour (colours::silk.withAlpha (0.75f));
        g.drawHorizontalLine ((int) head.getBottom() + 1, s.getX() + 6.0f, s.getRight() - 6.0f);
        if (i > 0)
            g.drawVerticalLine ((int) s.getX(), s.getY() - 14.0f, s.getBottom());
    }
    {
        // oscillator numbers down the left of the bank
        auto s = sections[1].withTrimmedTop (22).reduced (4, 0).toFloat();
        for (int i = 0; i < 3; ++i)
        {
            const float h = s.getHeight() / 3.0f;
            drawPrint (g, juce::String (i + 1), juce::Rectangle<float> (s.getX(), s.getY() + i * h, 18.0f, h), juce::Justification::centred, 14.0f);
        }
        // filter / contour sub-headings
        auto m = sections[3].withTrimmedTop (22).reduced (4, 0).toFloat();
        const float h = m.getHeight() / 3.0f;
        drawPrint (g, "FILTER CONTOUR", juce::Rectangle<float> (m.getX(), m.getY() + h, m.getWidth() * 0.75f, 14.0f), juce::Justification::centred, 9.5f, true);
        drawPrint (g, "LOUDNESS CONTOUR", juce::Rectangle<float> (m.getX(), m.getY() + 2 * h, m.getWidth() * 0.75f, 14.0f), juce::Justification::centred, 9.5f, true);
        g.setColour (colours::silk.withAlpha (0.35f));
        for (int k = 1; k < 3; ++k)
            g.drawHorizontalLine ((int) (m.getY() + k * h) - 2, m.getX() + 8.0f, m.getX() + m.getWidth() * 0.75f - 8.0f);
    }

    // ---- wordmark strip
    {
        auto wm = wordmarkArea.toFloat();
        g.setColour (colours::silk.withAlpha (0.35f));
        g.drawHorizontalLine ((int) wm.getY(), wm.getX() + 6.0f, wm.getRight() - 6.0f);
        juce::GlyphArrangement ga;
        const float fh = 26.0f;
        ga.addLineOfText (panelFont (fh, true), "fourpole", 0.0f, 0.0f);
        auto bb = ga.getBoundingBox (0, -1, true);
        juce::Path word;
        ga.createPath (word);
        word.applyTransform (juce::AffineTransform::translation (wm.getCentreX() - bb.getCentreX(), wm.getCentreY() - bb.getCentreY() + 2.0f));
        g.setColour (colours::silk);
        g.fillPath (word);
        drawPrint (g, "FP-3", juce::Rectangle<float> (wm.getCentreX() + bb.getWidth() * 0.5f + 12.0f, wm.getY(), 60.0f, wm.getHeight()),
                   juce::Justification::centredLeft, 13.0f);
        drawPrint (g, "THREE OSCILLATOR ANALOG MONOPHONIC SYNTHESIZER", wm.withTrimmedLeft (14.0f).withWidth (wm.getWidth() * 0.34f),
                   juce::Justification::centredLeft, 9.5f, true);
        drawPrint (g, "TRANSISTOR LADDER  24 dB / OCT", wm.withTrimmedRight (14.0f).removeFromRight (wm.getWidth() * 0.3f),
                   juce::Justification::centredRight, 9.5f, true);
    }

    // ---- keyboard bed: black felt behind the keys, the controller block in panel black
    auto bed = bedArea.toFloat();
    g.setColour (juce::Colour (0xff101012));
    g.fillRect (bed);
    auto cb = controllerBlock.toFloat();
    {
        const auto& tile = materials::cached ("paint:panel", [=] { return materials::paintTile (colours::panel, 240, 0.6f, 5); });
        g.setFillType (juce::FillType (tile, juce::AffineTransform::scale (1.0f / sc)));
        g.fillRect (cb);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRect (cb, 1.0f);
    }
    // the rail between panel and keys
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2c), 0, (float) midRail.getY(), juce::Colour (0xff0c0c0d), 0,
                                             (float) midRail.getBottom(), false));
    g.fillRect (midRail);
    juce::ignoreUnused (full);
}
} // namespace fourpole
