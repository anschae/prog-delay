#include "Sections.h"
#include "PluginProcessor.h"
#include "PresetBar.h"
#include "DuckingDisplay.h"
#include "dsp/modes/SoundModeRegistry.h"

namespace progdelay
{
using namespace theme;

namespace
{
void captionBox (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, float h = 11.0f)
{
    drawCaption (g, t, r, textDim, h);
}
} // namespace

// ============================================================================ BpmField
BpmField::BpmField (ProgDelayProcessor& p) : proc (p)
{
    bpmParam = p.getApvts().getParameter (pid::manualBpm);
    modeParam = p.getApvts().getParameter (pid::tempoMode);
    setTooltip ("Tempo. In Sync mode the host tempo is shown; in Manual mode drag up/down or double-click to type.");
    startTimerHz (10);
}

bool BpmField::isManual() const { return modeParam->getValue() > 0.5f; }

void BpmField::timerCallback()
{
    const float v = isManual() ? bpmParam->convertFrom0to1 (bpmParam->getValue()) : proc.getEngine().bpmDisplay.load();
    if (std::abs (v - shown) > 0.004f)
    {
        shown = v;
        repaint();
    }
}

void BpmField::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (control);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (isManual() ? straight.withAlpha (0.6f) : panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (text);
    g.setFont (font (14.0f));
    g.drawText (juce::String (shown, 2) + " BPM", getLocalBounds(), juce::Justification::centred);
}

void BpmField::mouseDown (const juce::MouseEvent&)
{
    if (! isManual())
        return;
    dragStartBpm = bpmParam->convertFrom0to1 (bpmParam->getValue());
    bpmParam->beginChangeGesture();
}

void BpmField::mouseDrag (const juce::MouseEvent& e)
{
    if (! isManual())
        return;
    const float factor = e.mods.isShiftDown() ? 0.02f : 0.25f;
    const float v = juce::jlimit (30.0f, 300.0f, dragStartBpm - (float) e.getDistanceFromDragStartY() * factor);
    bpmParam->setValueNotifyingHost (bpmParam->convertTo0to1 (std::round (v * 100.0f) / 100.0f));
}

void BpmField::mouseUp (const juce::MouseEvent&)
{
    if (isManual())
        bpmParam->endChangeGesture();
}

void BpmField::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isManual())
        return;
    editor = std::make_unique<juce::TextEditor>();
    editor->setJustification (juce::Justification::centred);
    editor->setFont (font (14.0f));
    editor->setText (juce::String (shown, 2), false);
    editor->selectAll();
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds());
    editor->grabKeyboardFocus();
    auto done = [this] (bool commit) {
        if (editor == nullptr)
            return;
        if (commit)
        {
            const float v = juce::jlimit (30.0f, 300.0f, editor->getText().getFloatValue());
            if (v >= 30.0f)
            {
                bpmParam->beginChangeGesture();
                bpmParam->setValueNotifyingHost (bpmParam->convertTo0to1 (v));
                bpmParam->endChangeGesture();
            }
        }
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<BpmField> (this)] {
            if (sp != nullptr) sp->editor.reset();
        });
    };
    editor->onReturnKey = [done] { done (true); };
    editor->onFocusLost = [done] { done (true); };
    editor->onEscapeKey = [done] { done (false); };
}

