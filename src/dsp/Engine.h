#pragma once
// The complete DSP: everything the processor does per block, independent of
// the AudioProcessor so the unit tests can drive it directly.
//
//   Input ─┬──────────────── Dry ───────────────────────────────┐
//          └► Stage 1 (multi-tap) ─┬► Stage 2 (feedback loop) ─┐ ├► Mix ► Out
//                                  └───────────────► Wet sum ◄─┘ │
//                                       Ducker ► Width ► Wet ────┘
//
// Processing happens in chunks of at most kChunk samples (any host block size works).

#include <array>
#include <atomic>
#include <memory>
#include "MultiTapStage.h"
#include "FeedbackStage.h"
#include "PatternSwitcher.h"
#include "TempoClock.h"
#include "Parameters.h"

namespace progdelay
{
class Engine
{
public:
    static constexpr int kChunk = MultiTapStage::kMaxChunk;

    struct Settings
    {
        // tempo
        TempoMode tempoMode = TempoMode::Sync;
        double manualBpm = 120.0;
        // output
        float mix = 0.35f;            // 0..1
        float dryDb = 0.0f, wetDb = 0.0f;
        bool killDry = false;
        float width = 1.0f;           // 0..2
        // stage levels
        float s1Db = 0.0f, s2Db = 0.0f;
        // stage 2
        float feedback = 0.0f;        // 0..1
        FbBarMode fbBarMode = FbBarMode::Host;
        int fbNum = 4, fbDen = 4;
        float lowCut = 20.0f, highCut = 20000.0f;
        StereoMode stereo = StereoMode::Keep;
        // sound mode
        int modeIndex = 0;
        float character = 0.5f, age = 0.3f, tone = 0.0f;  // tone -1..1
        bool colorRefl = false;
        // pattern switching
        int activePattern = 0;        // 0-based (automation parameter)
        SwitchSource switchSource = SwitchSource::Automation;
        SwitchQuant switchQuant = SwitchQuant::Immediate;
        int fallbackPattern = -1;
    };

    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Message thread: the latest pattern bank (lock-free hand-over). */
    TripleBuffer<BankSnapshot>& getBankBuffer() noexcept { return bankBuffer; }

    /** buffer: on input the main input (numInputChannels channels), on output stereo. */
    void process (juce::AudioBuffer<float>& buffer, int numInputChannels, const HostTiming& host,
                  const Settings& s, std::optional<int> midiPattern = std::nullopt) noexcept;

    // ---- read-outs for the UI (written on the audio thread)
    std::atomic<int> playingPattern { 0 };
    std::atomic<int> pendingPattern { -1 };
    std::atomic<float> bpmDisplay { 120.0f };
    std::atomic<int> sigNumDisplay { 4 }, sigDenDisplay { 4 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> ppqInBar { 0.0 };
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };
    std::array<std::atomic<float>, kMaxReflections> tapLevels {};

    double getSampleRate() const noexcept { return sampleRate; }
    const TempoClock& getClock() const noexcept { return clock; }
    const MultiTapStage& getStage1() const noexcept { return stage1; }
    /** Samples of one feedback bar as used in the last block. */
    double getFeedbackBarSamples() const noexcept { return lastBarSamples; }

private:
    void acquireBank() noexcept;

    double sampleRate = 48000.0;
    TempoClock clock;
    PatternSwitcher switcher;
    MultiTapStage stage1;
    FeedbackStage stage2;
    ModeHost colorHost;  // sound mode on stage 1 ("Color reflections")
    double lastBarSamples = 96000.0;

    TripleBuffer<BankSnapshot> bankBuffer;
    // Two alternating copies: stage 1 crossfades from the old to the new one.
    std::unique_ptr<BankSnapshot> banks[2];
    int bankIndex = 0;
    bool bankInitialised = false;

    juce::SmoothedValue<float> dryGain, wetGain, s1Gain, s2Gain, widthS, colorMix;

    std::array<float, kChunk> mono {}, s1L {}, s1R {}, s2L {}, s2R {}, colL {}, colR {};
    std::array<float, kMaxReflections> tapScratch {};
};
} // namespace progdelay
