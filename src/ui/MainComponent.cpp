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
    return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz" : (hz < 100.0 ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz))) + " Hz";
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

    // ---- bind every control to the patch
    auto knob = [this] (PanelKnob& k, std::function<void (double)> set) {
        k.onChange = [this, set] (double v) {
            if (loading)
                return;
            set (v);
            patchEdited();
        };
        addAndMakeVisible (k);
    };
    auto rocker = [this] (Rocker& r, std::function<void (bool)> set) {
        r.onToggle = [this, set] (bool on) {
            if (loading)
                return;
            set (on);
            patchEdited();
        };
        addAndMakeVisible (r);
    };
    knob (tune, [this] (double v) { patch.tune = (float) v; });
    knob (glide, [this] (double v) { patch.glide = (float) v; });
    knob (modMix, [this] (double v) { patch.modMix = (float) v; });
    rocker (oscMod, [this] (bool b) { patch.oscModulation = b; });
    rocker (osc3Control, [this] (bool b) { patch.osc3Control = b; });
    for (int i = 0; i < 3; ++i)
    {
        knob (*range[(size_t) i], [this, i] (double v) { patch.osc[(size_t) i].range = (int) v; });
        knob (*wave[(size_t) i], [this, i] (double v) { patch.osc[(size_t) i].wave = (int) v; });
        if (freq[(size_t) i])
            knob (*freq[(size_t) i], [this, i] (double v) { patch.osc[(size_t) i].frequency = (float) v; });
        knob (*oscVol[(size_t) i], [this, i] (double v) { patch.oscVolume[(size_t) i] = (float) v; });
        rocker (*oscOn[(size_t) i], [this, i] (bool b) { patch.oscOn[(size_t) i] = b; });
    }
    knob (extVol, [this] (double v) { patch.extVolume = (float) v; });
    rocker (extOn, [this] (bool b) { patch.extOn = b; });
    knob (noiseVol, [this] (double v) { patch.noiseVolume = (float) v; });
    rocker (noiseOn, [this] (bool b) { patch.noiseOn = b; });
    rocker (pink, [this] (bool b) { patch.pinkNoise = b; });
    extOn.setTooltip ("With nothing plugged in, EXTERNAL INPUT hears the main output: turn it up for feedback overdrive");
    addAndMakeVisible (overload);

    knob (cutoff, [this] (double v) { patch.cutoff = (float) v; });
    knob (emphasis, [this] (double v) { patch.emphasis = (float) v; });
    knob (contourAmt, [this] (double v) { patch.contourAmount = (float) v; });
    rocker (filterMod, [this] (bool b) { patch.filterModulation = b; });
    rocker (kb1, [this] (bool b) { patch.keyboardControl1 = b; });
    rocker (kb2, [this] (bool b) { patch.keyboardControl2 = b; });
    kb1.setTooltip ("Filter follows the keyboard: 1/3 (with 2: full tracking)");
    kb2.setTooltip ("Filter follows the keyboard: 2/3 (with 1: full tracking)");
    knob (fAttack, [this] (double v) { patch.filterAttack = (float) v; });
    knob (fDecay, [this] (double v) { patch.filterDecay = (float) v; });
    knob (fSustain, [this] (double v) { patch.filterSustain = (float) v; });
    knob (lAttack, [this] (double v) { patch.loudAttack = (float) v; });
    knob (lDecay, [this] (double v) { patch.loudDecay = (float) v; });
    knob (lSustain, [this] (double v) { patch.loudSustain = (float) v; });
    knob (volume, [this] (double v) { patch.volume = (float) v; });
    rocker (outputOn, [this] (bool b) { patch.outputOn = b; });
    rocker (a440, [this] (bool b) { patch.a440 = b; });
    rocker (glideOn, [this] (bool b) { patch.glideOn = b; });
    rocker (decayOn, [this] (bool b) { patch.decayOn = b; });
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
            unitText (*freq[(size_t) i], [] (double v) { return (v >= 0 ? "+" : "") + juce::String (v, 2) + " semitones"; });
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

    // ---- MIDI learn: every knob and switch has an ID; right-click to assign a controller CC
    registerControl ("tune", tune);
    registerControl ("glide", glide);
    registerControl ("modMix", modMix);
    registerControl ("oscModulation", oscMod);
    registerControl ("osc3Control", osc3Control);
    for (int i = 0; i < 3; ++i)
    {
        const auto n = "osc" + juce::String (i + 1);
        registerControl (n + ".range", *range[(size_t) i]);
        registerControl (n + ".wave", *wave[(size_t) i]);
        if (freq[(size_t) i])
            registerControl (n + ".frequency", *freq[(size_t) i]);
        registerControl (n + ".volume", *oscVol[(size_t) i]);
        registerControl (n + ".on", *oscOn[(size_t) i]);
    }
    registerControl ("extVolume", extVol);
    registerControl ("extOn", extOn);
    registerControl ("noiseVolume", noiseVol);
    registerControl ("noiseOn", noiseOn);
    registerControl ("pinkNoise", pink);
    registerControl ("cutoff", cutoff);
    registerControl ("emphasis", emphasis);
    registerControl ("contourAmount", contourAmt);
    registerControl ("filterModulation", filterMod);
    registerControl ("keyboardControl1", kb1);
    registerControl ("keyboardControl2", kb2);
    registerControl ("filterAttack", fAttack);
    registerControl ("filterDecay", fDecay);
    registerControl ("filterSustain", fSustain);
    registerControl ("loudAttack", lAttack);
    registerControl ("loudDecay", lDecay);
    registerControl ("loudSustain", lSustain);
    registerControl ("volume", volume);
    registerControl ("outputOn", outputOn);
    registerControl ("a440", a440);
    registerControl ("glideOn", glideOn);
    registerControl ("decayOn", decayOn);
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
}

