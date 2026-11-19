#pragma once
// Decides which pattern should play (automation / follow time signature / MIDI)
// and when the switch happens (immediately, next beat, next bar).

#include <optional>
#include <cmath>
#include "TempoClock.h"
#include "model/BankSnapshot.h"
#include "Parameters.h"

namespace progdelay
{
class PatternSwitcher
{
public:
    struct Input
    {
        SwitchSource source = SwitchSource::Automation;
        SwitchQuant quant = SwitchQuant::Immediate;
        int automationPattern = 0;      // 0-based
        int fallbackPattern = -1;       // -1 = keep current
        std::optional<int> midiPattern; // set when MIDI selected a pattern this block
    };

    void reset (int initialPattern) noexcept
    {
        target = initialPattern;
        midiSelected = initialPattern;
        pending.reset();
        lastDesired = initialPattern;
        matchedSig = false;
    }

    /** Returns the sample offset in this block where `getTarget()` should take
        over, or nullopt if no switch happens in this block. */
    std::optional<int> process (const Input& in, const TempoClock& clock, const BankSnapshot& bank,
                                int currentPattern, double sampleRate, int numSamples) noexcept
    {
        if (in.midiPattern.has_value())
            midiSelected = std::clamp (*in.midiPattern, 0, kNumPatterns - 1);

        int desired = lastDesired;
        switch (in.source)
        {
            case SwitchSource::Automation: desired = in.automationPattern; break;
            case SwitchSource::Midi: desired = midiSelected; break;
            case SwitchSource::FollowSig:
            {
                const int match = findSigMatch (bank, clock.getSigNum(), clock.getSigDen());
                matchedSig = match >= 0;
                if (match >= 0)
                    desired = match;
                else if (in.fallbackPattern >= 0)
                    desired = in.fallbackPattern;
                else
                    desired = lastDesired;  // keep
                break;
            }
        }
        desired = std::clamp (desired, 0, kNumPatterns - 1);
        lastDesired = desired;

        if (desired == currentPattern && ! pending.has_value())
            return std::nullopt;

        if (desired != currentPattern)
            pending = desired;
        else
        {
            pending.reset();  // the user went back before the switch happened
            return std::nullopt;
        }

        const auto offset = switchOffset (in.quant, clock, sampleRate, numSamples);
        if (! offset.has_value())
            return std::nullopt;

        target = *pending;
        pending.reset();
        return offset;
    }

    int getTarget() const noexcept { return target; }
    std::optional<int> getPending() const noexcept { return pending; }
    bool didMatchSig() const noexcept { return matchedSig; }

    static int findSigMatch (const BankSnapshot& bank, int num, int den) noexcept
    {
        for (int i = 0; i < kNumPatterns; ++i)
        {
            const auto& p = bank.patterns[(size_t) i];
            if (p.sigNum == num && p.sigDen == den)
                return i;
        }
        return -1;
    }

    /** Offset of the next beat / bar boundary inside this block (nullopt = not in this block). */
    static std::optional<int> switchOffset (SwitchQuant q, const TempoClock& clock, double sampleRate, int numSamples) noexcept
    {
        if (q == SwitchQuant::Immediate || ! clock.isPlaying() || ! clock.getPpq().has_value())
            return 0;

        const double ppq = *clock.getPpq();
        const double barStart = clock.getPpqBarStart().value_or (0.0);
        const double beatLen = 4.0 / clock.getSigDen();
        const double unit = q == SwitchQuant::Beat ? beatLen : beatLen * clock.getSigNum();
        const double rel = ppq - barStart;
        double next = std::ceil (rel / unit - 1.0e-9) * unit;
        if (next < rel - 1.0e-9)
            next += unit;
        const double samplesPerPpq = sampleRate * 60.0 / clock.getHostBpm();
        const double off = (next - rel) * samplesPerPpq;
        if (off < numSamples)
            return std::max (0, (int) std::floor (off));
        return std::nullopt;
    }

private:
    int target = 0, midiSelected = 0, lastDesired = 0;
    std::optional<int> pending;
    bool matchedSig = false;
};
} // namespace progdelay
