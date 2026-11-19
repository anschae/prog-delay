#pragma once
// Immutable, allocation-free pattern data for the audio thread, and the
// lock-free triple buffer that hands it over from the message thread.

#include <atomic>
#include <array>
#include <cstdint>
#include "Pattern.h"

namespace progdelay
{
struct TapData
{
    int tick = 0;
    float gainL = 0.0f, gainR = 0.0f;  // volume * constant-power pan gains
};

struct PatternAudio
{
    int numTaps = 0;
    int maxTick = 0;          // largest tap tick (0 if no taps)
    int sigNum = 0, sigDen = 0;
    std::array<TapData, kMaxReflections> taps {};
};

struct BankSnapshot
{
    std::array<PatternAudio, kNumPatterns> patterns {};

    static PatternAudio makePatternAudio (const Pattern& p) noexcept
    {
        PatternAudio a;
        a.sigNum = p.sig.num;
        a.sigDen = p.sig.den;
        for (const auto& r : p.reflections)
        {
            if (a.numTaps >= kMaxReflections || r.tick <= 0)
                continue;
            auto& t = a.taps[(size_t) a.numTaps++];
            t.tick = r.tick;
            float l, rr;
            panGains (r.pan, l, rr);
            t.gainL = r.volume * l;
            t.gainR = r.volume * rr;
            a.maxTick = std::max (a.maxTick, r.tick);
        }
        return a;
    }
};

/** Single-writer / single-reader wait-free triple buffer.
    The writer fills `writeBuffer()` and calls `publish()`; the reader calls
    `acquire()` (returns true if newer data arrived) and then reads `readBuffer()`. */
template <typename T>
class TripleBuffer
{
public:
    TripleBuffer() = default;

    T& writeBuffer() noexcept { return buffers[(size_t) back]; }

    void publish() noexcept
    {
        back = middle.exchange (back | dirtyBit, std::memory_order_acq_rel) & indexMask;
    }

    bool acquire() noexcept
    {
        if ((middle.load (std::memory_order_acquire) & dirtyBit) == 0)
            return false;
        front = middle.exchange (front, std::memory_order_acq_rel) & indexMask;
        return true;
    }

    const T& readBuffer() const noexcept { return buffers[(size_t) front]; }

private:
    static constexpr int dirtyBit = 4, indexMask = 3;
    std::array<T, 3> buffers {};
    int back = 0, front = 1;
    std::atomic<int> middle { 2 };
};
} // namespace progdelay