// ============================================================================ HeaderBar
HeaderBar::HeaderBar (ProgDelayProcessor& p, PresetBar& pb)
    : proc (p), presets (pb),
      tempoMode (p.getApvts(), pid::tempoMode),
      switchSource (p.getApvts(), pid::switchSource),
      bpm (p)
{
    addAndMakeVisible (presets);
    addAndMakeVisible (undo);
    addAndMakeVisible (redo);
    addAndMakeVisible (tempoMode);
    addAndMakeVisible (bpm);
    addAndMakeVisible (hostSig);
    addAndMakeVisible (switchSource);
    addAndMakeVisible (quant);

    undo.setConnectedEdges (juce::Button::ConnectedOnRight);
    redo.setConnectedEdges (juce::Button::ConnectedOnLeft);
    undo.onClick = [this] { proc.getBank().undoEdit(); };
    redo.onClick = [this] { proc.getBank().redoEdit(); };
    undo.setTooltip ("Undo pattern edit (Cmd/Ctrl+Z)");
    redo.setTooltip ("Redo pattern edit (Cmd/Ctrl+Shift+Z)");

    tempoMode.setTooltip ("Sync: follow the host tempo. Manual: use your own BPM.");
    switchSource.setTooltip ("How the playing pattern is chosen: by the 'Active Pattern' automation parameter, "
                             "by the host time signature (patterns with a matching assigned signature), or by MIDI.");
#if ! PD_MIDI_SWITCHING
    switchSource.setItemEnabled (2, false);
    switchSource.setItemTooltip (2, "MIDI switching needs a build with PD_MIDI_SWITCHING=ON (see README).");
#endif

    hostSig.setJustificationType (juce::Justification::centred);
    hostSig.setFont (font (14.0f));
    hostSig.setColour (juce::Label::textColourId, straight);
    hostSig.setColour (juce::Label::backgroundColourId, control);
    hostSig.setColour (juce::Label::outlineColourId, panelEdge);
    hostSig.setTooltip ("Time signature reported by the host");

    quant.addItemList ({ "Q: Immediate", "Q: Next beat", "Q: Next bar" }, 1);
    quant.setTooltip ("Switch quantize: when a pattern change takes effect.");
    quantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getApvts(), pid::switchQuant, quant);
    startTimerHz (8);
}

void HeaderBar::timerCallback()
{
    auto& e = proc.getEngine();
    const juce::String s = "Host " + juce::String (e.sigNumDisplay.load()) + "/" + juce::String (e.sigDenDisplay.load());
    if (hostSig.getText() != s)
        hostSig.setText (s, juce::dontSendNotification);
    undo.setEnabled (proc.getBank().canUndo());
    redo.setEnabled (proc.getBank().canRedo());
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.setColour (header);
    g.fillRect (getLocalBounds());
    g.setColour (panelEdge);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    auto l = logoArea;
    auto titleRow = l.removeFromTop (30);
    auto f = font (24.0f, true).withExtraKerningFactor (0.2f);
    g.setFont (f);
    g.setColour (text);
    const int w1 = juce::GlyphArrangement::getStringWidthInt (f, "PROG ");
    g.drawText ("PROG ", titleRow, juce::Justification::bottomLeft, false);
    g.setColour (straight);
    g.drawText ("DELAY", titleRow.withTrimmedLeft (w1), juce::Justification::bottomLeft, false);
    g.setColour (textDim);
    g.setFont (font (10.0f).withExtraKerningFactor (0.25f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("ANSCHAE \xc2\xb7 MULTI-PATTERN DELAY")), l.removeFromTop (16),
                juce::Justification::topLeft, false);

    captionBox (g, tempoCaption, "TEMPO", 10.0f);
    captionBox (g, switchCaption, "PATTERN SWITCH", 10.0f);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (20, 0);
    logoArea = r.removeFromLeft (232).withTrimmedTop (6);
    auto row = [] (juce::Rectangle<int> area) { return area.withSizeKeepingCentre (area.getWidth(), 28).translated (0, 6); };

    presets.setBounds (row (r.removeFromLeft (300)));
    r.removeFromLeft (14);
    auto ur = row (r.removeFromLeft (64));
    undo.setBounds (ur.removeFromLeft (32));
    redo.setBounds (ur);

    auto sw = r.removeFromRight (318);
    switchCaption = sw.withHeight (20).translated (0, 3);
    auto swRow = row (sw);
    quant.setBounds (swRow.removeFromRight (106));
    swRow.removeFromRight (8);
    switchSource.setBounds (swRow);
    r.removeFromRight (16);

    auto tp = r.removeFromRight (276);
    tempoCaption = tp.withHeight (20).translated (0, 3);
    auto tpRow = row (tp);
    tempoMode.setBounds (tpRow.removeFromLeft (110));
    tpRow.removeFromLeft (6);
    hostSig.setBounds (tpRow.removeFromRight (70));
    tpRow.removeFromRight (6);
    bpm.setBounds (tpRow);
}

