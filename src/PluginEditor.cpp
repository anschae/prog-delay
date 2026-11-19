#include "PluginEditor.h"

namespace progdelay
{
// ============================================================================ MainView
MainView::MainView (ProgDelayProcessor& p)
    : model (p), presets (p), headerBar (p, presets), tabs (model), toolbar (model), grids (model), randomize (model),
      feedback (p), soundMode (p), ducking (p), output (p), proc (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &headerBar, &tabs, &toolbar, &grids, &randomize, &feedback,
                                                             &soundMode, &ducking, &output, &fallback })
        addAndMakeVisible (c);

    fallback.addItemList ({ "Else: keep", "Else: 1", "Else: 2", "Else: 3", "Else: 4", "Else: 5", "Else: 6", "Else: 7", "Else: 8" }, 1);
    fallback.setTooltip ("Follow sig.: pattern to use when no pattern's assigned signature matches the host.");
    fallbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getApvts(), pid::fallback, fallback);

    model.onChange = [this] {
        tabs.refresh();
        toolbar.refresh();
        grids.repaint();
        randomize.repaint();
        for (auto* c : grids.getChildren())
            c->repaint();
    };
}

MainView::~MainView() { model.onChange = nullptr; }

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::panelEdge);
    g.drawHorizontalLine (tabs.getBottom(), 0.0f, (float) getWidth());
}

void MainView::resized()
{
    headerBar.setBounds (0, 0, kWidth, 58);
    const bool follow = proc.getApvts().getRawParameterValue (pid::switchSource)->load() > 0.5f
                     && proc.getApvts().getRawParameterValue (pid::switchSource)->load() < 1.5f;
    auto tabRow = juce::Rectangle<int> (20, 64, kWidth - 40, 34);
    if (follow)
        fallback.setBounds (tabRow.removeFromRight (112).reduced (0, 4));
    fallback.setVisible (follow);
    tabs.setBounds (tabRow.withTrimmedRight (follow ? 10 : 0));
    toolbar.setBounds (20, 106, kWidth - 40, 30);
    grids.setBounds (20, 146, 1012, 406);
    randomize.setBounds (1044, 146, kWidth - 20 - 1044, 406);
    const int y = 562, h = kHeight - 12 - y;
    feedback.setBounds (20, y, 300, h);
    soundMode.setBounds (332, y, 268, h);
    ducking.setBounds (612, y, 420, h);
    output.setBounds (1044, y, kWidth - 20 - 1044, h);
}

// ============================================================================ ProgDelayEditor
ProgDelayEditor::ProgDelayEditor (ProgDelayProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::kWidth * 3 / 4, MainView::kHeight * 3 / 4, MainView::kWidth * 3 / 2, MainView::kHeight * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainView::kWidth / MainView::kHeight);
    setSize (MainView::kWidth, MainView::kHeight);
    setWantsKeyboardFocus (true);
    startTimerHz (5);
}

ProgDelayEditor::~ProgDelayEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ProgDelayEditor::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void ProgDelayEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::kWidth;
    view.setBounds (0, 0, MainView::kWidth, MainView::kHeight);
    view.setTransform (juce::AffineTransform::scale (scale));
}

void ProgDelayEditor::timerCallback()
{
    const int src = juce::roundToInt (proc.getApvts().getRawParameterValue (pid::switchSource)->load());
    if (src != lastSwitchSource)
    {
        lastSwitchSource = src;
        view.resized();
    }
}

bool ProgDelayEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && k.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown())
            proc.getBank().redoEdit();
        else
            proc.getBank().undoEdit();
        return true;
    }
    if (mods.isCommandDown() && k.getKeyCode() == 'Y')
    {
        proc.getBank().redoEdit();
        return true;
    }
    return false;
}
} // namespace progdelay
