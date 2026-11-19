#include "MultiTapStage.h"
#include <limits>

namespace progdelay
{
namespace
{
constexpr int64_t kSinceForever = std::numeric_limits<int64_t>::lowest() / 4;
constexpr int64_t kUntilForever = std::numeric_limits<int64_t>::max() / 4;
}

void MultiTapStage::prepare (double sr)
{
    sampleRate = sr;
    rampLen = std::max (1.0, sr * kRampMs * 0.001);
    editFadeLen = std::max (1, (int) (sr * kEditFadeMs * 0.001));
    line.prepare (1, (int64_t) (sr * kMaxDelaySeconds) + 2 * kMaxChunk);
    spwX.prepare (sr);
    spwX.reset (samplesPerWhole (120.0, sr));
    reset();
}

void MultiTapStage::reset() noexcept
{
    line.clear();
    const int active = numSegments > 0 ? getActivePattern() : 0;
    segments[0] = { kSinceForever, active };
    numSegments = 1;
    editFadePos = editFadeLen;
    previous = current;
    spwX.reset (spwX.current() > 0.0 ? spwX.current() : samplesPerWhole (120.0, sampleRate));
    tapPeak.fill (0.0f);
}

void MultiTapStage::setSnapshot (const BankSnapshot* snap, bool crossfade) noexcept
{
    previous = crossfade && current != nullptr ? current : snap;
    current = snap;
    editFadePos = crossfade ? 0 : editFadeLen;
}

void MultiTapStage::switchTo (int pattern, int64_t atTime) noexcept
{
    pattern = std::clamp (pattern, 0, kNumPatterns - 1);
    if (pattern == getActivePattern())
        return;

    // If the last switch has not happened yet (atTime earlier), replace it.
    while (numSegments > 1 && segments[(size_t) numSegments - 1].start >= atTime)
        --numSegments;
    if (pattern == getActivePattern())
        return;

    if (numSegments == kMaxSegments)
    {
        // Too many switches in flight: drop the oldest (its tails get cut).
        for (int i = 1; i < numSegments; ++i)
            segments[(size_t) i - 1] = segments[(size_t) i];
        --numSegments;
        segments[0].start = kSinceForever;
    }
    segments[(size_t) numSegments++] = { atTime, pattern };
}

double MultiTapStage::maxDelayFor (int pattern) const noexcept
{
    int maxTick = 0;
    if (current != nullptr)
        maxTick = std::max (maxTick, current->patterns[(size_t) pattern].maxTick);
    if (previous != nullptr)
        maxTick = std::max (maxTick, previous->patterns[(size_t) pattern].maxTick);
    return maxTick * spwX.maxValue() / TICKS_PER_WHOLE + 4.0;
}

void MultiTapStage::pruneSegments() noexcept
{
    const auto t = (double) line.now();
    while (numSegments > 1)
    {
        const double end = (double) segments[1].start + rampLen;
        if (t - maxDelayFor (segments[0].pattern) > end)
        {
            for (int i = 1; i < numSegments; ++i)
                segments[(size_t) i - 1] = segments[(size_t) i];
            --numSegments;
        }
        else
            break;
    }
}

void MultiTapStage::process (const float* inMono, float* outL, float* outR, int n) noexcept
{
    jassert (n <= kMaxChunk);
    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);

    pruneSegments();

    const int64_t t0 = line.now();
    for (int i = 0; i < n; ++i)
        line.pushMono (inMono[i]);

    if (current == nullptr)
        return;

    bool tempoFading = false;
    for (int i = 0; i < n; ++i)
    {
        spwX.step();
        scaleCur[(size_t) i] = spwX.current() / TICKS_PER_WHOLE;
        scalePrev[(size_t) i] = spwX.previous() / TICKS_PER_WHOLE;
        tempoAlpha[(size_t) i] = spwX.alpha();
        tempoFading = tempoFading || spwX.isFading();

        if (editFadePos < editFadeLen)
            ++editFadePos;
        editAlpha[(size_t) i] = (float) editFadePos / (float) editFadeLen;
    }
    const bool editFading = editAlpha[0] < 1.0f && previous != nullptr && previous != current;

    const double invRamp = 1.0 / rampLen;
    auto ramp = [invRamp] (double x) noexcept { return std::clamp (x * invRamp + 0.5, 0.0, 1.0); };

    for (int s = 0; s < (editFading ? 2 : 1); ++s)
    {
        const BankSnapshot& snap = s == 0 ? *current : *previous;
        const bool isPrevSnap = s == 1;

        for (int k = 0; k < numSegments; ++k)
        {
            const auto& seg = segments[(size_t) k];
            const auto& P = snap.patterns[(size_t) seg.pattern];
            if (P.numTaps == 0)
                continue;

            const int64_t segStart = seg.start;
            const int64_t segEnd = k + 1 < numSegments ? segments[(size_t) k + 1].start : kUntilForever;
            const bool unbounded = segStart <= kSinceForever && segEnd >= kUntilForever;
            const bool trackLevels = (s == 0 && k == numSegments - 1);

            for (int tapIdx = 0; tapIdx < P.numTaps; ++tapIdx)
            {
                const auto& tap = P.taps[(size_t) tapIdx];
                const double tick = tap.tick;
                float peak = 0.0f;

                for (int i = 0; i < n; ++i)
                {
                    const double t = (double) (t0 + i);
                    const double m = t - tick * scaleCur[(size_t) i];

                    double w = 1.0;
                    if (! unbounded)
                    {
                        w = (segStart <= kSinceForever ? 1.0 : ramp (m - (double) segStart))
                          - (segEnd >= kUntilForever ? 0.0 : ramp (m - (double) segEnd));
                        if (w <= 0.0)
                            continue;
                    }
                    if (editFading)
                        w *= isPrevSnap ? 1.0 - editAlpha[(size_t) i] : editAlpha[(size_t) i];

                    float x;
                    if (tempoFading && tempoAlpha[(size_t) i] < 1.0f)
                    {
                        const float a = tempoAlpha[(size_t) i];
                        const double mp = t - tick * scalePrev[(size_t) i];
                        x = (1.0f - a) * line.read (0, mp) + a * line.read (0, m);
                    }
                    else
                        x = line.read (0, m);

                    x *= (float) w;
                    outL[i] += x * tap.gainL;
                    outR[i] += x * tap.gainR;
                    if (trackLevels)
                        peak = std::max (peak, std::abs (x) * std::max (tap.gainL, tap.gainR));
                }

                if (trackLevels)
                    tapPeak[(size_t) tapIdx] = std::max (tapPeak[(size_t) tapIdx], peak);
            }
        }
    }
}

void MultiTapStage::collectTapLevels (std::array<float, kMaxReflections>& dest) noexcept
{
    dest = tapPeak;
    tapPeak.fill (0.0f);
}
} // namespace progdelay
