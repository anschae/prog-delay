#include "PatternTabs.h"
#include "PluginProcessor.h"

namespace progdelay
{
using namespace theme;

// ============================================================================ PatternTabs
PatternTabs::PatternTabs (GridModel& m) : model (m)
{
    setTooltip ("Click: edit this pattern. Double-click: rename. Right-click: colour, copy, paste, clear. "
                "The green LED marks the pattern that is playing.");
    startTimerHz (15);
}

void PatternTabs::timerCallback()
{
    auto& e = model.processor.getEngine();
    const int p = e.playingPattern.load(), q = e.pendingPattern.load();
    if (p != playing || q != pending)
    {
        playing = p;
        pending = q;
        repaint();
    }
}

void PatternTabs::resized()
{
    tabRects.clear();
    auto r = getLocalBounds();
    int x = 0;
    for (int i = 0; i < kNumPatterns; ++i)
    {
        const auto p = model.bank.getPattern (i);
        const bool empty = p.reflections.empty() && p.name.startsWith ("Pattern ");
        const juce::String label = empty ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) : p.name;
        const bool led = i == playing || i == pending;
        int w = 14 + 12 + 8 + 9 + 8 + juce::GlyphArrangement::getStringWidthInt (font (15.0f), label) + (led ? 22 : 0) + 16;
        w = juce::jlimit (64, 190, w);
        tabRects.push_back ({ x, r.getY(), w, r.getHeight() });
        x += w + 6;
    }
}

void PatternTabs::paint (juce::Graphics& g)
{
    resized();  // widths depend on names
    const int edited = model.slot();
    for (int i = 0; i < kNumPatterns; ++i)
    {
        const auto p = model.bank.getPattern (i);
        const auto r = tabRects[(size_t) i].toFloat();
        const bool sel = i == edited;
        const bool empty = p.reflections.empty() && p.name.startsWith ("Pattern ");

        juce::Path tab;
        tab.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() + 2.0f, 7.0f, 7.0f, true, true, false, false);
        g.setColour (sel ? panel : juce::Colour (0xff151920));
        g.fillPath (tab);
        g.setColour (sel ? panelEdge.brighter (0.15f) : panelEdge);
        g.strokePath (tab, juce::PathStrokeType (1.0f));
        if (sel)
        {
            g.setColour (p.colour);
            g.fillRect (r.getX() + 7.0f, r.getY(), r.getWidth() - 14.0f, 2.0f);
        }

        auto content = r.reduced (14.0f, 0.0f);
        g.setFont (font (15.0f));
        g.setColour (textDim);
        g.drawText (juce::String (i + 1), content.removeFromLeft (12.0f), juce::Justification::centredLeft);
        content.removeFromLeft (8.0f);
        g.setColour (empty ? textFaint : p.colour);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ content.getX() + 4.5f, content.getCentreY() }));
        content.removeFromLeft (17.0f);

        const bool isPlaying = i == playing;
        const bool isPending = i == pending;
        auto ledArea = (isPlaying || isPending) ? content.removeFromRight (18.0f) : juce::Rectangle<float>();
        g.setColour (sel ? text : (empty ? textFaint : textDim.brighter (0.2f)));
        g.drawText (empty ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) : p.name, content,
                    juce::Justification::centredLeft, true);
        if (isPlaying)
        {
            g.setColour (hostBar.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (ledArea.getCentre()));
            g.setColour (hostBar);
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (ledArea.getCentre()));
        }
        else if (isPending)
        {
            g.setColour (warn);
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (ledArea.getCentre()), 1.5f);
        }
    }

    // signature map: "7/8 → 1 · 10/8 → 2"
    juce::StringArray parts;
    for (int i = 0; i < kNumPatterns; ++i)
    {
        const auto p = model.bank.getPattern (i);
        if (p.sig.isValid())
            parts.add (p.sig.toString() + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 ")) + juce::String (i + 1));
    }
    if (! parts.isEmpty() && ! tabRects.empty())
    {
        auto area = getLocalBounds().withLeft (tabRects.back().getRight() + 16);
        g.setColour (textDim);
        g.setFont (font (13.0f));
        g.drawFittedText (parts.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))), area,
                          juce::Justification::centredRight, 1, 0.8f);
    }
}

int PatternTabs::tabAt (juce::Point<int> p) const
{
    for (size_t i = 0; i < tabRects.size(); ++i)
        if (tabRects[i].contains (p))
            return (int) i;
    return -1;
}

void PatternTabs::mouseDown (const juce::MouseEvent& e)
{
    const int t = tabAt (e.getPosition());
    if (t < 0)
        return;
    if (e.mods.isPopupMenu())
    {
        showMenu (t);
        return;
    }
    model.bank.setEditedSlot (t);
    model.selectedTick = -1;
}

void PatternTabs::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int t = tabAt (e.getPosition());
    if (t >= 0)
        startRename (t);
}