// ============================================================================ FeedbackSection
FeedbackSection::FeedbackSection (ProgDelayProcessor& p)
    : proc (p),
      barMode (p.getApvts(), pid::fbBarMode),
      stereo (p.getApvts(), pid::stereoMode),
      feedback (p.getApvts(), pid::feedback, "Feedback", straight),
      lowCut (p.getApvts(), pid::lowCut, "Low Cut", textDim),
      highCut (p.getApvts(), pid::highCut, "High Cut", textDim),
      s1 (p.getApvts(), pid::s1Level, "S1 Level", textDim),
      s2 (p.getApvts(), pid::s2Level, "S2 Level", textDim)
{
    for (auto* c : std::initializer_list<juce::Component*> { &barMode, &stereo, &num, &den, &hostBarLabel, &feedback, &lowCut,
                                                             &highCut, &s1, &s2 })
        addAndMakeVisible (c);
    for (int i = 1; i <= 32; ++i)
        num.addItem (juce::String (i), i);
    den.addItemList ({ "2", "4", "8", "16" }, 1);
    numAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getApvts(), pid::fbBarNum, num);
    denAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getApvts(), pid::fbBarDen, den);
    hostBarLabel.setJustificationType (juce::Justification::centred);
    hostBarLabel.setColour (juce::Label::textColourId, textDim);
    hostBarLabel.setColour (juce::Label::backgroundColourId, control);
    hostBarLabel.setColour (juce::Label::outlineColourId, panelEdge);

    barMode.setTooltip ("Length of one feedback repeat: the host bar (follows time-signature changes) or a manual signature.");
    stereo.setTooltip ("Keep: pan as defined. Ping-Pong: L/R swap every repeat. Mono: repeats summed to mono.");
    num.setTooltip ("Manual feedback bar: numerator");
    den.setTooltip ("Manual feedback bar: denominator");
    feedback.setTooltipText ("How much of the whole pattern repeats every feedback bar.");
    lowCut.setTooltipText ("Low cut inside the feedback loop (12 dB/oct): each repeat gets thinner.");
    highCut.setTooltipText ("High cut inside the feedback loop (12 dB/oct): each repeat gets darker.");
    s1.setTooltipText ("Level of stage 1 (the pattern's reflections) in the wet signal.");
    s2.setTooltipText ("Level of stage 2 (the feedback repeats) in the wet signal.");
    startTimerHz (8);
}

void FeedbackSection::timerCallback()
{
    const bool manual = proc.getApvts().getRawParameterValue (pid::fbBarMode)->load() > 0.5f;
    num.setVisible (manual);
    den.setVisible (manual);
    hostBarLabel.setVisible (! manual);
    auto& e = proc.getEngine();
    hostBarLabel.setText (juce::String (e.sigNumDisplay.load()) + " / " + juce::String (e.sigDenDisplay.load()), juce::dontSendNotification);
    const float fb = proc.getApvts().getRawParameterValue (pid::feedback)->load();
    if (fb != lastFb)
    {
        lastFb = fb;
        repaint (repeatsArea);
    }
}

void FeedbackSection::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, juce::String (juce::CharPointer_UTF8 ("STAGE 2 \xc2\xb7 FEEDBACK")), { 18, 10, 260, 18 });
    captionBox (g, barCaption, "BAR", 10.5f);
    captionBox (g, stereoCaption, "STEREO", 10.5f);

    // repeats preview: level of each repeat
    auto r = repeatsArea;
    g.setColour (textFaint);
    g.setFont (font (11.0f));
    g.drawText ("repeats", r.removeFromLeft (56), juce::Justification::centredLeft);
    const float fb = proc.getApvts().getRawParameterValue (pid::feedback)->load() * 0.01f;
    float lvl = 1.0f;
    const float slot = (float) r.getWidth() / 8.0f;
    for (int k = 0; k < 8; ++k)
    {
        lvl *= fb;
        const float h = juce::jlimit (1.0f, (float) r.getHeight(), lvl * (float) r.getHeight());
        g.setColour (straight.withAlpha (0.25f + 0.6f * lvl));
        for (int j = 0; j < 3; ++j)
        {
            const float hh = h * (1.0f - 0.25f * (float) j);
            g.fillRect (r.getX() + k * slot + j * 6.0f, (float) r.getBottom() - hh, 4.0f, hh);
        }
    }
}

