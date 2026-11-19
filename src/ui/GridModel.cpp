#include "GridModel.h"
#include "PluginProcessor.h"

namespace progdelay
{
GridModel::GridModel (ProgDelayProcessor& p) : processor (p), bank (p.getBank())
{
    bank.getTree().addListener (this);
    refresh();
}

GridModel::~GridModel() { bank.getTree().removeListener (this); }

void GridModel::refresh()
{
    cached = bank.getPattern (slot());
    visibleRefl.clear();
    for (const auto& r : cached.reflections)
        if (r.tick <= cached.lengthTicks())
            visibleRefl.push_back (r);
    clickPositions = PatternBank::gridPositions (cached, bank.getSnapMode());
    allGridPositions = PatternBank::gridPositions (cached, SnapMode::Both);
    if (selectedTick >= 0 && std::none_of (visibleRefl.begin(), visibleRefl.end(), [this] (auto& r) { return r.tick == selectedTick; }))
        selectedTick = -1;
    notify();
}

float GridModel::tickToX (double tick, int w) const noexcept
{
    const float plotW = (float) (w - kGutterLeft - kGutterRight);
    return (float) kGutterLeft + (float) (tick / cached.lengthTicks()) * plotW;
}

double GridModel::xToTick (float x, int w) const noexcept
{
    const float plotW = (float) (w - kGutterLeft - kGutterRight);
    return (x - (float) kGutterLeft) / plotW * cached.lengthTicks();
}

float GridModel::columnWidth (int w) const noexcept
{
    // spacing of the finest visible grid
    int unit = cached.mode == GridMode::Triplet ? tripletUnit (cached.resolution) : straightUnit (cached.resolution);
    if (cached.mode == GridMode::Combined)
        unit = tripletUnit (cached.resolution) / 2;  // straight/triplet neighbours can be 1/3 apart
    const float spacing = tickToX (unit, w) - tickToX (0, w);
    return juce::jlimit (3.0f, 34.0f, spacing * (cached.mode == GridMode::Combined ? 1.1f : 0.55f));
}

int GridModel::nearestPosition (float x, int w, bool clickableOnly) const
{
    const auto& pos = clickableOnly ? clickPositions : allGridPositions;
    int best = -1;
    float bestD = 1.0e9f;
    for (int t : pos)
    {
        const float d = std::abs (tickToX (t, w) - x);
        if (d < bestD)
        {
            bestD = d;
            best = t;
        }
    }
    return best;
}

int GridModel::nearestReflection (float x, int w) const
{
    int best = -1;
    float bestD = 1.0e9f;
    for (const auto& r : visibleRefl)
    {
        const float d = std::abs (tickToX (r.tick, w) - x);
        if (d < bestD)
        {
            bestD = d;
            best = r.tick;
        }
    }
    return best;
}

int GridModel::hitReflection (float x, int w) const
{
    const int r = nearestReflection (x, w);
    if (r < 0)
        return -1;
    const float dr = std::abs (tickToX (r, w) - x);
    const int c = nearestPosition (x, w);
    const float dc = c >= 0 ? std::abs (tickToX (c, w) - x) : 1.0e9f;
    if (c == r || dr <= dc + 0.5f)
        return dr <= std::max (columnWidth (w) * 0.5f + 3.0f, 6.0f) || c == r ? r : -1;
    return -1;
}
} // namespace progdelay
