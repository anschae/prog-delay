#pragma once
#include <functional>
#include <memory>
#include <vector>
#include "SoundMode.h"

namespace progdelay
{
/** Global list of sound-mode factories. The mode choice parameter and the UI
    selector are built from it. Index in the list == parameter index. */
class SoundModeRegistry
{
public:
    struct Entry
    {
        juce::String id, displayName;
        std::function<std::unique_ptr<SoundMode>()> create;
    };

    static const SoundModeRegistry& get();

    int size() const noexcept { return (int) entries.size(); }
    const Entry& operator[] (int i) const { return entries[(size_t) i]; }
    juce::StringArray getDisplayNames() const;
    /** Index for a stored ID, or -1. */
    int indexOf (const juce::String& id) const;

    template <typename ModeClass>
    void add()
    {
        auto probe = std::make_unique<ModeClass>();
        entries.push_back ({ probe->getId(), probe->getDisplayName(), [] { return std::make_unique<ModeClass>(); } });
    }

private:
    SoundModeRegistry();
    std::vector<Entry> entries;
};
} // namespace progdelay
