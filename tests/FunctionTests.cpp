// The instrument's functions around the sound: undo / redo, and the patch list that < > and MIDI
// program changes step through.

#include "../src/core/History.h"
#include "../src/core/PatchLibrary.h"
#include <juce_core/juce_core.h>

using namespace fourpole;

namespace
{
PanelState withCutoff (float c)
{
    PanelState s;
    s.patch.cutoff = c;
    return s;
}
} // namespace

struct FunctionTests : juce::UnitTest
{
    FunctionTests() : juce::UnitTest ("Functions", "fourpole") {}

    void runTest() override
    {
        beginTest ("undo / redo: a knob turn is one step; different controls are separate steps");
        {
            History h;
            juce::String what;
            expect (! h.canUndo() && ! h.undo (withCutoff (0), what));
            // a knob turned through many values within a gesture
            h.remember (withCutoff (0.0f), "cutoff", 1000);
            h.remember (withCutoff (0.5f), "cutoff", 1100);
            h.remember (withCutoff (1.0f), "cutoff", 1200);
            // then another control
            PanelState e = withCutoff (1.5f);
            h.remember (e, "emphasis", 1300);
            e.patch.emphasis = 6.0f;

            auto back = h.undo (e, what);
            expect (back.has_value() && what == "emphasis");
            expectEquals (back->patch.emphasis, 0.0f, "emphasis undone");
            expectEquals (back->patch.cutoff, 1.5f, "the cutoff stays where it was turned to");
            back = h.undo (*back, what);
            expect (back.has_value() && what == "cutoff");
            expectEquals (back->patch.cutoff, 0.0f, "the whole cutoff turn goes in one undo");
            expect (! h.canUndo());

            auto fwd = h.redo (*back, what);
            expect (fwd.has_value() && what == "cutoff");
            expectEquals (fwd->patch.cutoff, 1.5f);
            fwd = h.redo (*fwd, what);
            expectEquals (fwd->patch.emphasis, 6.0f, "redo brings both back");
            expect (! h.canRedo());
        }

        beginTest ("undo: the same control after a pause is a new step; a new edit clears redo; capped size");
        {
            History h;
            juce::String what;
            h.remember (withCutoff (0.0f), "cutoff", 1000);
            h.remember (withCutoff (1.0f), "cutoff", 5000); // 4 s later: a new gesture
            auto s = h.undo (withCutoff (2.0f), what);
            expectEquals (s->patch.cutoff, 1.0f);
            s = h.undo (*s, what);
            expectEquals (s->patch.cutoff, 0.0f);

            h.remember (*s, "volume", 6000);
            expect (! h.canRedo(), "a new edit after undoing drops the redo steps");

            History big;
            for (int i = 0; i < (int) History::kMaxSteps + 50; ++i)
                big.remember (withCutoff ((float) i), "c" + juce::String (i), (uint32_t) i * 10000);
            int n = 0;
            PanelState cur;
            while (auto st = big.undo (cur, what))
            {
                cur = *st;
                ++n;
            }
            expectEquals (n, (int) History::kMaxSteps, "keeps the last 200 steps");
            expectEquals (cur.patch.cutoff, 50.0f, "the oldest ones are dropped");
        }

        beginTest ("patch list: factory patches, then the folder's files in name order; stepping wraps");
        {
            auto dir = juce::File::createTempFile ("fourpole-patches");
            dir.createDirectory();
            for (auto* n : { "Zap", "bass 10", "bass 2", "Alpha" })
            {
                Patch p;
                p.cutoff = 1.25f;
                dir.getChildFile (juce::String (n) + ".fourpole").replaceWithText (juce::JSON::toString (p.toJson()));
            }
            dir.getChildFile ("notes.txt").replaceWithText ("not a patch");
            dir.getChildFile ("Broken.fourpole").replaceWithText ("{ \"format\": \"something else\" }");

            PatchLibrary lib (dir);
            const int f = lib.numFactory();
            expectEquals (lib.size(), f + 5, "the five .fourpole files (not the .txt)");
            expect (lib.isFactory (0) && ! lib.isFactory (f));
            expectEquals (lib.fileAt (f).getFileName(), juce::String ("Alpha.fourpole"));
            expectEquals (lib.fileAt (f + 1).getFileName(), juce::String ("bass 2.fourpole"), "natural order: 2 before 10");
            expectEquals (lib.fileAt (f + 2).getFileName(), juce::String ("bass 10.fourpole"));
            expectEquals (lib.indexOf (dir.getChildFile ("Zap.fourpole")), f + 4);
            expectEquals (lib.indexOf (juce::File ("/nowhere/else.fourpole")), -1);

            Patch p;
            expect (lib.load (f + 1, p));
            expectEquals (p.name, juce::String ("bass 2"), "named after its file");
            expectEquals (p.cutoff, 1.25f);
            expect (lib.load (1, p) && p.name == factoryPatches()[1].name);
            expect (! lib.load (lib.indexOf (dir.getChildFile ("Broken.fourpole")), p), "not a Fourpole patch");

            expectEquals (lib.step (lib.size() - 1, 1), 0, "> from the last wraps to the first");
            expectEquals (lib.step (0, -1), lib.size() - 1, "< from the first wraps to the last");

            dir.getChildFile ("Beta.fourpole").replaceWithText (juce::JSON::toString (Patch().toJson()));
            lib.rescan();
            // (case-insensitive, like Finder: Alpha, bass 2, bass 10, Beta, Zap)
            expectEquals (lib.indexOf (dir.getChildFile ("Beta.fourpole")), f + 3, "a newly saved patch takes its place in the order");
            dir.deleteRecursively();
        }
    }
};

static FunctionTests functionTests;
