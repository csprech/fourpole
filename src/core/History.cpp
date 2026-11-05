#include "core/History.h"

namespace fourpole
{
void History::remember (const PanelState& before, const juce::String& what, uint32_t nowMs, uint32_t coalesceMs)
{
    const bool sameGesture = what == lastWhat && ! undos.empty() && nowMs - lastMs < coalesceMs;
    lastWhat = what;
    lastMs = nowMs;
    redos.clear(); // a new edit: what was undone can't be redone any more
    if (sameGesture)
        return;
    undos.push_back ({ before, what });
    while (undos.size() > kMaxSteps)
        undos.pop_front();
}

std::optional<PanelState> History::undo (const PanelState& current, juce::String& what)
{
    if (undos.empty())
        return std::nullopt;
    auto step = std::move (undos.back());
    undos.pop_back();
    redos.push_back ({ current, step.what });
    what = step.what;
    lastWhat.clear(); // the next edit is a new step, even on the same control
    return std::move (step.state);
}

std::optional<PanelState> History::redo (const PanelState& current, juce::String& what)
{
    if (redos.empty())
        return std::nullopt;
    auto step = std::move (redos.back());
    redos.pop_back();
    undos.push_back ({ current, step.what });
    what = step.what;
    lastWhat.clear();
    return std::move (step.state);
}

void History::clear()
{
    undos.clear();
    redos.clear();
    lastWhat.clear();
}
} // namespace fourpole
