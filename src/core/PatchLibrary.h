#pragma once

// What < and > (and MIDI program changes) step through: the factory patches, then the .fourpole
// files in the patch folder, sorted by name.

#include "core/Patch.h"

namespace fourpole
{
class PatchLibrary
{
public:
    explicit PatchLibrary (juce::File folderToUse) : folder (std::move (folderToUse)) { rescan(); }

    /** Re-reads the folder (after a save, or files added in Finder). */
    void rescan();
    const juce::File& getFolder() const { return folder; }

    int size() const;
    int numFactory() const;
    bool isFactory (int index) const { return index >= 0 && index < numFactory(); }
    /** The file for a user patch; none for factory ones. */
    juce::File fileAt (int index) const;
    /** Position of a file in the list, or -1 if it isn't in the folder. */
    int indexOf (const juce::File& f) const;
    /** index + step, wrapping around the whole list. */
    int step (int index, int by) const;

    /** Loads entry `index`. A file patch is named after its file. False if it can't be read. */
    bool load (int index, Patch& out) const;
    /** Reads any patch file (from the folder or not). False if it isn't a Fourpole patch. */
    static bool loadFile (const juce::File& f, Patch& out);

private:
    juce::File folder;
    juce::Array<juce::File> files;
};
} // namespace fourpole
