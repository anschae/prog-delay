#pragma once
// Header bar and the bottom-row sections (Feedback, Sound Mode, Ducking, Output).

#include "Widgets.h"
#include "GridModel.h"

namespace progdelay
{
class ProgDelayProcessor;
class PresetBar;

/** "132.00 BPM": shows the effective tempo; in Manual mode drag or double-click to edit. */
class BpmField : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit BpmField (ProgDelayProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    bool isManual() const;
    ProgDelayProcessor& proc;
    juce::RangedAudioParameter* bpmParam;
    juce::RangedAudioParameter* modeParam;
    float shown = 0.0f, dragStartBpm = 120.0f;
    std::unique_ptr<juce::TextEditor> editor;
};

class HeaderBar : public juce::Component, private juce::Timer
{
public:
    HeaderBar (ProgDelayProcessor& p, PresetBar& presets);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ProgDelayProcessor& proc;
    PresetBar& presets;
    juce::TextButton undo { juce::CharPointer_UTF8 ("\xe2\x86\xb6") }, redo { juce::CharPointer_UTF8 ("\xe2\x86\xb7") };
    Segmented tempoMode, switchSource;
    BpmField bpm;
    juce::Label hostSig;
    juce::ComboBox quant;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> quantAttachment;
    juce::Rectangle<int> logoArea, tempoCaption, switchCaption;
};

class FeedbackSection : public juce::Component, private juce::Timer
{
public:
    explicit FeedbackSection (ProgDelayProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ProgDelayProcessor& proc;
    Segmented barMode, stereo;
    juce::ComboBox num, den;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> numAtt, denAtt;
    juce::Label hostBarLabel;
    Knob feedback, lowCut, highCut, s1, s2;
    juce::Rectangle<int> barCaption, stereoCaption, repeatsArea;
    float lastFb = -1.0f;
};

class SoundModeSection : public juce::Component
{
public:
    explicit SoundModeSection (ProgDelayProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    Knob character, age, tone;
    SwitchToggle colorRefl;
};

class DuckingDisplay;

class DuckingSection : public juce::Component
{
public:
    explicit DuckingSection (ProgDelayProcessor& p);
    ~DuckingSection() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showMore();
    ProgDelayProcessor& proc;
    SwitchToggle on;
    Segmented source, target, position;
    std::unique_ptr<DuckingDisplay> display;
    Knob thresh, depth, attack, hold, release, detHp, look;
    juce::TextButton more { juce::CharPointer_UTF8 ("More \xe2\x96\xbe") };
};

class OutputSection : public juce::Component, private juce::Timer
{
public:
    explicit OutputSection (ProgDelayProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ProgDelayProcessor& proc;
    Knob mix, width, dry, wet;
    ChipToggle killDry;
    juce::Rectangle<int> meterArea;
    float levelL = 0.0f, levelR = 0.0f, holdL = 0.0f, holdR = 0.0f;
};
} // namespace progdelay
