#include "TestHelpers.h"
#include "dsp/modes/SoundModeRegistry.h"

using namespace progdelay;
using namespace testutil;
using Catch::Approx;

namespace
{
Engine::Settings fbSettings (float fb, int num = 4, int den = 4)
{
    auto s = wetOnly (120.0);
    s.feedback = fb;
    s.fbBarMode = FbBarMode::Manual;
    s.fbNum = num;
    s.fbDen = den;
    return s;
}

void prepareWithTap (Engine& e, float pan = 0.0f, int tick = straightTick (1, 4))
{
    e.prepare (48000.0, 512);
    publish (e, { { 0, patternWith ({ { tick, classifyTick (tick), 1.0f, pan } }) } });
}
} // namespace

TEST_CASE ("Feedback repeats the pattern every bar and decays by the feedback amount", "[stage2]")
{
    Engine e;
    prepareWithTap (e);
    auto out = run (e, impulse (48000 * 9), fbSettings (0.5f));  // bar = 96000 samples

    const float c = std::sqrt (0.5f);
    CHECK (out.getSample (0, 24000) == Approx (c).margin (1e-4));
    CHECK (out.getSample (0, 24000 + 96000) == Approx (0.5f * c).margin (1e-4));
    CHECK (out.getSample (0, 24000 + 2 * 96000) == Approx (0.25f * c).margin (1e-4));
    CHECK (out.getSample (0, 24000 + 3 * 96000) == Approx (0.125f * c).margin (1e-4));
    CHECK (out.getSample (1, 24000 + 3 * 96000) == Approx (0.125f * c).margin (1e-4));
    CHECK (nonZero (out, 0, 1.0e-3f) == std::vector<int> { 24000, 120000, 216000, 312000, 408000 });
}

TEST_CASE ("Feedback 0 leaves only stage 1; stage levels scale the parts", "[stage2]")
{
    Engine e;
    prepareWithTap (e);
    auto s = fbSettings (0.0f);
    auto out = run (e, impulse (48000 * 5), s);
    CHECK (nonZero (out, 0) == std::vector<int> { 24000 });

    Engine e2;
    prepareWithTap (e2);
    s = fbSettings (0.5f);
    s.s1Db = -60.0f;  // stage 1 muted: only the repeats
    out = run (e2, impulse (48000 * 4), s);
    CHECK (std::abs (out.getSample (0, 24000)) < 1e-4f);
    CHECK (out.getSample (0, 120000) == Approx (0.5f * std::sqrt (0.5f)).margin (1e-4));
}

TEST_CASE ("Feedback bar follows the host time signature in Host mode", "[stage2]")
{
    Engine e;
    prepareWithTap (e);
    auto s = fbSettings (0.5f);
    s.fbBarMode = FbBarMode::Host;
    HostTiming h;
    h.sigNum = 7;
    h.sigDen = 8;
    auto out = run (e, impulse (48000 * 5), s, 512, h);
    // a 7/8 bar at 120 BPM = 7 * 12000 = 84000 samples
    CHECK (out.getSample (0, 24000 + 84000) == Approx (0.5f * std::sqrt (0.5f)).margin (1e-4));
    CHECK (e.getFeedbackBarSamples() == Approx (84000.0));
}

TEST_CASE ("Manual feedback bars: 10/8 and 13/16", "[stage2]")
{
    for (auto [num, den, samples] : { std::tuple { 10, 8, 120000 }, std::tuple { 13, 16, 78000 } })
    {
        Engine e;
        prepareWithTap (e);
        auto out = run (e, impulse (48000 * 4), fbSettings (0.5f, num, den));
        CHECK (out.getSample (0, 24000 + samples) == Approx (0.5f * std::sqrt (0.5f)).margin (1e-4));
    }
}

TEST_CASE ("Ping-pong swaps sides on every repeat; mono sums", "[stage2]")
{
    Engine e;
    prepareWithTap (e, -1.0f);  // hard left
    auto s = fbSettings (0.5f);
    s.stereo = StereoMode::PingPong;
    auto out = run (e, impulse (48000 * 6), s);
    CHECK (out.getSample (0, 24000) == Approx (1.0f).margin (1e-4));
    CHECK (std::abs (out.getSample (1, 24000)) < 1e-4f);
    CHECK (std::abs (out.getSample (0, 120000)) < 1e-4f);
    CHECK (out.getSample (1, 120000) == Approx (0.5f).margin (1e-4));
    CHECK (out.getSample (0, 216000) == Approx (0.25f).margin (1e-4));

    Engine m;
    prepareWithTap (m, -1.0f);
    s.stereo = StereoMode::Mono;
    out = run (m, impulse (48000 * 4), s);
    CHECK (out.getSample (0, 120000) == Approx (0.25f).margin (1e-4));
    CHECK (out.getSample (1, 120000) == Approx (0.25f).margin (1e-4));
}

