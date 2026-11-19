#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/Engine.h"
#include "model/PatternBank.h"

namespace progdelay
{
class ProgDelayProcessor : public juce::AudioProcessor,
                           private juce::ValueTree::Listener,
                           private juce::Timer
{
public:
    ProgDelayProcessor();
    ~ProgDelayProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return PD_PLUGIN_NAME; }
    bool acceptsMidi() const override { return PD_MIDI_SWITCHING != 0; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- for the editor, presets and tests
    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    PatternBank& getBank() noexcept { return bank; }
    Engine& getEngine() noexcept { return engine; }

    juce::ValueTree captureState();
    void restoreState (const juce::ValueTree& state);

    /** Rebuilds the audio snapshot from the pattern tree (message thread). */
    void publishPatterns();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { publishPatterns(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { publishPatterns(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { publishPatterns(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { publishPatterns(); }
    void valueTreeRedirected (juce::ValueTree&) override { publishPatterns(); }

    Engine::Settings readSettings() const noexcept;
    void timerCallback() override;  // reports lookahead latency changes (message thread)

    juce::AudioProcessorValueTreeState apvts;
    PatternBank bank;
    Engine engine;
    juce::SpinLock publishLock;

    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }

    std::atomic<float> *pMix, *pDry, *pWet, *pKillDry, *pWidth, *pS1, *pS2, *pTempoMode, *pManualBpm,
                       *pActive, *pSwitchSource, *pSwitchQuant, *pFallback, *pFeedback;
    std::atomic<float> *d_on = nullptr, *d_source = nullptr, *d_hp = nullptr, *d_lp = nullptr, *d_det = nullptr, *d_rms = nullptr,
        *d_thresh = nullptr, *d_depth = nullptr, *d_full = nullptr, *d_knee = nullptr, *d_attack = nullptr, *d_hold = nullptr,
        *d_release = nullptr, *d_relSync = nullptr, *d_relNote = nullptr, *d_look = nullptr, *d_target = nullptr, *d_pos = nullptr;
    double currentSampleRate = 48000.0;
    std::atomic<float> *c_age = nullptr, *c_character = nullptr, *c_colorRefl = nullptr, *c_fbBarDen = nullptr, *c_fbBarMode = nullptr, *c_fbBarNum = nullptr, *c_highCut = nullptr, *c_lowCut = nullptr, *c_soundMode = nullptr, *c_stereoMode = nullptr, *c_tone = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgDelayProcessor)
};
} // namespace progdelay
