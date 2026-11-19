#pragma once
// The main editing panel (volume + pan grids) and the randomize panel beside it.

#include "VolumeGrid.h"
#include "Widgets.h"

namespace progdelay
{
class GridPanel : public juce::Component, private juce::Timer
{
public:
    explicit GridPanel (GridModel& m);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    GridModel& model;
    VolumeGrid volume;
    PanGrid pan;
    int lastPlaying = -1;
};

class RandomizePanel : public juce::Component, private juce::Timer
{
public:
    explicit RandomizePanel (GridModel& m);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    GridModel& model;
    juce::TextButton all { "Randomize All" }, vol { "Randomize Volume" }, pan { "Randomize Pan" };
    Knob amount, density;
    juce::Random rng;
    juce::String lastInfo;
};
} // namespace progdelay
