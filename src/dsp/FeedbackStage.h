#pragma once
// STAGE 2: the feedback delay. Repeats the summed stereo output of stage 1
// every feedback bar (B samples), scaled by Feedback each time:
//
//     s2[n] = proc( fb * (s1[n-B] + s2[n-B]) )
//
// where proc = stereo mode (ping-pong swap / mono) -> low cut -> high cut ->
// sound mode -> soft clipper (prevents runaway at high feedback).
// The buffer stores s1 + s2. B changes are crossfaded (or glided in modes that
// prefer it, e.g. Tape); the sound mode's wow/flutter modulates B.

#include "DelayLine.h"
#include "Filters.h"
#include "TempoClock.h"
#include "modes/ModeHost.h"
#include "Parameters.h"

namespace progdelay
{
class FeedbackStage
{
public:
    static constexpr int kMaxChunk = 64;
    static constexpr double kMaxBarSeconds = 32.0;

    struct Params
    {
        float feedback = 0.0f;        // 0..1
        double barSamples = 48000.0;  // B
        StereoMode stereo = StereoMode::Keep;
        float lowCutHz = 20.0f, highCutHz = 20000.0f;
        int modeIndex = 0;
        ModeParams mode;
    };

    void prepare (double sampleRate);
    void reset() noexcept;

    /** s1 in, s2 out (n <= kMaxChunk). feedGain (optional) scales s1 on its way into the
        loop ("Pre" ducking of stage 2). */
    void process (const float* s1L, const float* s1R, float* s2L, float* s2R, int n, const Params& p,
                  const float* feedGain = nullptr) noexcept;

    ModeHost& getModeHost() noexcept { return modes; }
    double getMaxBarSamples() const noexcept { return maxBar; }

private:
    DelayLine line;
    ModeHost modes;
    DelayTimeXfade barX;
    Svf hp[2], lp[2];
    juce::SmoothedValue<float> fbGain, lowCutS, highCutS;
    double sampleRate = 48000.0, maxBar = 48000.0;
    float modPrev[2] {}, modCur[2] {};
    bool first = true;
};
} // namespace progdelay
