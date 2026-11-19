#pragma once
// Shared state of the two grids: the cached edited pattern, the selection and
// the geometry that maps ticks to x (identical for both grids, so their
// columns line up).

#include <juce_gui_basics/juce_gui_basics.h>
#include "model/PatternBank.h"

namespace progdelay
{
class ProgDelayProcessor;

class GridModel : private juce::ValueTree::Listener
{
public:
    explicit GridModel (ProgDelayProcessor& p);
    ~GridModel() override;

    ProgDelayProcessor& processor;
    PatternBank& bank;

    int slot() const { return bank.getEditedSlot(); }
    const Pattern& pattern() const noexcept { return cached; }
    /** Reflections inside the grid length, sorted by tick (index == tap index). */
    const std::vector<Reflection>& visible() const noexcept { return visibleRefl; }
    const std::vector<int>& positions() const noexcept { return clickPositions; }  // snap-filtered
    const std::vector<int>& allPositions() const noexcept { return allGridPositions; }

    int selectedTick = -1;
    std::function<void()> onChange;    // pattern / selection changed → repaint
    void notify() { if (onChange) onChange(); }

    // ---- geometry: plot spans [plotX, plotX + plotW] in the grid components' coordinates
    static constexpr int kGutterLeft = 64, kGutterRight = 16;
    float tickToX (double tick, int componentWidth) const noexcept;
    double xToTick (float x, int componentWidth) const noexcept;
    float columnWidth (int componentWidth) const noexcept;   // bar width
    int nearestPosition (float x, int componentWidth, bool clickableOnly = true) const;
    int nearestReflection (float x, int componentWidth) const;  // -1 if none
    int hitReflection (float x, int componentWidth) const;      // prefers reflection if closest

    juce::ValueTree reflTree (int tick) const { return bank.findReflection (slot(), tick); }

    // live info, refreshed by the panel's timer
    std::array<float, kMaxReflections> flash {};
    int hostSigNum = 4, hostSigDen = 4;
    bool playing = false;
    double ppqInBar = 0.0;
    double bpm = 120.0;

    void refresh();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { refresh(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { refresh(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { refresh(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { refresh(); }
    void valueTreeRedirected (juce::ValueTree&) override { refresh(); }

    Pattern cached;
    std::vector<Reflection> visibleRefl;
    std::vector<int> clickPositions, allGridPositions;
};
} // namespace progdelay
