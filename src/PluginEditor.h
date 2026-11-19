#pragma once
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/GridModel.h"
#include "ui/GridPanel.h"
#include "ui/PatternTabs.h"
#include "ui/Sections.h"
#include "ui/PresetBar.h"

namespace progdelay
{
/** Everything at the design size (1280 x 820); the editor scales it as a whole. */
class MainView : public juce::Component
{
public:
    static constexpr int kWidth = 1280, kHeight = 820;

    explicit MainView (ProgDelayProcessor& p);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    GridModel model;
    PresetBar presets;
    HeaderBar headerBar;
    PatternTabs tabs;
    PatternToolbar toolbar;
    GridPanel grids;
    RandomizePanel randomize;
    FeedbackSection feedback;
    SoundModeSection soundMode;
    DuckingSection ducking;
    OutputSection output;
    juce::ComboBox fallback;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fallbackAttachment;
    ProgDelayProcessor& proc;
};

class ProgDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ProgDelayEditor (ProgDelayProcessor&);
    ~ProgDelayEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    MainView& getView() noexcept { return view; }

private:
    void timerCallback() override;
    ProgDelayProcessor& proc;
    PDLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 700 };
    int lastSwitchSource = -1;
};
} // namespace progdelay
