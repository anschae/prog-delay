#pragma once
#include "Widgets.h"

namespace progdelay
{
class ProgDelayProcessor;

/** Preset browser (prev / name menu / next) and A/B compare. */
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (ProgDelayProcessor& p);
    ~PresetBar() override;
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void askSaveName();
    void refresh();

    ProgDelayProcessor& proc;
    juce::TextButton prev { juce::CharPointer_UTF8 ("\xe2\x97\x82") }, next { juce::CharPointer_UTF8 ("\xe2\x96\xb8") }, name;
    Segmented ab { juce::StringArray { "A", "B" } };
};
} // namespace progdelay
