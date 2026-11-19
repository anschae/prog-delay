#pragma once
// Circular delay buffer with a power-of-two size and 4-point Hermite reads.
// Positions are absolute sample indices (int64 "write counter" time), which
// makes tail-preserving pattern switching easy: every input sample has a
// unique time stamp.

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace progdelay
{
class DelayLine
{
public:
    /** Allocates for at least `maxDelaySamples` of history. Not real-time safe. */
    void prepare (int numChannels, int64_t maxDelaySamples)
    {
        size_t size = 1;
        while ((int64_t) size < maxDelaySamples + 8)
            size <<= 1;
        mask = (int64_t) size - 1;
        data.assign ((size_t) numChannels, std::vector<float> (size, 0.0f));
        writeIndex = 0;
    }

    void clear() noexcept
    {
        for (auto& ch : data)
            std::fill (ch.begin(), ch.end(), 0.0f);
    }

    int64_t capacity() const noexcept { return mask + 1; }
    int numChannels() const noexcept { return (int) data.size(); }

    /** Absolute time of the next sample to be written. */
    int64_t now() const noexcept { return writeIndex; }

    void write (int channel, int64_t time, float x) noexcept
    {
        data[(size_t) channel][(size_t) (time & mask)] = x;
    }

    /** Writes one frame at now() and advances. */
    void push (const float* frame) noexcept
    {
        for (size_t c = 0; c < data.size(); ++c)
            data[c][(size_t) (writeIndex & mask)] = frame[c];
        ++writeIndex;
    }

    void pushMono (float x) noexcept
    {
        data[0][(size_t) (writeIndex & mask)] = x;
        ++writeIndex;
    }

    /** Advances the write position without writing (the caller already wrote at now()). */
    void advance (int n = 1) noexcept { writeIndex += n; }

    float at (int channel, int64_t time) const noexcept { return data[(size_t) channel][(size_t) (time & mask)]; }

    /** Hermite-interpolated read at absolute (fractional) time t. */
    float read (int channel, double t) const noexcept
    {
        const double fl = std::floor (t);
        const auto i = (int64_t) fl;
        const auto f = (float) (t - fl);
        const auto& d = data[(size_t) channel];
        const float xm1 = d[(size_t) ((i - 1) & mask)];
        const float x0  = d[(size_t) (i & mask)];
        const float x1  = d[(size_t) ((i + 1) & mask)];
        const float x2  = d[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<std::vector<float>> data;
    int64_t mask = 0;
    int64_t writeIndex = 0;
};
} // namespace progdelay
