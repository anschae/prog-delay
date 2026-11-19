#pragma once
// Pattern slot tabs (1-8) and the pattern toolbar (grid resolution, mode,
// snap, length, assigned time signature, tools).

#include "GridModel.h"
#include "Widgets.h"

namespace progdelay
{
class PatternTabs : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PatternTabs (GridModel& m);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh() { repaint(); }

private:
    void timerCallback() override;
    int tabAt (juce::Point<int>) const;
    void startRename (int slot);
    void showMenu (int slot);

    GridModel& model;
    std::vector<juce::Rectangle<int>> tabRects;
    int playing = -1, pending = -1;
    std::unique_ptr<juce::TextEditor> renameEditor;
    int renameSlot = -1;
};

class PatternToolbar : public juce::Component
{
public:
    explicit PatternToolbar (GridModel& m);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void showSigMenu();
    void showDuplicateMenu();

    GridModel& model;
    Segmented resolution { juce::StringArray { "1/4", "1/8", "1/16", "1/32" } };
    Segmented mode { juce::StringArray { "Straight", "Triplet", "Combined" } };
    Segmented snap { juce::StringArray { "Both", "Str.", "Trip." } };
    juce::ComboBox length;
    juce::TextButton sig;
    juce::TextButton copy { "Copy" }, paste { "Paste" }, dup { "Dup" }, mirror { juce::CharPointer_UTF8 ("L\xe2\x86\x94R") },
        left { juce::CharPointer_UTF8 ("\xe2\x97\x82") }, right { juce::CharPointer_UTF8 ("\xe2\x96\xb8") }, clear { "Clear" };
    juce::Rectangle<int> gridLabel, modeLabel, snapLabel, lengthLabel, sigLabel;
};
} // namespace progdelay
