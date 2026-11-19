#include "GridPanel.h"
#include "PluginProcessor.h"

namespace progdelay
{
using namespace theme;

// ============================================================================ GridPanel
GridPanel::GridPanel (GridModel& m) : model (m), volume (m), pan (m)
{
    addAndMakeVisible (volume);
    addAndMakeVisible (pan);
    volume.setWantsKeyboardFocus (true);
    volume.setTooltip ("Click: add a reflection (height = volume). Drag: volume (Shift = fine). "
                       "Alt/Option-drag: move in time. Click-drag across empty steps: paint. "
                       "Double-click or right-click: delete.");
    pan.setTooltip ("Drag a reflection up (left) or down (right). Shift = fine. Double-click: centre.");
    startTimerHz (30);
}

void GridPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, "VOLUME", { 18, 10, 120, 18 });

    // colour legend
    auto l = juce::Rectangle<float> ((float) getWidth() - 200.0f, 9.0f, 0.0f, 18.0f);
    g.setFont (font (13.0f));
    for (auto [c, s] : { std::pair<juce::Colour, const char*> { straight, "straight" }, { triplet, "triplet" } })
    {
        g.setColour (c);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ l.getX() + 4.0f, l.getCentreY() }));
        g.setColour (textDim);
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s);
        g.drawText (s, juce::Rectangle<float> (l.getX() + 13.0f, l.getY(), tw + 4.0f, l.getHeight()), juce::Justification::centredLeft);
        l.setX (l.getX() + tw + 34.0f);
    }
    drawCaption (g, "PANNING", { 18, pan.getY() - 8, 120, 18 });
}

void GridPanel::resized()
{
    auto r = getLocalBounds().reduced (6, 8);
    r.removeFromTop (16);
    auto volArea = r.removeFromTop (juce::roundToInt (r.getHeight() * 0.66f));
    volume.setBounds (volArea);
    r.removeFromTop (14);
    pan.setBounds (r);
}

void GridPanel::timerCallback()
{
    auto& e = model.processor.getEngine();
    bool dirty = false;

    const int num = e.sigNumDisplay.load(), den = e.sigDenDisplay.load();
    if (num != model.hostSigNum || den != model.hostSigDen)
    {
        model.hostSigNum = num;
        model.hostSigDen = den;
        dirty = true;
    }
    const bool playing = e.hostPlaying.load();
    if (playing != model.playing || playing)
        dirty = true;
    model.playing = playing;
    model.ppqInBar = e.ppqInBar.load();
    model.bpm = e.bpmDisplay.load();

    // reflection flashes: only for the pattern that is playing
    const int playingSlot = e.playingPattern.load();
    const bool showFlash = playingSlot == model.slot();
    for (size_t i = 0; i < model.flash.size(); ++i)
    {
        const float lvl = e.tapLevels[i].exchange (0.0f);
        const float target = showFlash ? juce::jlimit (0.0f, 1.0f, lvl * 6.0f) : 0.0f;
        const float next = std::max (target, model.flash[i] * 0.82f);
        if (std::abs (next - model.flash[i]) > 0.004f)
            dirty = true;
        model.flash[i] = next < 0.01f ? 0.0f : next;
    }
    if (playingSlot != lastPlaying)
    {
        lastPlaying = playingSlot;
        dirty = true;
    }
    if (dirty)
    {
        volume.repaint();
        pan.repaint();
    }
}