MainComponent::~MainComponent()
{
    if (engine.isRecording())
        engine.stopRecording();
    saveSettings();
    stopTimer();
    engine.onMidiDeviceMessage = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MainComponent::parentHierarchyChanged()
{
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::MessageManager::callAsync ([safe] {
        if (safe != nullptr && safe->isShowing())
            safe->grabKeyboardFocus();
    });
}

// ---------------------------------------------------------------- patch <-> controls

void MainComponent::loadPatchIntoControls()
{
    const juce::ScopedValueSetter<bool> svs (loading, true);
    const auto& p = patch;
    tune.setValue (p.tune);
    glide.setValue (p.glide);
    modMix.setValue (p.modMix);
    oscMod.setToggleState (p.oscModulation, juce::dontSendNotification);
    osc3Control.setToggleState (p.osc3Control, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
    {
        range[(size_t) i]->setValue (p.osc[(size_t) i].range);
        wave[(size_t) i]->setValue (p.osc[(size_t) i].wave);
        if (freq[(size_t) i])
            freq[(size_t) i]->setValue (p.osc[(size_t) i].frequency);
        oscVol[(size_t) i]->setValue (p.oscVolume[(size_t) i]);
        oscOn[(size_t) i]->setToggleState (p.oscOn[(size_t) i], juce::dontSendNotification);
    }
    extVol.setValue (p.extVolume);
    extOn.setToggleState (p.extOn, juce::dontSendNotification);
    noiseVol.setValue (p.noiseVolume);
    noiseOn.setToggleState (p.noiseOn, juce::dontSendNotification);
    pink.setToggleState (p.pinkNoise, juce::dontSendNotification);
    cutoff.setValue (p.cutoff);
    emphasis.setValue (p.emphasis);
    contourAmt.setValue (p.contourAmount);
    filterMod.setToggleState (p.filterModulation, juce::dontSendNotification);
    kb1.setToggleState (p.keyboardControl1, juce::dontSendNotification);
    kb2.setToggleState (p.keyboardControl2, juce::dontSendNotification);
    fAttack.setValue (p.filterAttack);
    fDecay.setValue (p.filterDecay);
    fSustain.setValue (p.filterSustain);
    lAttack.setValue (p.loudAttack);
    lDecay.setValue (p.loudDecay);
    lSustain.setValue (p.loudSustain);
    volume.setValue (p.volume);
    outputOn.setToggleState (p.outputOn, juce::dontSendNotification);
    a440.setToggleState (p.a440, juce::dontSendNotification);
    glideOn.setToggleState (p.glideOn, juce::dontSendNotification);
    decayOn.setToggleState (p.decayOn, juce::dontSendNotification);
    repaint (namePlate);
}

void MainComponent::patchEdited()
{
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
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fourpole Patches");
}

void MainComponent::stepPatch (int dir)
{
    // factory patches, then the ones saved in ~/Music/Fourpole Patches
    juce::Array<juce::File> user;
    if (patchFolder().isDirectory())
        user = patchFolder().findChildFiles (juce::File::findFiles, false, "*.fourpole");
    user.sort();
    const int total = (int) factoryPatches().size() + user.size();
    patchIndex = ((patchIndex + dir) % total + total) % total;
    if (patchIndex < (int) factoryPatches().size())
    {
        patch = factoryPatches()[(size_t) patchIndex];
        currentFile = juce::File();
    }
    else
    {
        currentFile = user[patchIndex - (int) factoryPatches().size()];
        patch = Patch::fromJson (juce::JSON::parse (currentFile));
    }
    loadPatchIntoControls();
    engine.setPatch (patch);
}

void MainComponent::savePatch (bool askName)
{
    auto write = [this] (const juce::File& f) {
        patch.name = f.getFileNameWithoutExtension();
        if (f.replaceWithText (juce::JSON::toString (patch.toJson())))
        {
            currentFile = f;
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
                              auto f = fc.getResult();
                              if (! f.existsAsFile())
                                  return;
                              const auto json = juce::JSON::parse (f);
                              if (json.getProperty ("format", {}).toString() != "fourpole-patch")
                                  return flash ("Not a Fourpole patch");
                              patch = Patch::fromJson (json);
                              currentFile = f;
                              loadPatchIntoControls();
                              engine.setPatch (patch);
                              flash ("Loaded " + f.getFileName());
                          });
}

void MainComponent::showSetupMenu()
{
    // conveniences the hardware didn't have
    juce::PopupMenu m, bend, prio, vel, drift;
    for (int st : { 1, 2, 3, 5, 7, 12 })
        bend.addItem ("+/- " + juce::String (st) + " semitones", true, juce::approximatelyEqual (patch.bendRange, (float) st),
                      [this, st] { patch.bendRange = (float) st; patchEdited(); });
    const char* prios[] = { "Low note (original)", "High note", "Last note" };
    for (int i = 0; i < 3; ++i)
        prio.addItem (prios[i], true, (int) patch.priority == i, [this, i] { patch.priority = (NotePriority) i; patchEdited(); });
    for (auto [label, amt] : { std::pair { "Off (original)", 0.0f }, std::pair { "Light", 0.35f }, std::pair { "Full", 1.0f } })
        vel.addItem (label, true, juce::approximatelyEqual (patch.velocity, amt), [this, a = amt] { patch.velocity = a; patchEdited(); });
    for (auto [label, amt] : { std::pair { "Off (perfectly stable)", 0.0f }, std::pair { "Subtle", 0.5f }, std::pair { "Vintage", 1.0f } })
        drift.addItem (label, true, juce::approximatelyEqual (patch.drift, amt), [this, a = amt] { patch.drift = a; patchEdited(); });
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
    m.addItem ("Init patch", [this] { patch = factoryPatches()[0]; patchIndex = 0; currentFile = juce::File(); loadPatchIntoControls(); engine.setPatch (patch); });
    m.addItem ("All notes off", [this] { engine.allNotesOff(); });
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
        "PATCHES: < > step through factory and saved patches. Save goes to Music/Fourpole Patches. "
        "SETUP: pitch wheel range, note priority, velocity, drift, the external input jack, MIDI assignments.\n\n"
        "MIDI CONTROLLERS: knobs on CC 70-77 (e.g. an MPK Mini's K1-K8) control contour amount, emphasis, loudness decay "
        "and attack, cutoff, filter decay, glide and oscillator 2 frequency. RIGHT-CLICK any knob or switch and choose MIDI learn "
        "to assign it to whatever you move next. A sustain pedal holds notes.\n\n"
        "REC records what you play to Music/Fourpole Recordings. Hover a knob to read its value in Hz, ms or semitones.\n\n"
        "MIDI IN: " + (inputs.isEmpty() ? juce::String ("no controllers connected") : inputs.joinIntoString (", ")) + ".");
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(), 0, 0, 1, 2, true, false, true, false);
    selector->setSize (520, 420);
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

void MainComponent::registerControl (const juce::String& id, PanelKnob& k)
{
    controls[id].knob = &k;
    k.slider.onRightClick = [this, id] { showControlMenu (id); };
}

void MainComponent::registerControl (const juce::String& id, Rocker& r)
{
    controls[id].rocker = &r;
    r.onRightClick = [this, id] { showControlMenu (id); };
}

juce::String MainComponent::controlName (const juce::String& id) const
{
    auto it = controls.find (id);
    if (it == controls.end())
        return id;
    juce::String n = it->second.knob != nullptr ? it->second.knob->getName() : it->second.rocker->getButtonText();
    n = n.replace ("\n", " ");
    if (n.isEmpty())
        n = id;
    // disambiguate the repeated names (contours, oscillators, mixer switches)
    if (id.startsWith ("filter") && (id.endsWith ("Attack") || id.endsWith ("Decay") || id.endsWith ("Sustain")))
        n = "FILTER " + n;
    else if (id.startsWith ("loud"))
        n = "LOUDNESS " + n;
    else if (id.startsWith ("osc") && id.containsChar ('.'))
        n = "OSC " + id.substring (3, 4) + " " + (id.endsWith (".on") ? juce::String ("ON/OFF") : n);
    else if (id == "extOn")
        n = "EXTERNAL INPUT ON/OFF";
    else if (id == "noiseOn")
        n = "NOISE ON/OFF";
    return n;
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
    if (settings->getBoolValue ("externalAudioInput", false))
        engine.setExternalAudioInput (true);
}

void MainComponent::saveSettings()
{
    if (settings == nullptr)
        return;
    settings->setValue ("midiMap", juce::JSON::toString (midiMap.toJson(), true));
    settings->setValue ("externalAudioInput", engine.isUsingExternalAudioInput());
    settings->saveIfNeeded();
}

// ---------------------------------------------------------------- timer / keys

void MainComponent::timerCallback()
{
    overload.setOn (engine.isOverloaded());
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
    if (! modWheel.isBeingDragged() && std::abs (engine.getMidiModWheel() - modWheel.getValue()) > 0.001f && engine.getMidiModWheel() > 0.0f)
        modWheel.setValue (engine.getMidiModWheel());
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
        return false;
    }
    if (c == 'z') { octaveBase = juce::jmax (24, octaveBase - 12); flash ("Computer keyboard: C" + juce::String (octaveBase / 12 - 1)); return true; }
    if (c == 'x') { octaveBase = juce::jmin (84, octaveBase + 12); flash ("Computer keyboard: C" + juce::String (octaveBase / 12 - 1)); return true; }
    if (k == juce::KeyPress::leftKey) { stepPatch (-1); return true; }
    if (k == juce::KeyPress::rightKey) { stepPatch (1); return true; }
    if (k.getTextCharacter() == '?') { showHelp(); return true; }
    if (k == juce::KeyPress::escapeKey && learnTarget.isNotEmpty()) { learnTarget.clear(); flash ("MIDI learn cancelled"); return true; }
    for (auto key : kKeys)
        if (c == (juce::juce_wchar) key)
            return true; // handled in keyStateChanged
    return false;
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
    theme::drawPrint (g, showMsg ? message : patch.name + (currentFile.existsAsFile() ? juce::String() : juce::String ("  (factory)")),
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
