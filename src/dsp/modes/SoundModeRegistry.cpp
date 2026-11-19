#include "SoundModeRegistry.h"
#include "DigitalMode.h"

namespace progdelay
{
SoundModeRegistry::SoundModeRegistry()
{
    // ---- Registration: APPEND new modes at the end, never reorder or remove. ----
    add<DigitalMode>();
}

const SoundModeRegistry& SoundModeRegistry::get()
{
    static const SoundModeRegistry instance;
    return instance;
}

juce::StringArray SoundModeRegistry::getDisplayNames() const
{
    juce::StringArray names;
    for (const auto& e : entries)
        names.add (e.displayName);
    return names;
}

int SoundModeRegistry::indexOf (const juce::String& id) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].id == id)
            return (int) i;
    return -1;
}
} // namespace progdelay
