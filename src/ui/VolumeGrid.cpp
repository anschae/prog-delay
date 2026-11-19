#include "VolumeGrid.h"
#include "PluginProcessor.h"

namespace progdelay
{
using namespace theme;

// ============================================================================ GridBase
juce::Rectangle<float> GridBase::plotArea() const
{
    return { (float) GridModel::kGutterLeft, (float) topPad,
             (float) (getWidth() - GridModel::kGutterLeft - GridModel::kGutterRight),
             (float) (getHeight() - topPad - bottomPad) };
}

juce::Colour GridBase::colourFor (const Reflection& r) const
{
    return r.type == ReflType::Triplet ? triplet : straight;
}

bool GridBase::onGrid (const Reflection& r) const
{
    const auto& p = model.pattern();
    return isOnGrid (r.tick, p.resolution, p.mode);
}

void GridBase::drawVerticalLines (juce::Graphics& g, juce::Rectangle<float> plot, bool withLabels, bool withMarkerLabels)
{
    const auto& p = model.pattern();
    const int w = getWidth();
    const int len = p.lengthTicks();
    const int su = straightUnit (p.resolution), tu = tripletUnit (p.resolution);
    const float dash[] = { 3.0f, 3.0f };

    // triplet subdivisions: orange dashed
    if (p.mode != GridMode::Straight)
        for (int t = tu; t < len; t += tu)
        {
            if (p.mode == GridMode::Combined && t % su == 0)
                continue;
            const float x = model.tickToX (t, w);
            g.setColour (triplet.withAlpha (p.mode == GridMode::Combined ? 0.28f : 0.35f));
            juce::Line<float> l (x, plot.getY(), x, plot.getBottom());
            g.drawDashedLine (l, dash, 2, 1.0f);
        }

    // straight subdivisions (in Triplet mode: quarter notes only, for orientation)
    const int sStep = p.mode == GridMode::Triplet ? straightUnit (4) : su;
    for (int t = sStep; t < len; t += sStep)
    {
        const float x = model.tickToX (t, w);
        const bool beat = t % straightUnit (4) == 0;
        g.setColour (beat ? gridLine.brighter (0.25f) : gridLine);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    // labels along the top
    if (withLabels)
    {
        g.setFont (font (12.0f));
        const int labelStep = p.mode == GridMode::Triplet ? tu : su;
        const float spacing = model.tickToX (labelStep, w) - model.tickToX (0, w);
        const int every = std::max (1, (int) std::ceil (30.0f / std::max (1.0f, spacing)));
        int k = 0;
        for (int t = labelStep; t <= len; t += labelStep)
        {
            ++k;
            if (k % every != 0)
                continue;
            const float x = model.tickToX (t, w);
            juce::String s = p.mode == GridMode::Triplet ? juce::String (k) + "T"
                                                          : juce::String (k) + "/" + juce::String (p.resolution);
            const bool beat = t % straightUnit (4) == 0;
            g.setColour (p.mode == GridMode::Triplet ? triplet.withAlpha (0.8f) : (beat ? textDim : straight.withAlpha (0.7f)));
            g.drawText (s, juce::Rectangle<float> (x - 22.0f, 2.0f, 44.0f, 16.0f), juce::Justification::centred, false);
        }
    }

    // bar markers: host time signature dashed, pattern signature solid
    auto marker = [&] (int barLen, juce::Colour c, bool dashed, const juce::String& sigText, bool isHost, int labelRow) {
        if (barLen <= 0)
            return;
        int k = 1;
        for (int t = barLen; t <= len; t += barLen, ++k)
        {
            const float x = model.tickToX (t, w);
            juce::Line<float> l (x, plot.getY() - (withMarkerLabels ? 4.0f : 0.0f), x, plot.getBottom());
            g.setColour (c.withAlpha (0.85f));
            if (dashed)
            {
                const float d[] = { 5.0f, 4.0f };
                g.drawDashedLine (l, d, 2, 1.6f);
            }
            else
                g.drawLine (l, 1.6f);

            if (withMarkerLabels)
            {
                const juce::String label = k == 1 ? (isHost ? juce::String (juce::CharPointer_UTF8 ("BAR END \xc2\xb7 ")) + sigText + " (host)"
                                                          : sigText + " (pattern)")
                                                  : juce::String (k) + juce::String (juce::CharPointer_UTF8 (" \xc3\x97 ")) + sigText;
                g.setFont (font (11.5f, true));
                const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 12.0f;
                auto box = juce::Rectangle<float> (x + 4.0f, plot.getY() + 6.0f + labelRow * 22.0f, tw, 18.0f);
                if (box.getRight() > plot.getRight())
                    box.setX (x - 4.0f - tw);
                g.setColour (c.withAlpha (0.16f));
                g.fillRoundedRectangle (box, 3.0f);
                g.setColour (c);
                g.drawText (label, box, juce::Justification::centred, false);
            }
        }
    };

    const int hostBarLen = barTicks (model.hostSigNum, model.hostSigDen);
    marker (hostBarLen, hostBar, true, juce::String (model.hostSigNum) + "/" + juce::String (model.hostSigDen), true, 0);
    if (p.sig.isValid())
    {
        const int patBarLen = barTicks (p.sig.num, p.sig.den);
        if (patBarLen != hostBarLen || p.sig.den != model.hostSigDen)
            marker (patBarLen, p.colour, false, p.sig.toString(), false, 1);
    }
}

void GridBase::drawPlayhead (juce::Graphics& g, juce::Rectangle<float> plot)
{
    if (! model.playing)
        return;
    const double tick = std::fmod (model.ppqInBar / 4.0 * TICKS_PER_WHOLE, (double) model.pattern().lengthTicks());
    const float x = model.tickToX (tick, getWidth());
    g.setColour (textDim.withAlpha (0.6f));
    g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
}

// ============================================================================ VolumeGrid
float VolumeGrid::volumeToY (float v) const
{
    const auto plot = plotArea();
    return plot.getBottom() - v * plot.getHeight();
}

float VolumeGrid::yToVolume (float y) const
{
    const auto plot = plotArea();
    return juce::jlimit (0.0f, 1.0f, (plot.getBottom() - y) / plot.getHeight());
}

void VolumeGrid::paint (juce::Graphics& g)
{
    const auto plot = plotArea();
    const int w = getWidth();
    g.setColour (plot.isEmpty() ? bg : theme::plot);
    g.fillRect (plot);

    // horizontal lines + labels
    g.setFont (font (12.0f));
    for (int pct : { 0, 25, 50, 75, 100 })
    {
        const float y = volumeToY (pct / 100.0f);
        g.setColour (gridLine.withAlpha (pct == 0 ? 1.0f : 0.6f));
        const float dash[] = { 2.0f, 4.0f };
        if (pct == 0 || pct == 100)
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        else
            g.drawDashedLine ({ plot.getX(), y, plot.getRight(), y }, dash, 2, 1.0f);
        g.setColour (textFaint);
        g.drawText (juce::String (pct) + "%", juce::Rectangle<float> (0.0f, y - 8.0f, (float) GridModel::kGutterLeft - 10.0f, 16.0f),
                    juce::Justification::centredRight, false);
    }

    drawVerticalLines (g, plot, true, true);

    const float bw = model.columnWidth (w);
    const bool roomForLabels = bw >= 11.0f;

    // hover ghost
    if (hoverTick > 0 && mode == Mode::None && ! model.reflTree (hoverTick).isValid())
    {
        const float x = model.tickToX (hoverTick, w);
        const bool trip = classifyTick (hoverTick) == ReflType::Triplet;
        const auto c = trip ? triplet : straight;
        auto r = juce::Rectangle<float> (x - bw * 0.5f, volumeToY (hoverVol), bw, plot.getBottom() - volumeToY (hoverVol));
        juce::Path outline;
        outline.addRectangle (r);
        juce::Path dashed;
        const float d[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, d, 2);
        g.setColour (c.withAlpha (0.8f));
        g.fillPath (dashed);
        const auto& p = model.pattern();
        const juce::String lbl = "+ " + positionLabel (hoverTick) + (trip ? "  (1/" + juce::String (p.resolution) + "T)" : "");
        g.setFont (font (12.0f));
        g.drawText (lbl, juce::Rectangle<float> (x - 70.0f, r.getY() - 20.0f, 140.0f, 16.0f), juce::Justification::centred, false);
    }

    // reflections
    const auto& refl = model.visible();
    for (size_t i = 0; i < refl.size(); ++i)
    {
        const auto& r = refl[i];
        const float x = model.tickToX (r.tick, w);
        const auto c = colourFor (r);
        const float top = volumeToY (r.volume);
        auto bar = juce::Rectangle<float> (x - bw * 0.5f, top, bw, plot.getBottom() - top);
        const bool sel = r.tick == model.selectedTick;
        const float fl = model.flash[i];

        if (onGrid (r))
        {
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.95f), bar.getX(), bar.getY(),
                                                     c.darker (0.45f).withAlpha (0.9f), bar.getX(), bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (c.brighter (0.6f));
            g.fillRect (bar.withHeight (std::min (3.0f, bar.getHeight())));
        }
        else
        {
            // off-grid: outlined until moved onto the grid
            g.setColour (c.withAlpha (0.12f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (c);
            g.drawRoundedRectangle (bar.reduced (0.5f), 2.0f, 1.4f);
        }

        if (fl > 0.01f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f * fl));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (c.withAlpha (0.5f * fl));
            g.fillEllipse (juce::Rectangle<float> (bw + 14.0f, bw + 14.0f).withCentre ({ x, top }));
        }

        if (sel)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (bar.expanded (3.0f, 3.0f), 4.0f, 1.5f);
        }

        if (roomForLabels || sel)
        {
            g.setColour (c);
            g.setFont (font (13.5f, true));
            g.drawText (juce::String (juce::roundToInt (r.volume * 100.0f)) + "%",
                        juce::Rectangle<float> (x - 30.0f, top - 21.0f, 60.0f, 16.0f), juce::Justification::centred, false);
        }
    }

