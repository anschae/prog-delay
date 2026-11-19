#pragma once
// Presets (factory + user files) and A/B compare. Message thread only.
// User presets are XML files (*.pdpreset) with the full plugin state, in
//   <user app data>/anschae/Prog Delay/Presets
// (macOS: ~/Library/Application Support/..., Windows: %APPDATA%\...).

#include <juce_audio_processors/juce_audio_processors.h>

namespace progdelay
{
class ProgDelayProcessor;

class PresetManager
{
public:
    explicit PresetManager (ProgDelayProcessor& p);

    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
        juce::String displayName() const { return (factory ? "Factory / " : "User / ") + name; }
    };

    /** Factory presets first, then user presets (alphabetical). Rescans the folder. */
    std::vector<Entry> list() const;

    bool load (const Entry& e);
    void loadNext (int direction);
    bool saveUser (const juce::String& name);   // overwrites an existing file of that name
    bool deleteUser (const Entry& e);

    juce::String getCurrentName() const;
    juce::String getCurrentDisplayName() const;

    static juce::File getUserFolder();
    /** Tests can redirect the user folder. */
    static void setUserFolderOverride (const juce::File& f);

    // ---- A/B compare
    int getActiveAB() const noexcept { return activeAB; }
    void switchAB (int index);
    void copyActiveToOther();

    std::function<void()> onChange;

private:
    ProgDelayProcessor& proc;
    juce::ValueTree ab[2];
    int activeAB = 0;
};
} // namespace progdelay
