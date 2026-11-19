#pragma once
#include "Widgets.h"

namespace progdelay
{
class ProgDelayProcessor;

/** Preset browser + A/B compare (filled in by milestone 7). */
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (ProgDelayProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::control);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::panelEdge);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (14.0f));
        g.drawText ("Init", getLocalBounds(), juce::Justification::centred);
    }

private:
    ProgDelayProcessor& proc;
};
} // namespace progdelay