    // editing bubble
    if (mode == Mode::Volume || mode == Mode::Move || mode == Mode::Paint)
    {
        auto rt = model.reflTree (dragTick);
        if (rt.isValid())
        {
            const float x = model.tickToX (dragTick, w);
            const float vol = (float) rt.getProperty (ids::volume);
            const float y = volumeToY (vol);
            auto box = juce::Rectangle<float> (x + bw * 0.5f + 8.0f, y + 6.0f, 150.0f, 44.0f);
            if (box.getRight() > plot.getRight())
                box.setX (x - bw * 0.5f - 8.0f - box.getWidth());
            box.setY (juce::jlimit (plot.getY(), plot.getBottom() - box.getHeight(), box.getY()));
            g.setColour (juce::Colour (0xf0232935));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (panelEdge.brighter (0.2f));
            g.drawRoundedRectangle (box, 5.0f, 1.0f);
            g.setColour (text);
            g.setFont (font (14.0f, true));
            g.drawText (positionLabel (dragTick) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + juce::String (juce::roundToInt (vol * 100.0f)) + " %",
                        box.reduced (10.0f, 4.0f).removeFromTop (19.0f), juce::Justification::centredLeft, false);
            g.setColour (textDim);
            g.setFont (font (12.0f));
            g.drawText (mode == Mode::Move ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x94 move in time"))
                                           : juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x95 drag  \xc2\xb7  \xe2\x87\xa7 fine  \xc2\xb7  \xe2\x8c\xa5 move")),
                        box.reduced (10.0f, 4.0f).removeFromBottom (17.0f), juce::Justification::centredLeft, false);
        }
    }

