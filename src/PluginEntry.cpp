// Plugin entry point. Kept out of the shared code so the unit tests can link
// the processor without the plugin wrappers.

#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new progdelay::ProgDelayProcessor();
}
