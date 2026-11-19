#pragma once
// Volume grid (top) and pan grid (bottom). Both use GridModel for the column
// geometry so they line up exactly.

#include "GridModel.h"
#include "Theme.h"

namespace progdelay
{
/** Common drawing: vertical subdivision lines, bar markers, playhead. */
class GridBase : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GridBase (GridModel& m) : model (m) { setRepaintsOnMouseActivity (false); }

protected:
    juce::Rectangle<float> plotArea() const;
    void drawVerticalLines (juce::Graphics&, juce::Rectangle<float> plot, bool withLabels, bool withMarkerLabels);
    void drawPlayhead (juce::Graphics&, juce::Rectangle<float> plot);
    juce::Colour colourFor (const Reflection& r) const;
    bool onGrid (const Reflection& r) const;

    GridModel& model;
    int topPad = 26, bottomPad = 8;
};

class VolumeGrid : public GridBase
{
public:
    explicit VolumeGrid (GridModel& m) : GridBase (m) { topPad = 30; bottomPad = 6; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    float yToVolume (float y) const;
    float volumeToY (float v) const;
    void paintAt (int tick, float volume);

    enum class Mode { None, Volume, Move, Paint };
    Mode mode = Mode::None;
    int dragTick = -1;
    float lastY = 0.0f;
    int lastPaintTick = -1;
    int hoverTick = -1;
    float hoverVol = 0.5f;
};

class PanGrid : public GridBase
{
public:
    explicit PanGrid (GridModel& m) : GridBase (m) { topPad = 20; bottomPad = 20; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    float panToY (float pan) const;
    int dragTick = -1;
    float lastY = 0.0f;
};
} // namespace progdelay