void FeedbackSection::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    r.removeFromTop (24);
    auto row1 = r.removeFromTop (30);
    barCaption = row1.removeFromLeft (44);
    barMode.setBounds (row1.removeFromLeft (124).reduced (0, 1));
    row1.removeFromLeft (8);
    auto bar = row1.removeFromLeft (120).reduced (0, 1);
    hostBarLabel.setBounds (bar.withWidth (66));
    num.setBounds (bar.removeFromLeft (56));
    bar.removeFromLeft (4);
    den.setBounds (bar.removeFromLeft (56));

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (30);
    stereoCaption = row2.removeFromLeft (60);
    stereo.setBounds (row2.removeFromLeft (220).reduced (0, 1));

    repeatsArea = r.removeFromBottom (26);
    r.removeFromBottom (4);
    auto knobs = r;
    const int kw = knobs.getWidth() / 5;
    for (auto* k : { &feedback, &lowCut, &highCut, &s1, &s2 })
        k->setBounds (knobs.removeFromLeft (kw).reduced (2, 2));
}

// ============================================================================ SoundModeSection
SoundModeSection::SoundModeSection (ProgDelayProcessor& p)
    : character (p.getApvts(), pid::character, "Character", triplet),
      age (p.getApvts(), pid::age, "Age / Mod", triplet),
      tone (p.getApvts(), pid::tone, "Tone", triplet),
      colorRefl (p.getApvts(), pid::colorRefl, "Color reflections (Stage 1)")
{
    const auto& reg = SoundModeRegistry::get();
    auto* param = p.getApvts().getParameter (pid::soundMode);
    for (int i = 0; i < reg.size(); ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (reg[i].displayName));
        b->getProperties().set ("accent", triplet.toString());
        b->onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) {
        for (int i = 0; i < modeButtons.size(); ++i)
            modeButtons[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
    });
    modeAttachment->sendInitialUpdate();

    addAndMakeVisible (character);
    addAndMakeVisible (age);
    addAndMakeVisible (tone);
    addAndMakeVisible (colorRefl);
    colorRefl.getProperties().set ("accent", straight.toString());
    character.setTooltipText ("Amount of colouration (saturation, grit) of the selected mode.");
    age.setTooltipText ("Modulation and wear: wow/flutter, chorus, noise.");
    tone.setTooltipText ("Darker or brighter repeats.");
    colorRefl.setTooltip ("Also apply the sound mode to stage 1 (the pattern's reflections). The feedback loop is always coloured.");
}

void SoundModeSection::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, "SOUND MODE", { 18, 10, 200, 18 });
}

void SoundModeSection::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    r.removeFromTop (24);
    const int rows = (modeButtons.size() + 1) / 2;
    auto grid = r.removeFromTop (rows * 32 + (rows - 1) * 6);
    for (int i = 0; i < modeButtons.size(); ++i)
    {
        const int row = i / 2, col = i % 2;
        const int bw = (grid.getWidth() - 8) / 2;
        modeButtons[i]->setBounds (grid.getX() + col * (bw + 8), grid.getY() + row * 38, bw, 32);
    }
    colorRefl.setBounds (r.removeFromBottom (26));
    r.removeFromTop (6);
    const int kw = r.getWidth() / 3;
    for (auto* k : { &character, &age, &tone })
        k->setBounds (r.removeFromLeft (kw).reduced (8, 0));
}

