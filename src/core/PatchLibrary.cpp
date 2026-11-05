#include "core/PatchLibrary.h"

namespace fourpole
{
void PatchLibrary::rescan()
{
    files.clearQuick();
    if (folder.isDirectory())
        files = folder.findChildFiles (juce::File::findFiles, false, "*.fourpole");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
}

int PatchLibrary::numFactory() const { return (int) factoryPatches().size(); }
int PatchLibrary::size() const { return numFactory() + files.size(); }

juce::File PatchLibrary::fileAt (int index) const
{
    const int u = index - numFactory();
    return u >= 0 && u < files.size() ? files[u] : juce::File();
}

int PatchLibrary::indexOf (const juce::File& f) const
{
    for (int i = 0; i < files.size(); ++i)
        if (files[i] == f)
            return numFactory() + i;
    return -1;
}

int PatchLibrary::step (int index, int by) const
{
    const int n = size();
    return ((index + by) % n + n) % n;
}

bool PatchLibrary::loadFile (const juce::File& f, Patch& out)
{
    const auto json = juce::JSON::parse (f);
    if (json.getProperty ("format", {}).toString() != "fourpole-patch")
        return false;
    out = Patch::fromJson (json);
    out.name = f.getFileNameWithoutExtension(); // the name you see in Finder is the name on the panel
    return true;
}

bool PatchLibrary::load (int index, Patch& out) const
{
    if (isFactory (index))
    {
        out = factoryPatches()[(size_t) index];
        return true;
    }
    const auto f = fileAt (index);
    return f.existsAsFile() && loadFile (f, out);
}
} // namespace fourpole
