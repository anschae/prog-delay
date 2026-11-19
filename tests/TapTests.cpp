#include "TestHelpers.h"

using namespace progdelay;
using namespace testutil;
using Catch::Approx;

TEST_CASE ("An impulse produces exactly the pattern's reflections", "[stage1]")
{
    for (int blockSize : { 1, 7, 64, 512, 4096 })
    {
        Engine e;
        e.prepare (48000.0, blockSize);
        auto p = patternWith ({ { straightTick (3, 8), ReflType::Straight, 0.5f, 0.0f },
                                { straightTick (5, 8), ReflType::Straight, 1.0f, -1.0f },
                                { tripletTick (7, 8), ReflType::Triplet, 0.25f, 1.0f } });
        publish (e, { { 0, p } });

        const int len = 48000 * 2;
        auto out = run (e, impulse (len), wetOnly (120.0), blockSize);

        // 120 BPM, 48 kHz: one whole note = 96000 samples
        const int t1 = 36000;               // 3/8
        const int t2 = 60000;               // 5/8
        const int t3 = 7 * 96000 / 12;      // 7 x 1/8T = 7/12 = 56000

        const float c = std::sqrt (0.5f);
        CHECK (out.getSample (0, t1) == Approx (0.5f * c).margin (1e-5));
        CHECK (out.getSample (1, t1) == Approx (0.5f * c).margin (1e-5));
        CHECK (out.getSample (0, t2) == Approx (1.0f).margin (1e-5));
        CHECK (out.getSample (1, t2) == Approx (0.0f).margin (1e-5));
        CHECK (out.getSample (0, t3) == Approx (0.0f).margin (1e-5));
        CHECK (out.getSample (1, t3) == Approx (0.25f).margin (1e-5));

        const auto nzL = nonZero (out, 0);
        const auto nzR = nonZero (out, 1);
        CHECK (nzL == std::vector<int> { t1, t2 });
        CHECK (nzR == std::vector<int> { t1, t3 });
    }
}

TEST_CASE ("Taps follow the tempo, odd meters and the manual BPM", "[stage1]")
{
    for (double bpm : { 90.0, 132.0, 200.0 })
    {
        Engine e;
        e.prepare (44100.0, 256);
        // 11/8, 13/16 and a 7/8-bar-length reflection
        auto p = patternWith ({ { straightTick (11, 8), ReflType::Straight, 1.0f, 0.0f },
                                { straightTick (13, 16), ReflType::Straight, 1.0f, 0.0f } }, 16);
        publish (e, { { 0, p } });
        const double spw = samplesPerWhole (bpm, 44100.0);
        const int len = (int) (spw * 1.5) + 100;
        auto out = run (e, impulse (len), wetOnly (bpm), 256);

        for (double frac : { 11.0 / 8.0, 13.0 / 16.0 })
        {
            const double t = frac * spw;
            const int i = (int) std::floor (t);
            // energy around the expected (possibly fractional) position
            float around = 0.0f;
            for (int k = i - 2; k <= i + 3; ++k)
                around += out.getSample (0, k);
            CHECK (around == Approx (std::sqrt (0.5f)).margin (0.02));
            // nothing well before
            CHECK (std::abs (out.getSample (0, i - 50)) < 1.0e-5f);
        }
    }
}

TEST_CASE ("Host tempo is used in sync mode, with fallbacks", "[stage1]")
{
    Engine e;
    e.prepare (48000.0, 512);
    publish (e, { { 0, patternWith ({ { straightTick (1, 4), ReflType::Straight, 1.0f, 0.0f } }) } });

    auto s = wetOnly();
    s.tempoMode = TempoMode::Sync;
    HostTiming h;
    h.bpm = 150.0;
    auto out = run (e, impulse (48000), s, 512, h);
    // a quarter at 150 BPM = 0.4 s = 19200 samples
    CHECK (nonZero (out, 0) == std::vector<int> { 19200 });
    CHECK (e.getClock().getBpm() == Approx (150.0));

    // no host info: keep the last known tempo
    out = run (e, impulse (48000), s, 512, {});
    CHECK (nonZero (out, 0) == std::vector<int> { 19200 });
}

TEST_CASE ("Tempo changes do not click (crossfade between read positions)", "[stage1]")
{
    Engine e;
    e.prepare (48000.0, 64);
    publish (e, { { 0, patternWith ({ { straightTick (1, 8), ReflType::Straight, 1.0f, 0.0f } }) } });

    // a steady sine through the delay, then a tempo jump
    const int len = 48000;
    juce::AudioBuffer<float> in (1, len);
    for (int i = 0; i < len; ++i)
        in.setSample (0, i, std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / 48000.0) * 0.5f);

    juce::AudioBuffer<float> out (2, len);
    float maxStep = 0.0f, prev = 0.0f;
    for (int start = 0; start < len; start += 64)
    {
        auto s = wetOnly (start < len / 2 ? 120.0 : 97.0);
        juce::AudioBuffer<float> block (2, 64);
        block.copyFrom (0, 0, in, 0, start, 64);
        block.copyFrom (1, 0, in, 0, start, 64);
        e.process (block, 1, {}, s);
        if (start > 20000)
            for (int i = 0; i < 64; ++i)
            {
                maxStep = std::max (maxStep, std::abs (block.getSample (0, i) - prev));
                prev = block.getSample (0, i);
            }
        else
            prev = block.getSample (0, 63);
    }
    // a 220 Hz sine of amplitude ~0.35 changes by at most ~0.01 per sample
    CHECK (maxStep < 0.03f);
}