// ============================================================================ DuckingSection
DuckingSection::DuckingSection (ProgDelayProcessor& p)
    : proc (p),
      on (p.getApvts(), pid::duckOn, ""),
      source (p.getApvts(), pid::duckSource),
      target (p.getApvts(), pid::duckTarget, { "S1", "S2", "Both" }),
      position (p.getApvts(), pid::duckPos),
      thresh (p.getApvts(), pid::duckThresh, "Thresh", duck),
      depth (p.getApvts(), pid::duckDepth, "Depth", duck),
      attack (p.getApvts(), pid::duckAttack, "Attack", duck),
      hold (p.getApvts(), pid::duckHold, "Hold", duck),
      release (p.getApvts(), pid::duckRelease, "Release", duck),
      detHp (p.getApvts(), pid::duckHP, "Det. HP", duck),
      look (p.getApvts(), pid::duckLook, "Look", duck)
{
    display = std::make_unique<DuckingDisplay> (p);
    for (auto* c : std::initializer_list<juce::Component*> { &on, &source, &target, &position, display.get(), &thresh, &depth, &attack,
                                                             &hold, &release, &detHp, &look, &more })
        addAndMakeVisible (c);
    on.getProperties().set ("accent", duck.toString());
    on.setTooltip ("Ducking on/off: lowers the delay while you play, so it blooms in the gaps.");
    source.setTooltip ("Detector: the plugin input or the external sidechain input.");
    target.setTooltip ("What gets ducked: stage 1, stage 2 or both.");
    position.setTooltip ("Post: duck the delay output. Pre: duck what goes into the delay (loud passages never fill the loop).");
    thresh.setTooltipText ("Detector level above which ducking starts.");
    depth.setTooltipText ("Maximum gain reduction (see More for full duck).");
    attack.setTooltipText ("How fast the ducking engages.");
    hold.setTooltipText ("How long the ducking holds after the level drops.");
    release.setTooltipText ("How fast the delay comes back (can be tempo-synced, see More).");
    detHp.setTooltipText ("High-pass on the detector only, so palm-muted low end doesn't dominate the ducking.");
    look.setTooltipText ("Lookahead: delays the signal so ducking can start before the transient (adds latency).");
    more.setTooltip ("More detector settings: low-pass, knee, peak/RMS, full duck, tempo-synced release");
    more.onClick = [this] { showMore(); };
    release.setValueFormatter ([this] (float) {
        auto& apvts = proc.getApvts();
        if (apvts.getRawParameterValue (pid::duckRelSync)->load() > 0.5f)
            return apvts.getParameter (pid::duckRelNote)->getCurrentValueAsText() + juce::String (juce::CharPointer_UTF8 (" \xe2\x99\xa9"));
        return apvts.getParameter (pid::duckRelease)->getCurrentValueAsText();
    });
    depth.setValueFormatter ([this] (float) {
        auto& apvts = proc.getApvts();
        if (apvts.getRawParameterValue (pid::duckFull)->load() > 0.5f)
            return juce::String ("-inf dB");
        return apvts.getParameter (pid::duckDepth)->getCurrentValueAsText();
    });
}

DuckingSection::~DuckingSection() = default;

void DuckingSection::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, "DUCKING", { 18, 13, 80, 18 });
}

void DuckingSection::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (28).withTrimmedTop (2);
    top.removeFromLeft (74);
    on.setBounds (top.removeFromLeft (38));
    source.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (6);
    target.setBounds (top.removeFromLeft (88));
    top.removeFromLeft (6);
    position.setBounds (top.removeFromLeft (76));

    r.removeFromTop (8);
    display->setBounds (r.removeFromTop (r.getHeight() - 88));
    more.setBounds (display->getBounds().removeFromTop (22).removeFromRight (64).translated (-4, 4));
    more.toFront (false);
    r.removeFromTop (6);
    const int kw = r.getWidth() / 7;
    for (auto* k : { &thresh, &depth, &attack, &hold, &release, &detHp, &look })
        k->setBounds (r.removeFromLeft (kw).reduced (1, 0));
}

namespace
{
/** Pop-up with the less common ducking settings. */
class DuckMore : public juce::Component
{
public:
    explicit DuckMore (juce::AudioProcessorValueTreeState& apvts)
        : lp (apvts, pid::duckLP, "Det. LP", duck),
          knee (apvts, pid::duckKnee, "Knee", duck),
          rms (apvts, pid::duckRms, "RMS win.", duck),
          det (apvts, pid::duckDetMode),
          full (apvts, pid::duckFull, "Full duck (-inf)"),
          sync (apvts, pid::duckRelSync, "Tempo-sync release"),
          note (apvts, pid::duckRelNote)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &lp, &knee, &rms, &det, &full, &sync, &note })
            addAndMakeVisible (c);
        full.getProperties().set ("accent", duck.toString());
        sync.getProperties().set ("accent", duck.toString());
        lp.setTooltipText ("Low-pass on the detector only.");
        knee.setTooltipText ("Soft knee around the threshold.");
        rms.setTooltipText ("RMS window (RMS detector only).");
        det.setTooltip ("Peak reacts to every transient; RMS follows the loudness.");
        full.setTooltip ("Duck to silence instead of the Depth value.");
        sync.setTooltip ("Release time as a note value at the current tempo.");
        setSize (420, 230);
    }
    void paint (juce::Graphics& g) override { drawCaption (g, "DETECTOR & RELEASE", { 14, 8, 300, 16 }); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 8);
        r.removeFromTop (20);
        auto knobs = r.removeFromTop (96);
        const int kw = knobs.getWidth() / 4;
        lp.setBounds (knobs.removeFromLeft (kw));
        knee.setBounds (knobs.removeFromLeft (kw));
        rms.setBounds (knobs.removeFromLeft (kw));
        det.setBounds (knobs.withSizeKeepingCentre (knobs.getWidth() - 6, 28));
        r.removeFromTop (8);
        full.setBounds (r.removeFromTop (26));
        r.removeFromTop (6);
        auto row = r.removeFromTop (28);
        sync.setBounds (row.removeFromLeft (190));
        note.setBounds (row);
    }

