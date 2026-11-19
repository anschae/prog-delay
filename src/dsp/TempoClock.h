#pragma once
// Resolves the effective tempo and time signature for a block (host sync or
// manual, with fallbacks), and provides a click-free delay-time transition
// helper used by both stages.

#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include "model/Timing.h"

namespace progdelay
{
inline constexpr double kMinBpm = 30.0, kMaxBpm = 300.0;

/** What the host told us this block (all optional). */
struct HostTiming
{
    std::optional<double> bpm;
    std::optional<int> sigNum, sigDen;
    bool isPlaying = false;
    std::optional<double> ppqPosition;
    std::optional<double> ppqLastBarStart;

    static HostTiming fromPlayHead (juce::AudioPlayHead* ph)
    {
        HostTiming h;
        if (ph == nullptr)
            return h;
        const auto pos = ph->getPosition();
        if (! pos.hasValue())
            return h;
        if (auto b = pos->getBpm())
            h.bpm = *b;
        if (auto s = pos->getTimeSignature())
        {
            h.sigNum = s->numerator;
            h.sigDen = s->denominator;
        }
        h.isPlaying = pos->getIsPlaying();
        if (auto p = pos->getPpqPosition())
            h.ppqPosition = *p;
        if (auto b = pos->getPpqPositionOfLastBarStart())
            h.ppqLastBarStart = *b;
        return h;
    }
};

class TempoClock
{
public:
    /** Updates from the host info. `manual` = tempo mode Manual (uses manualBpm). */
    void update (const HostTiming& h, bool manual, double manualBpm) noexcept
    {
        if (h.bpm.has_value() && *h.bpm > 1.0)
            lastHostBpm = *h.bpm;
        if (h.sigNum.has_value() && h.sigDen.has_value() && *h.sigNum > 0 && *h.sigDen > 0)
        {
            lastSigNum = std::clamp (*h.sigNum, 1, 64);
            lastSigDen = std::clamp (*h.sigDen, 1, 64);
            hostHasSig = true;
        }
        hostBpmKnown = hostBpmKnown || h.bpm.has_value();
        bpm = std::clamp (manual ? manualBpm : lastHostBpm, kMinBpm, kMaxBpm);
        playing = h.isPlaying;
        ppq = h.ppqPosition;
        ppqBar = h.ppqLastBarStart;
    }

    double getBpm() const noexcept { return bpm; }
    double getHostBpm() const noexcept { return lastHostBpm; }
    int getSigNum() const noexcept { return lastSigNum; }
    int getSigDen() const noexcept { return lastSigDen; }
    bool hasHostSig() const noexcept { return hostHasSig; }
    bool isPlaying() const noexcept { return playing; }
    std::optional<double> getPpq() const noexcept { return ppq; }
    std::optional<double> getPpqBarStart() const noexcept { return ppqBar; }

private:
    double lastHostBpm = 120.0, bpm = 120.0;
    int lastSigNum = 4, lastSigDen = 4;
    bool hostHasSig = false, hostBpmKnown = false, playing = false;
    std::optional<double> ppq, ppqBar;
};

/** A delay time (or any read-position scale) that changes without clicks.
    Crossfade mode: when the target changes, the old and new values are both
    read for `fadeLen` samples and crossfaded (no pitch glide).
    Glide mode: the value slides to the target (tape-style pitch bend). */
class DelayTimeXfade
{
public:
    void prepare (double sampleRate, double fadeMs = 20.0, double glideMs = 250.0)
    {
        fadeLen = std::max (1, (int) (sampleRate * fadeMs * 0.001));
        glideCoef = 1.0 - std::exp (-1.0 / (sampleRate * glideMs * 0.001));
    }

    void reset (double v) noexcept { cur = prev = target = v; pos = fadeLen; }

    void setTarget (double v, bool glideMode) noexcept
    {
        target = v;
        glide = glideMode;
    }

    /** Advance by one sample. */
    void step() noexcept
    {
        if (glide)
        {
            if (pos < fadeLen) { ++pos; return; }   // finish a running crossfade first
            prev = cur;
            cur += (target - cur) * glideCoef;
            if (std::abs (target - cur) < 1.0e-4)
                cur = target;
            prev = cur;
            return;
        }
        if (pos < fadeLen)
            ++pos;
        else if (cur != target)
        {
            prev = cur;
            cur = target;
            pos = 0;
        }
    }

    double current() const noexcept { return cur; }
    double previous() const noexcept { return prev; }
    /** 0 = only previous, 1 = only current. */
    float alpha() const noexcept { return pos >= fadeLen ? 1.0f : (float) pos / (float) fadeLen; }
    bool isFading() const noexcept { return pos < fadeLen; }
    double maxValue() const noexcept { return std::max ({ cur, prev, target }); }

private:
    double cur = 0.0, prev = 0.0, target = 0.0;
    int pos = 0, fadeLen = 1;
    double glideCoef = 0.001;
    bool glide = false;
};
} // namespace progdelay