    drawPlayhead (g, plot);
}

void VolumeGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const int w = getWidth();
    const auto x = (float) e.x;
    if (! plotArea().expanded (0.0f, (float) topPad).contains (e.position))
        return;

    const int hit = model.hitReflection (x, w);
    auto& bank = model.bank;
    const int slot = model.slot();

    if (e.mods.isPopupMenu())
    {
        if (hit > 0)
        {
            bank.beginEdit ("Delete reflection");
            bank.removeReflection (slot, model.reflTree (hit));
        }
        return;
    }

    lastY = (float) e.y;
    if (hit > 0)
    {
        model.selectedTick = hit;
        dragTick = hit;
        mode = e.mods.isAltDown() ? Mode::Move : Mode::Volume;
        bank.beginEdit (mode == Mode::Move ? "Move reflection" : "Change volume");
        model.notify();
        return;
    }

    const int c = model.nearestPosition (x, w);
    if (c <= 0)
        return;
    bank.beginEdit ("Add reflection");
    if (bank.addReflection (slot, c, yToVolume ((float) e.y)).isValid())
    {
        mode = Mode::Paint;
        dragTick = c;
        lastPaintTick = c;
        model.selectedTick = c;
        model.notify();
    }
}

void VolumeGrid::paintAt (int tick, float volume)
{
    auto& bank = model.bank;
    auto rt = model.reflTree (tick);
    if (rt.isValid())
        bank.setVolume (rt, volume);
    else
        bank.addReflection (model.slot(), tick, volume);
}