void PatternTabs::startRename (int slot)
{
    renameSlot = slot;
    renameEditor = std::make_unique<juce::TextEditor>();
    renameEditor->setFont (font (15.0f));
    renameEditor->setText (model.bank.getPattern (slot).name, false);
    renameEditor->selectAll();
    addAndMakeVisible (*renameEditor);
    renameEditor->setBounds (tabRects[(size_t) slot].reduced (34, 6).withTrimmedRight (-20));
    renameEditor->grabKeyboardFocus();
    auto finish = [this] (bool commit) {
        if (renameEditor == nullptr)
            return;
        if (commit && renameEditor->getText().trim().isNotEmpty())
            model.bank.setName (renameSlot, renameEditor->getText().trim());
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PatternTabs> (this)] {
            if (sp != nullptr) { sp->renameEditor.reset(); sp->repaint(); }
        });
    };
    renameEditor->onReturnKey = [finish] { finish (true); };
    renameEditor->onFocusLost = [finish] { finish (true); };
    renameEditor->onEscapeKey = [finish] { finish (false); };
}

void PatternTabs::showMenu (int slot)
{
    juce::PopupMenu m, colours;
    static const juce::uint32 palette[] = { 0xff2ec4d6, 0xffff8a3d, 0xffa58bff, 0xff4cd68a, 0xfff2c94c,
                                            0xffff5f7e, 0xff5fa8ff, 0xffc0c6d0, 0xffe07bd8, 0xff8fd14f };
    const char* names[] = { "Cyan", "Orange", "Violet", "Green", "Yellow", "Red", "Blue", "Grey", "Pink", "Lime" };
    for (int i = 0; i < 10; ++i)
        colours.addColouredItem (100 + i, names[i], juce::Colour (palette[i]), true,
                                 model.bank.getPattern (slot).colour == juce::Colour (palette[i]));
    m.addSectionHeader ("Pattern " + juce::String (slot + 1));
    m.addItem (1, "Rename...");
    m.addSubMenu ("Colour", colours);
    m.addSeparator();
    m.addItem (2, "Copy");
    m.addItem (3, "Paste", model.bank.canPaste());
    m.addItem (4, "Clear");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [this, slot] (int r) {
                         if (r == 1) startRename (slot);
                         else if (r == 2) model.bank.copyPattern (slot);
                         else if (r == 3) model.bank.pastePattern (slot);
                         else if (r == 4) model.bank.clearPattern (slot);
                         else if (r >= 100)
                         {
                             static const juce::uint32 pal[] = { 0xff2ec4d6, 0xffff8a3d, 0xffa58bff, 0xff4cd68a, 0xfff2c94c,
                                                                 0xffff5f7e, 0xff5fa8ff, 0xffc0c6d0, 0xffe07bd8, 0xff8fd14f };
                             model.bank.setColour (slot, juce::Colour (pal[r - 100]));
                         }
                     });
}

// ============================================================================ PatternToolbar
PatternToolbar::PatternToolbar (GridModel& m) : model (m)
{
    addAndMakeVisible (resolution);
    addAndMakeVisible (mode);
    addAndMakeVisible (snap);
    addAndMakeVisible (length);
    addAndMakeVisible (sig);
    for (auto* b : { &copy, &paste, &dup, &mirror, &left, &right, &clear })
        addAndMakeVisible (b);
    mode.setItemAccent (2, triplet.interpolatedWith (straight, 0.5f));

    resolution.onChange = [this] (int i) { model.bank.setResolution (model.slot(), kResolutions[i]); };
    mode.onChange = [this] (int i) { model.bank.setGridMode (model.slot(), (GridMode) i); };
    snap.onChange = [this] (int i) { model.bank.setSnapMode ((SnapMode) i); };

    for (int i = 1; i <= 4; ++i)
        length.addItem (juce::String (i) + (i == 1 ? " bar" : " bars"), i);
    length.onChange = [this] {
        if (length.getSelectedId() > 0 && length.getSelectedId() != model.pattern().lengthWholes)
            model.bank.setLength (model.slot(), length.getSelectedId());
    };
    sig.onClick = [this] { showSigMenu(); };

    copy.onClick = [this] { model.bank.copyPattern (model.slot()); refresh(); };
    paste.onClick = [this] { model.bank.pastePattern (model.slot()); };
    dup.onClick = [this] { showDuplicateMenu(); };
    mirror.onClick = [this] { model.bank.mirrorPan (model.slot()); };
    left.onClick = [this] { model.bank.shiftPattern (model.slot(), -1); };
    right.onClick = [this] { model.bank.shiftPattern (model.slot(), +1); };
    clear.onClick = [this] { model.bank.clearPattern (model.slot()); };

    resolution.setTooltip ("Grid resolution of this pattern. Existing reflections keep their time.");
    mode.setTooltip ("Straight, triplet, or both (straight = cyan, triplet = orange).");
    snap.setTooltip ("In Combined mode: which positions a click snaps to.");
    length.setTooltip ("Grid length in whole notes (one 4/4 bar each).");
    sig.setTooltip ("Time signature assigned to this pattern: shown as a solid bar marker and used by 'Follow sig.'.");
    copy.setTooltip ("Copy this pattern");
    paste.setTooltip ("Paste the copied pattern here (keeps this slot's name and colour)");
    dup.setTooltip ("Duplicate this pattern to another slot");
    mirror.setTooltip ("Mirror the pan of all reflections (L <-> R)");
    left.setTooltip ("Shift all reflections one step earlier (wraps around)");
    right.setTooltip ("Shift all reflections one step later (wraps around)");
    clear.setTooltip ("Remove all reflections of this pattern");

    copy.setConnectedEdges (juce::Button::ConnectedOnRight);
    for (auto* b : { &paste, &dup, &mirror, &left, &right })
        b->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    clear.setConnectedEdges (juce::Button::ConnectedOnLeft);
    refresh();
}

