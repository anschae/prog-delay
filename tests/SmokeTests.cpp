#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"

TEST_CASE ("Processor can be created and processes a block", "[smoke]")
{
    progdelay::ProgDelayProcessor p;
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    buf.clear();
    juce::MidiBuffer midi;
    p.processBlock (buf, midi);
    REQUIRE (buf.getMagnitude (0, 512) == 0.0f);
}