void VolumeGrid::mouseDrag (const juce::MouseEvent& e)
{
    const int w = getWidth();
    auto& bank = model.bank;
    switch (mode)
    {
        case Mode::Volume:
        {
            auto rt = model.reflTree (dragTick);
            const float factor = e.mods.isShiftDown() ? 0.1f : 1.0f;
            const float dv = (lastY - (float) e.y) / plotArea().getHeight() * factor;
            lastY = (float) e.y;
            bank.setVolume (rt, (float) rt.getProperty (ids::volume) + dv);
            break;
        }
        case Mode::Move:
        {
            const int c = model.nearestPosition ((float) e.x, w);
            if (c > 0 && c != dragTick && bank.moveReflection (model.slot(), model.reflTree (dragTick), c))
            {
                dragTick = c;
                model.selectedTick = c;
            }
            break;
        }
        case Mode::Paint:
        {
            const int c = model.nearestPosition ((float) e.x, w);
            const float v = yToVolume ((float) e.y);
            if (c <= 0)
                break;
            if (c == lastPaintTick)
                paintAt (c, v);
            else
            {
                // fill every empty position crossed since the last one
                const auto& pos = model.positions();
                const int lo = std::min (c, lastPaintTick), hi = std::max (c, lastPaintTick);
                for (int t : pos)
                    if (t > lo && t < hi && ! model.reflTree (t).isValid())
                        bank.addReflection (model.slot(), t, v);
                if (! model.reflTree (c).isValid())
                    bank.addReflection (model.slot(), c, v);
                lastPaintTick = c;
                dragTick = c;
                model.selectedTick = c;
            }
            break;
        }
        case Mode::None: break;
    }
    model.notify();
}

void VolumeGrid::mouseUp (const juce::MouseEvent&)
{
    mode = Mode::None;
    dragTick = -1;
    repaint();
}

void VolumeGrid::mouseMove (const juce::MouseEvent& e)
{
    const int w = getWidth();
    int newHover = -1;
    if (plotArea().contains (e.position) && model.hitReflection ((float) e.x, w) < 0)
        newHover = model.nearestPosition ((float) e.x, w);
    hoverVol = yToVolume ((float) e.y);
    if (newHover != hoverTick || newHover > 0)
    {
        hoverTick = newHover;
        repaint();
    }
    setMouseCursor (model.hitReflection ((float) e.x, w) > 0 ? juce::MouseCursor::UpDownResizeCursor
                                                              : juce::MouseCursor::CrosshairCursor);
}

void VolumeGrid::mouseExit (const juce::MouseEvent&)
{
    hoverTick = -1;
    repaint();
}

void VolumeGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = model.hitReflection ((float) e.x, getWidth());
    if (hit > 0)
    {
        model.bank.beginEdit ("Delete reflection");
        model.bank.removeReflection (model.slot(), model.reflTree (hit));
        mode = Mode::None;
    }
}

bool VolumeGrid::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && model.selectedTick > 0)
    {
        model.bank.beginEdit ("Delete reflection");
        model.bank.removeReflection (model.slot(), model.reflTree (model.selectedTick));
        return true;
    }
    return false;
}