// ============================================================================ RandomizePanel
RandomizePanel::RandomizePanel (GridModel& m)
    : model (m),
      amount ("Amount", accent, [this] { return model.bank.getRandAmount(); }, [this] (float v) { model.bank.setRandAmount (v); }),
      density ("Density", accent, [this] { return model.bank.getRandDensity(); }, [this] (float v) { model.bank.setRandDensity (v); })
{
    for (auto* b : { &all, &vol, &pan })
    {
        addAndMakeVisible (b);
        b->getProperties().set ("accent", accent.toString());
    }
    all.setToggleState (true, juce::dontSendNotification);  // drawn highlighted (primary action)
    all.setTooltip ("Create a new set of reflections on the current grid (Density = how many positions get one).");
    vol.setTooltip ("Change only the volume of the existing reflections.");
    pan.setTooltip ("Change only the pan of the existing reflections.");
    amount.setTooltipText ("How far values may move away from their current values (100 % = fully random).");
    density.setTooltipText ("How many grid positions get a reflection (Randomize All).");

    all.onClick = [this] { model.bank.randomizeAll (model.slot(), model.bank.getRandDensity(), model.bank.getRandAmount(), rng); };
    vol.onClick = [this] { model.bank.randomizeVolume (model.slot(), model.bank.getRandAmount(), rng); };
    pan.onClick = [this] { model.bank.randomizePan (model.slot(), model.bank.getRandAmount(), rng); };
    addAndMakeVisible (amount);
    addAndMakeVisible (density);
    startTimerHz (10);
}

void RandomizePanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, "RANDOMIZE", { 18, 10, 200, 18 });

    auto info = getLocalBounds().reduced (14).removeFromBottom (138);
    drawCaption (g, "SELECTED REFLECTION", info.removeFromTop (22));
    auto box = info.toFloat();
    g.setColour (juce::Colour (0xff141820));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);

    auto rows = info.reduced (14, 8);
    auto rt = model.reflTree (model.selectedTick);
    const bool has = rt.isValid();
    const int tick = model.selectedTick;
    const float v = has ? (float) rt.getProperty (ids::volume) : 0.0f;
    const float p = has ? (float) rt.getProperty (ids::pan) : 0.0f;
    const bool trip = has && classifyTick (tick) == ReflType::Triplet;

    auto row = [&] (const juce::String& k, const juce::String& val, juce::Colour c) {
        auto r = rows.removeFromTop (rows.getHeight() / 4 + 1);
        g.setFont (font (14.0f));
        g.setColour (textDim);
        g.drawText (k, r, juce::Justification::centredLeft);
        g.setColour (c);
        g.drawText (val, r, juce::Justification::centredRight);
    };
    if (! has)
    {
        g.setColour (textFaint);
        g.setFont (font (13.0f));
        g.drawFittedText ("Click a reflection in the grid to select it.", rows, juce::Justification::centred, 2);
        return;
    }
    const double ms = ticksToSeconds (tick, model.bpm) * 1000.0;
    row ("Position", positionLabel (tick) + juce::String (juce::CharPointer_UTF8 (trip ? "  \xc2\xb7  triplet" : "  \xc2\xb7  straight")), trip ? triplet : straight);
    row ("Time", juce::String (juce::roundToInt (ms)) + " ms", text);
    row ("Volume", juce::String (juce::roundToInt (v * 100.0f)) + " %  ("
                       + (v > 0.0f ? juce::String (juce::Decibels::gainToDecibels (v), 1) : juce::String ("-inf")) + " dB)", text);
    row ("Pan", panLabel (p), text);
}

void RandomizePanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (28);
    for (auto* b : { &all, &vol, &pan })
    {
        b->setBounds (r.removeFromTop (30));
        r.removeFromTop (6);
    }
    auto knobs = r.removeFromTop (92);
    amount.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (8, 0));
    density.setBounds (knobs.reduced (8, 0));
}

void RandomizePanel::timerCallback()
{
    // the selection info depends on tempo and edits; cheap to repaint
    amount.refresh();
    density.refresh();
    auto rt = model.reflTree (model.selectedTick);
    const juce::String info = juce::String (model.selectedTick) + (rt.isValid() ? rt.toXmlString() : juce::String()) + juce::String (model.bpm);
    if (info != lastInfo)
    {
        lastInfo = info;
        repaint();
    }
}
} // namespace progdelay
