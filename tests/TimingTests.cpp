#include "TestHelpers.h"
#include "model/Timing.h"

using namespace progdelay;
using Catch::Approx;

TEST_CASE ("Straight note values map to whole-note fractions", "[timing]")
{
    for (int r : kResolutions)
    {
        CHECK (straightUnit (r) * r == TICKS_PER_WHOLE);
        for (int k = 1; k <= 2 * r; ++k)
            CHECK (straightTick (k, r) == k * TICKS_PER_WHOLE / r);
    }
}

TEST_CASE ("Triplet steps are 2/3 of the straight value", "[timing]")
{
    for (int r : kResolutions)
    {
        CHECK (tripletUnit (r) * 3 == straightUnit (r) * 2);
        CHECK (tripletUnit (r) * 3 * r == 2 * TICKS_PER_WHOLE);
    }
    // three 1/8 triplets = one quarter note
    CHECK (tripletTick (3, 8) == straightTick (1, 4));
}

TEST_CASE ("Note values convert to samples", "[timing]")
{
    // The example from the spec: grid 1/8, straight step 3 = 3/8 -> 0.75 s at 120 BPM.
    CHECK (ticksToSeconds (straightTick (3, 8), 120.0) == Approx (0.75));
    CHECK (ticksToSamples (straightTick (3, 8), 120.0, 48000.0) == Approx (36000.0));
    // a whole note at 120 BPM = 2 s; at 30 BPM = 8 s
    CHECK (secondsPerWhole (120.0) == Approx (2.0));
    CHECK (secondsPerWhole (30.0) == Approx (8.0));
    // 1/8 triplet step 3 = a quarter note = 0.5 s at 120 BPM
    CHECK (ticksToSeconds (tripletTick (3, 8), 120.0) == Approx (0.5));
    // 1/32 triplet step 1 at 132 BPM, 44.1 kHz
    CHECK (ticksToSamples (tripletTick (1, 32), 132.0, 44100.0)
           == Approx ((1.0 / 32.0) * (2.0 / 3.0) * (240.0 / 132.0) * 44100.0));

    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int r : kResolutions)
            for (int k = 1; k <= r; ++k)
            {
                CHECK (ticksToSamples (straightTick (k, r), 100.0, sr) == Approx (k / (double) r * 2.4 * sr));
                CHECK (ticksToSamples (tripletTick (k, r), 100.0, sr) == Approx (k / (double) r * (2.0 / 3.0) * 2.4 * sr));
            }
}

TEST_CASE ("Odd meters give the right bar lengths", "[timing]")
{
    CHECK (barTicks (7, 8) == straightTick (7, 8));
    CHECK (barTicks (10, 8) == straightTick (10, 8));
    CHECK (barTicks (13, 16) == straightTick (13, 16));
    CHECK (barTicks (4, 4) == TICKS_PER_WHOLE);
    // 7/8 at 120 BPM = 7 * 0.25 s = 1.75 s
    CHECK (ticksToSeconds (barTicks (7, 8), 120.0) == Approx (1.75));
    // 10/8 at 90 BPM: an eighth = 1/3 s
    CHECK (ticksToSeconds (barTicks (10, 8), 90.0) == Approx (10.0 / 3.0));
    // 13/16 at 140 BPM, 48 kHz
    CHECK (ticksToSamples (barTicks (13, 16), 140.0, 48000.0) == Approx (13.0 / 16.0 * 240.0 / 140.0 * 48000.0));
    // a reflection at 11/8 still fits a 2-whole-note grid
    CHECK (straightTick (11, 8) < 2 * TICKS_PER_WHOLE);
}

TEST_CASE ("Positions are classified straight / triplet and checked against grids", "[timing]")
{
    CHECK (classifyTick (straightTick (3, 8)) == ReflType::Straight);
    CHECK (classifyTick (tripletTick (1, 8)) == ReflType::Triplet);
    CHECK (classifyTick (tripletTick (3, 8)) == ReflType::Straight);  // coincides with a quarter
    CHECK (isOnGrid (straightTick (3, 8), 8, GridMode::Straight));
    CHECK_FALSE (isOnGrid (straightTick (3, 16), 8, GridMode::Straight));
    CHECK (isOnGrid (tripletTick (5, 16), 16, GridMode::Triplet));
    CHECK (isOnGrid (tripletTick (5, 16), 16, GridMode::Combined));
    CHECK_FALSE (isOnGrid (tripletTick (5, 16), 8, GridMode::Combined));
}

TEST_CASE ("Constant-power pan law", "[timing]")
{
    float l, r;
    panGains (0.0f, l, r);
    CHECK (l == Approx (std::sqrt (0.5f)));
    CHECK (r == Approx (std::sqrt (0.5f)));
    panGains (-1.0f, l, r);
    CHECK (l == Approx (1.0f));
    CHECK (r == Approx (0.0f).margin (1e-6));
    panGains (0.27f, l, r);
    CHECK (l * l + r * r == Approx (1.0f));
}