// ============================================================================ PanGrid
float PanGrid::panToY (float pan) const
{
    const auto plot = plotArea();
    return plot.getCentreY() + pan * plot.getHeight() * 0.5f;
}

void PanGrid::paint (juce::Graphics& g)
{
    const auto plot = plotArea();
    const int w = getWidth();
    g.setColour (theme::plot);
    g.fillRect (plot);

    g.setFont (font (12.0f));
    for (auto [pan, label] : { std::pair<float, const char*> { -1.0f, "L100" }, { 0.0f, "C" }, { 1.0f, "R100" } })
    {
        const float y = panToY (pan);
        g.setColour (pan == 0.0f ? gridLine.brighter (0.3f) : gridLine);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (textFaint);
        g.drawText (label, juce::Rectangle<float> (0.0f, y - 8.0f, (float) GridModel::kGutterLeft - 10.0f, 16.0f),
                    juce::Justification::centredRight, false);
    }

    drawVerticalLines (g, plot, false, false);

    const float bw = model.columnWidth (w);
    const float cy = panToY (0.0f);
    for (const auto& r : model.visible())
    {
        const float x = model.tickToX (r.tick, w);
        const auto c = colourFor (r);
        const float y = panToY (r.pan);
        auto bar = juce::Rectangle<float> (x - bw * 0.5f, std::min (y, cy), bw, std::max (2.0f, std::abs (y - cy)));
        g.setColour (onGrid (r) ? c.withAlpha (0.85f) : c.withAlpha (0.15f));
        g.fillRoundedRectangle (bar, 2.0f);
        if (! onGrid (r))
        {
            g.setColour (c);
            g.drawRoundedRectangle (bar.reduced (0.5f), 2.0f, 1.2f);
        }
        const float hr = std::max (5.0f, std::min (8.0f, bw * 0.45f));
        g.setColour (theme::plot);
        g.fillEllipse (juce::Rectangle<float> (hr * 2.0f, hr * 2.0f).withCentre ({ x, y }));
        g.setColour (r.tick == model.selectedTick ? juce::Colours::white : c);
        g.drawEllipse (juce::Rectangle<float> (hr * 2.0f, hr * 2.0f).withCentre ({ x, y }), 2.0f);

        if (bw >= 11.0f || r.tick == model.selectedTick)
        {
            g.setColour (c);
            g.setFont (font (13.0f, true));
            float ly = r.pan <= 0.0f ? y - hr - 17.0f : y + hr + 2.0f;
            ly = juce::jlimit (0.0f, (float) getHeight() - 15.0f, ly);
            g.drawText (panLabel (r.pan), juce::Rectangle<float> (x - 30.0f, ly, 60.0f, 15.0f), juce::Justification::centred, false);
        }
    }
}

void PanGrid::mouseDown (const juce::MouseEvent& e)
{
    const int hit = model.hitReflection ((float) e.x, getWidth());
    if (hit <= 0)
        return;  // clicking an empty column does nothing
    dragTick = hit;
    lastY = (float) e.y;
    model.selectedTick = hit;
    model.bank.beginEdit ("Change pan");
    model.notify();
}

void PanGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTick <= 0)
        return;
    auto rt = model.reflTree (dragTick);
    const float factor = e.mods.isShiftDown() ? 0.1f : 1.0f;
    const float dp = ((float) e.y - lastY) / (plotArea().getHeight() * 0.5f) * factor;
    lastY = (float) e.y;
    float pan = (float) rt.getProperty (ids::pan) + dp;
    // gentle snap to the centre
    if (std::abs (pan) < 0.015f && factor == 1.0f)
        pan = 0.0f;
    model.bank.setPan (rt, pan);
}

void PanGrid::mouseUp (const juce::MouseEvent&) { dragTick = -1; }

void PanGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = model.hitReflection ((float) e.x, getWidth());
    if (hit > 0)
    {
        model.bank.beginEdit ("Centre pan");
        model.bank.setPan (model.reflTree (hit), 0.0f);
    }
}
} // namespace progdelay
