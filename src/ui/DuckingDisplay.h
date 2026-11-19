#pragma once
#include "Theme.h"

namespace progdelay
{
class ProgDelayProcessor;

/** Scrolling detector level, threshold line and gain-reduction curve (milestone 6). */
class DuckingDisplay : public juce::Component
{
public:
    explicit DuckingDisplay (ProgDelayProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::plot);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }

private:
    ProgDelayProcessor& proc;
};
} // namespace progdelay