void PatternToolbar::refresh()
{
    const auto& p = model.pattern();
    int ri = 0;
    for (int i = 0; i < 4; ++i)
        if (kResolutions[i] == p.resolution)
            ri = i;
    resolution.setSelected (ri);
    mode.setSelected ((int) p.mode);
    snap.setSelected ((int) model.bank.getSnapMode());
    snap.setVisible (p.mode == GridMode::Combined);
    length.setSelectedId (p.lengthWholes, juce::dontSendNotification);
    sig.setButtonText (p.sig.isValid() ? p.sig.toString() : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")));
    paste.setEnabled (model.bank.canPaste());
    resized();
    repaint();
}

void PatternToolbar::showSigMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "None", true, ! model.pattern().sig.isValid());
    const int dens[] = { 2, 4, 8, 16 };
    const int maxNum[] = { 8, 12, 17, 32 };
    for (int d = 0; d < 4; ++d)
    {
        juce::PopupMenu sub;
        for (int n = 1; n <= maxNum[d]; ++n)
        {
            const TimeSig s { n, dens[d] };
            sub.addItem (1000 + dens[d] * 100 + n, s.toString(), true, model.pattern().sig == s);
        }
        m.addSubMenu ("/" + juce::String (dens[d]), sub);
    }
    m.addSeparator();
    m.addItem (2, "Use host signature (" + juce::String (model.hostSigNum) + "/" + juce::String (model.hostSigDen) + ")");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sig), [this] (int r) {
        if (r == 1)
            model.bank.setTimeSig (model.slot(), {});
        else if (r == 2)
            model.bank.setTimeSig (model.slot(), { model.hostSigNum, model.hostSigDen });
        else if (r >= 1000)
        {
            const int den = (r - 1000) / 100, num = (r - 1000) % 100;
            model.bank.setTimeSig (model.slot(), { num, den });
        }
    });
}

void PatternToolbar::showDuplicateMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Duplicate to slot");
    for (int i = 0; i < kNumPatterns; ++i)
        if (i != model.slot())
            m.addItem (i + 1, juce::String (i + 1) + "  " + model.bank.getPattern (i).name);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&dup), [this] (int r) {
        if (r > 0)
        {
            model.bank.duplicatePattern (model.slot(), r - 1);
            model.bank.setEditedSlot (r - 1);
        }
    });
}

void PatternToolbar::paint (juce::Graphics& g)
{
    drawCaption (g, "GRID", gridLabel, textDim, 10.5f);
    drawCaption (g, "MODE", modeLabel, textDim, 10.5f);
    if (snap.isVisible())
        drawCaption (g, "SNAP", snapLabel, textDim, 10.5f);
    drawCaption (g, "LENGTH", lengthLabel, textDim, 10.5f);
    drawCaption (g, "PATTERN SIG.", sigLabel, textDim, 10.5f);

}

void PatternToolbar::resized()
{
    auto r = getLocalBounds();
    gridLabel = r.removeFromLeft (38);
    resolution.setBounds (r.removeFromLeft (146));
    r.removeFromLeft (14);
    modeLabel = r.removeFromLeft (44);
    mode.setBounds (r.removeFromLeft (204));
    r.removeFromLeft (14);
    if (snap.isVisible())
    {
        snapLabel = r.removeFromLeft (42);
        snap.setBounds (r.removeFromLeft (126));
        r.removeFromLeft (14);
    }
    lengthLabel = r.removeFromLeft (58);
    length.setBounds (r.removeFromLeft (80));
    r.removeFromLeft (14);
    sigLabel = r.removeFromLeft (90);
    sig.setBounds (r.removeFromLeft (54));

    auto tools = r.removeFromRight (282);
    const int widths[] = { 46, 50, 40, 44, 28, 28, 46 };
    int i = 0;
    for (auto* b : { &copy, &paste, &dup, &mirror, &left, &right, &clear })
        b->setBounds (tools.removeFromLeft (widths[i++]));
}
} // namespace progdelay