private:
    Knob lp, knee, rms;
    Segmented det;
    SwitchToggle full, sync;
    Segmented note;
};
} // namespace

void DuckingSection::showMore()
{
    auto content = std::make_unique<DuckMore> (proc.getApvts());
    juce::CallOutBox::launchAsynchronously (std::move (content), more.getScreenBounds(), nullptr);
}

// ============================================================================ OutputSection
OutputSection::OutputSection (ProgDelayProcessor& p)
    : proc (p),
      mix (p.getApvts(), pid::mix, "Mix", text),
      width (p.getApvts(), pid::width, "Width", text),
      dry (p.getApvts(), pid::dry, "Dry", text),
      wet (p.getApvts(), pid::wet, "Wet", text),
      killDry (p.getApvts(), pid::killDry, "Kill Dry")
{
    for (auto* c : std::initializer_list<juce::Component*> { &mix, &width, &dry, &wet, &killDry })
        addAndMakeVisible (c);
    killDry.getProperties().set ("accent", juce::Colour (0xffff5f7e).toString());
    mix.setTooltipText ("Dry/wet balance. The dry signal stays at full level up to 50 %.");
    width.setTooltipText ("Stereo width of the wet signal (M/S): 0 % mono, 200 % extra wide.");
    dry.setTooltipText ("Dry level");
    wet.setTooltipText ("Wet level");
    killDry.setTooltip ("Mute the dry signal (for use on an FX send).");
    startTimerHz (30);
}

void OutputSection::timerCallback()
{
    auto& e = proc.getEngine();
    const float l = e.outPeakL.exchange (0.0f), r = e.outPeakR.exchange (0.0f);
    levelL = std::max (l, levelL * 0.86f);
    levelR = std::max (r, levelR * 0.86f);
    repaint (meterArea);
}

void OutputSection::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, "OUTPUT", { 18, 10, 120, 18 });

    auto m = meterArea;
    auto labels = m.removeFromBottom (16);
    const int mw = 10;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<int> (m.getX() + ch * (mw + 10), m.getY(), mw, m.getHeight()).toFloat();
        g.setColour (juce::Colour (0xff0e1116));
        g.fillRoundedRectangle (bar, 3.0f);
        const float lvl = ch == 0 ? levelL : levelR;
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto fill = bar.withTop (bar.getBottom() - frac * bar.getHeight());
        g.setGradientFill (juce::ColourGradient (straight, 0.0f, bar.getBottom(), juce::Colour (0xffff5f7e), 0.0f, bar.getY(), false));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (textDim);
        g.setFont (font (11.0f));
        g.drawText (ch == 0 ? "L" : "R", juce::Rectangle<float> (bar.getX() - 4.0f, (float) labels.getY(), mw + 8.0f, 14.0f),
                    juce::Justification::centred);
    }
}

void OutputSection::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    r.removeFromTop (24);
    meterArea = r.removeFromRight (34);
    r.removeFromRight (6);
    auto bottom = r.removeFromBottom (30);
    killDry.setBounds (bottom.withSizeKeepingCentre (96, 28));
    const int rh = r.getHeight() / 2;
    auto top = r.removeFromTop (rh);
    mix.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (4, 0));
    width.setBounds (top.reduced (4, 0));
    dry.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0));
    wet.setBounds (r.reduced (4, 0));
}
} // namespace progdelay
