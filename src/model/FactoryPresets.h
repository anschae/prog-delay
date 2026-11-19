#pragma once
// Built-in presets. Each is a full state tree (as StateSerializer::makeState
// would produce), built in code so it always matches the current format.

#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace progdelay
{
struct FactoryPreset
{
    juce::String name;
    juce::ValueTree state;
};

namespace FactoryPresets
{
const std::vector<FactoryPreset>& all();
}
} // namespace progdelay
