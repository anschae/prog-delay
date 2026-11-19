#pragma once
// Musical time maths. All positions inside the plugin are integer "ticks" of a
// whole note, so straight and triplet positions are exact and comparable.
//
// TICKS_PER_WHOLE = 6720 = 64 * 3 * 5 * 7: divisible by every straight value down
// to 1/64, by triplets, quintuplets and septuplets (backlog grid modes) and by
// dotted values down to 1/32.

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace progdelay
{
inline constexpr int TICKS_PER_WHOLE = 6720;

enum class GridMode : int { Straight = 0, Triplet = 1, Combined = 2 };
enum class SnapMode : int { Both = 0, Straight = 1, Triplet = 2 };
enum class ReflType : int { Straight = 0, Triplet = 1 };

/** Valid resolutions (note value denominators). */
inline constexpr int kResolutions[] = { 4, 8, 16, 32 };

inline constexpr bool isValidResolution (int r) noexcept { return r == 4 || r == 8 || r == 16 || r == 32; }

/** Ticks of one straight step at resolution 1/r. */
inline constexpr int straightUnit (int resolution) noexcept { return TICKS_PER_WHOLE / resolution; }

/** Ticks of one triplet step at resolution 1/r: (1/r) * 2/3 whole notes. */
inline constexpr int tripletUnit (int resolution) noexcept { return TICKS_PER_WHOLE * 2 / (3 * resolution); }

/** Straight step k at 1/r, in ticks. */
inline constexpr int straightTick (int k, int resolution) noexcept { return k * straightUnit (resolution); }

/** Triplet step k at 1/r, in ticks. */
inline constexpr int tripletTick (int k, int resolution) noexcept { return k * tripletUnit (resolution); }

/** Length of a time signature's bar (num/den) in ticks. */
inline constexpr int barTicks (int numerator, int denominator) noexcept
{
    return denominator > 0 ? numerator * TICKS_PER_WHOLE / denominator : TICKS_PER_WHOLE;
}

/** Seconds of one whole note. BPM always counts quarter notes (Cubase / Logic). */
inline double secondsPerWhole (double bpm) noexcept { return 4.0 * 60.0 / std::max (1.0, bpm); }

inline double samplesPerWhole (double bpm, double sampleRate) noexcept { return secondsPerWhole (bpm) * sampleRate; }

inline double ticksToSamples (double ticks, double bpm, double sampleRate) noexcept
{
    return ticks / TICKS_PER_WHOLE * samplesPerWhole (bpm, sampleRate);
}

inline double ticksToSeconds (double ticks, double bpm) noexcept { return ticks / TICKS_PER_WHOLE * secondsPerWhole (bpm); }

/** The finest grids: a straight 1/32 or a triplet 1/32 step. Used to classify a tick. */
inline constexpr bool isStraightTick (int tick) noexcept { return tick % straightUnit (32) == 0; }
inline constexpr bool isTripletTick (int tick) noexcept { return tick % tripletUnit (32) == 0; }

/** Type of a position: positions where straight and triplet coincide count as straight. */
inline constexpr ReflType classifyTick (int tick) noexcept
{
    return isStraightTick (tick) ? ReflType::Straight : ReflType::Triplet;
}

/** Whether a tick lies on the given grid. */
inline constexpr bool isOnGrid (int tick, int resolution, GridMode mode) noexcept
{
    const bool s = tick % straightUnit (resolution) == 0;
    const bool t = tick % tripletUnit (resolution) == 0;
    switch (mode)
    {
        case GridMode::Straight: return s;
        case GridMode::Triplet:  return t;
        case GridMode::Combined: return s || t;
    }
    return false;
}

inline constexpr int gcdInt (int a, int b) noexcept
{
    while (b != 0) { const int t = a % b; a = b; b = t; }
    return a < 0 ? -a : a;
}

/** Constant-power pan law (-3 dB in the centre). pan in -1 (L) .. +1 (R). */
inline void panGains (float pan, float& left, float& right) noexcept
{
    const float a = (std::clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265358979f;
    left = std::cos (a);
    right = std::sin (a);
}
} // namespace progdelay