TEST_CASE ("High cut in the loop makes every repeat darker", "[stage2]")
{
    Engine e;
    prepareWithTap (e);
    auto s = fbSettings (0.9f);
    s.highCut = 2000.0f;
    // a short noise burst
    juce::AudioBuffer<float> in (1, 48000 * 9);
    in.clear();
    juce::Random rng (3);
    for (int i = 0; i < 480; ++i)
        in.setSample (0, i, rng.nextFloat() - 0.5f);
    auto out = run (e, in, s);

    auto hfEnergy = [&] (int start) {
        double hf = 0.0, all = 0.0;
        for (int i = start + 1; i < start + 2400; ++i)
        {
            const double d = out.getSample (0, i) - out.getSample (0, i - 1);  // crude high-pass
            hf += d * d;
            all += (double) out.getSample (0, i) * out.getSample (0, i);
        }
        return all > 0.0 ? hf / all : 0.0;
    };
    const double r1 = hfEnergy (24000 + 96000);
    const double r3 = hfEnergy (24000 + 3 * 96000);
    CHECK (r3 < r1 * 0.8);
}

TEST_CASE ("Every sound mode stays bounded at 100 % feedback", "[stage2][modes]")
{
    const auto& reg = SoundModeRegistry::get();
    REQUIRE (reg.size() >= 4);
    for (int m = 0; m < reg.size(); ++m)
    {
        Engine e;
        prepareWithTap (e, 0.3f, straightTick (1, 16));
        auto s = fbSettings (1.0f, 1, 8);  // short bar: many repeats
        s.modeIndex = m;
        s.character = 1.0f;
        s.age = 1.0f;
        juce::AudioBuffer<float> in (1, 48000 * 4);
        juce::Random rng (m + 1);
        for (int i = 0; i < in.getNumSamples(); ++i)
            in.setSample (0, i, i < 48000 ? (rng.nextFloat() - 0.5f) * 1.6f : 0.0f);
        auto out = run (e, in, s, 333);
        INFO (reg[m].id);
        bool finite = true;
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                finite = finite && std::isfinite (out.getSample (ch, i));
                peak = std::max (peak, std::abs (out.getSample (ch, i)));
            }
        CHECK (finite);
        CHECK (peak < 4.0f);
    }
}

TEST_CASE ("Modes colour the loop; Digital does not", "[stage2][modes]")
{
    auto repeatOf = [] (int modeIndex) {
        Engine e;
        prepareWithTap (e);
        auto s = fbSettings (0.5f);
        s.modeIndex = modeIndex;
        s.age = 0.0f;
        auto out = run (e, impulse (48000 * 3), s);
        return out;
    };
    const auto& reg = SoundModeRegistry::get();
    auto digital = repeatOf (reg.indexOf ("digital"));
    CHECK (digital.getSample (0, 120000) == Approx (0.5f * std::sqrt (0.5f)).margin (1e-4));
    for (auto id : { "tape", "analog", "lofi" })
    {
        auto o = repeatOf (reg.indexOf (id));
        INFO (id);
        // the repeat's impulse is smeared / changed by the colouration
        CHECK (std::abs (o.getSample (0, 120000) - digital.getSample (0, 120000)) > 1e-3f);
        // stage 1 stays clean (Color reflections off)
        CHECK (o.getSample (0, 24000) == Approx (std::sqrt (0.5f)).margin (1e-4));
    }
}

TEST_CASE ("Switching sound modes crossfades (no click)", "[stage2][modes]")
{
    Engine e;
    prepareWithTap (e, 0.0f, straightTick (1, 16));
    const int len = 48000 * 2;
    juce::AudioBuffer<float> in (1, len);
    for (int i = 0; i < len; ++i)
        in.setSample (0, i, 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 150.0 * i / 48000.0));
    auto s = fbSettings (0.6f, 1, 8);
    float maxStep = 0.0f, prev = 0.0f;
    for (int start = 0; start < len; start += 64)
    {
        s.modeIndex = (start / 12000) % 2 == 0 ? 0 : 1;  // digital <-> tape every 0.25 s
        s.colorRefl = (start / 24000) % 2 == 1;
        s.age = 0.0f;
        s.character = 0.2f;
        juce::AudioBuffer<float> block (2, 64);
        block.copyFrom (0, 0, in, 0, start, 64);
        block.copyFrom (1, 0, in, 0, start, 64);
        e.process (block, 1, {}, s);
        for (int i = 0; i < 64; ++i)
        {
            if (start > 12000)
                maxStep = std::max (maxStep, std::abs (block.getSample (0, i) - prev));
            prev = block.getSample (0, i);
        }
    }
    CHECK (maxStep < 0.08f);
}
