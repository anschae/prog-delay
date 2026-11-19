#pragma once
#include "PluginProcessor.h"

namespace progdelay
{
class ProgDelayEditor : public juce::AudioProcessorEditor
{
public:
    explicit ProgDelayEditor (ProgDelayProcessor& p) : AudioProcessorEditor (p) { setSize (1280, 820); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff12151b));
        g.setColour (juce::Colours::white);
        g.setFont (32.0f);
        g.drawText ("PROG DELAY", getLocalBounds(), juce::Justification::centred);
    }
};
}
