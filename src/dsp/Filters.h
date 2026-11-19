#pragma once
// Small per-sample filters (TPT state-variable filter, one-pole) used by the
// feedback loop, the sound modes and the ducking detector.

#include <cmath>
#include <algorithm>

namespace progdelay
{
/** 12 dB/oct TPT state-variable filter (Zavalishin). One instance = one channel. */
class Svf
{
public:
    void setup (double sampleRate, double cutoffHz, double q = 0.7071) noexcept
    {
        const double fc = std::clamp (cutoffHz, 5.0, sampleRate * 0.49);
        g = std::tan (3.141592653589793 * fc / sampleRate);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0; }

    /** Processes one sample; returns low-pass, writes high-pass / band-pass if wanted. */
    inline void tick (double x, double& lp, double& hp, double& bp) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
    inline float lowpass (float x) noexcept { double l, h, b; tick (x, l, h, b); return (float) l; }
    inline float highpass (float x) noexcept { double l, h, b; tick (x, l, h, b); return (float) h; }

    void flushDenormals() noexcept
    {
        if (std::abs (ic1) < 1.0e-15) ic1 = 0.0;
        if (std::abs (ic2) < 1.0e-15) ic2 = 0.0;
    }

private:
    double g = 0.0, k = 1.4142, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double ic1 = 0.0, ic2 = 0.0;
};

/** One-pole low-pass (also used as an envelope smoother). */
class OnePole
{
public:
    void setCutoff (double sampleRate, double hz) noexcept { a = 1.0 - std::exp (-2.0 * 3.141592653589793 * hz / sampleRate); }
    void setTime (double sampleRate, double seconds) noexcept { a = 1.0 - std::exp (-1.0 / std::max (1.0e-6, sampleRate * seconds)); }
    void reset (double v = 0.0) noexcept { y = v; }
    inline float process (float x) noexcept { y += a * (x - y); return (float) y; }
    double value() const noexcept { return y; }

private:
    double a = 1.0, y = 0.0;
};

/** Tiny fast noise source (xorshift), uniform in -1..1. */
class Noise
{
public:
    inline float next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) ((double) state / 2147483648.0 - 1.0);
    }
    void seed (unsigned s) noexcept { state = s == 0 ? 0x1234567u : s; }

private:
    unsigned state = 0x9e3779b9u;
};

/** Smooth bounded saturation: linear below `knee`, approaches 1 above. */
inline float softClip (float x, float knee = 0.9f) noexcept
{
    const float a = std::abs (x);
    if (a <= knee)
        return x;
    const float r = 1.0f - knee;
    const float y = knee + r * std::tanh ((a - knee) / r);
    return x < 0.0f ? -y : y;
}
} // namespace progdelay
