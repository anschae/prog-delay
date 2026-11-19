#pragma once
// STAGE 1: the multi-tap delay. Produces exactly the reflections of the active
// pattern (each one tap: delay time, gain, pan) from a mono sum of the input.
//
// Tail-preserving switching: each pattern is active for a window of *input*
// time. A tap of pattern P only reads input samples written while P was
// active. The windows are kept as a short list of segments {start, pattern};
// neighbouring windows overlap with complementary linear ramps (kRampMs), so
// the weights of all segments always sum to 1 and switching never clicks.
// Segments are dropped once no tap of their pattern can still reach them.
//
// Snapshot changes (pattern edits) are crossfaded over kEditFadeMs, so adding,
// removing or moving reflections never clicks either.

#include <array>
#include <atomic>
#include "DelayLine.h"
#include "TempoClock.h"
#include "model/BankSnapshot.h"

namespace progdelay
{
class MultiTapStage
{
public:
    static constexpr int kMaxSegments = 8;
    static constexpr double kRampMs = 5.0;
    static constexpr double kEditFadeMs = 10.0;
    static constexpr int kMaxChunk = 64;
    static constexpr double kMaxDelaySeconds = 32.0 + 0.5;  // 4 whole notes at 30 BPM + margin

    void prepare (double sampleRate);
    void reset() noexcept;

    /** Sets the snapshot to play. The pointed-to data must stay valid and unchanged
        until the next call (the engine keeps two alternating copies).
        `crossfade` = false switches instantly (used at start-up). */
    void setSnapshot (const BankSnapshot* snap, bool crossfade) noexcept;
    bool isSnapshotFading() const noexcept { return editFadePos < editFadeLen; }

    /** Pattern `pattern` takes over for input written from absolute time `atTime` on. */
    void switchTo (int pattern, int64_t atTime) noexcept;

    /** Pattern that newly written input is assigned to. */
    int getActivePattern() const noexcept { return segments[(size_t) numSegments - 1].pattern; }

    /** Samples per whole note; glide = slide the delay times instead of crossfading. */
    void setSamplesPerWhole (double spw, bool glide) noexcept { spwX.setTarget (spw, glide); }

    /** Absolute time of the next input sample. */
    int64_t now() const noexcept { return line.now(); }

    /** Processes up to kMaxChunk samples. inMono is written into the delay line,
        outL/outR receive the sum of all taps (overwritten, not added). */
    void process (const float* inMono, float* outL, float* outR, int numSamples) noexcept;

    /** Per-tap output peak of the active pattern since the last call (for the UI flash). */
    void collectTapLevels (std::array<float, kMaxReflections>& dest) noexcept;

    int getNumSegments() const noexcept { return numSegments; }

private:
    struct Segment
    {
        int64_t start;   // input time where this pattern starts (lowest() = since forever)
        int pattern;
    };

    void pruneSegments() noexcept;
    double maxDelayFor (int pattern) const noexcept;

    DelayLine line;
    double sampleRate = 48000.0;
    double rampLen = 240.0;

    std::array<Segment, kMaxSegments> segments {};
    int numSegments = 1;

    const BankSnapshot* current = nullptr;
    const BankSnapshot* previous = nullptr;
    int editFadePos = 0, editFadeLen = 1;

    DelayTimeXfade spwX;
    std::array<double, kMaxChunk> scaleCur {}, scalePrev {};
    std::array<float, kMaxChunk> tempoAlpha {}, editAlpha {};

    std::array<float, kMaxReflections> tapPeak {};
};
} // namespace progdelay
