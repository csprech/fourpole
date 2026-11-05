#pragma once

// Undo / redo for panel edits: snapshots of the panel (the patch, and which patch file it is).

#include "Patch.h"
#include <deque>
#include <optional>

namespace fourpole
{
struct PanelState
{
    Patch patch;
    juce::String file;  // the patch file it came from (full path), or empty: factory / never saved
    int patchIndex = 0; // where < and > step from
    bool edited = false;
};

class History
{
public:
    static constexpr size_t kMaxSteps = 200;

    /** Call just before the panel changes, with the state it's in now. Changes to the same thing
        (`what`, e.g. a control ID) within `coalesceMs` of each other are one step: a knob turn is
        many small changes. */
    void remember (const PanelState& before, const juce::String& what, uint32_t nowMs, uint32_t coalesceMs = 1500);

    /** The state to go back to (the current one becomes redoable), or none. `what` says what's undone. */
    std::optional<PanelState> undo (const PanelState& current, juce::String& what);
    std::optional<PanelState> redo (const PanelState& current, juce::String& what);

    bool canUndo() const { return ! undos.empty(); }
    bool canRedo() const { return ! redos.empty(); }
    void clear();

private:
    struct Step
    {
        PanelState state;
        juce::String what;
    };
    std::deque<Step> undos, redos;
    juce::String lastWhat;
    uint32_t lastMs = 0;
};
} // namespace fourpole
