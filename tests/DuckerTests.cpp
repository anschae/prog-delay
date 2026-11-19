#include "TestHelpers.h"
#include "PluginProcessor.h"

using namespace progdelay;
using namespace testutil;
using Catch::Approx;

namespace
{
DuckParams duckOn (float thresh = -24.0f, float depth = -12.0f, float knee = 0.0f)
{
    DuckParams p;
    p.on = true;
    p.threshDb = thresh;
    p.depthDb = depth;
    p.kneeDb = knee;
    p.hpHz = 20.0f;
    p.attackMs = 1.0f;
    p.holdMs = 0.0f;
    p.releaseMs = 50.0f;
    return p;
}

/** Runs a constant-amplitude sine of `ampDb` through a ducker, returns the final gain in dB. */
float steadyGrDb (Ducker& d, const DuckParams& p, float ampDb, int samples = 48000)
{
    std::vector<float> x ((size_t) samples), g ((size_t) samples);
    const float a = juce::Decibels::decibelsToGain (ampDb);
    for (int i = 0; i < samples; ++i)
        x[(size_t) i] = a * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0);
    for (int i = 0; i < samples; i += 64)
        d.process (x.data() + i, x.data() + i, std::min (64, samples - i), p, g.data() + i);
    return juce::Decibels::gainToDecibels (g.back(), -200.0f);
}
} // namespace

TEST_CASE ("Ducker gain-reduction curve: threshold, depth, knee", "[ducker]")
{
    auto p = duckOn (-24.0f, -12.0f, 0.0f);
    CHECK (Ducker::targetReductionDb (-30.0f, p) == 0.0f);
    CHECK (Ducker::targetReductionDb (-20.0f, p) == Approx (-12.0f));
    CHECK (Ducker::targetReductionDb (0.0f, p) == Approx (-12.0f));

    p.kneeDb = 12.0f;  // knee from -30 to -18 dB
    CHECK (Ducker::targetReductionDb (-31.0f, p) == 0.0f);
    CHECK (Ducker::targetReductionDb (-24.0f, p) == Approx (-6.0f));
    CHECK (Ducker::targetReductionDb (-17.0f, p) == Approx (-12.0f));
    // monotonic through the knee
    float last = 1.0f;
    for (float l = -40.0f; l <= 0.0f; l += 0.5f)
    {
        const float r = Ducker::targetReductionDb (l, p);
        CHECK (r <= last + 1e-6f);
        last = r;
    }
    p.full = true;
    CHECK (Ducker::targetReductionDb (0.0f, p) == Approx (Ducker::kFloorDb));
    p.on = false;
    CHECK (Ducker::targetReductionDb (0.0f, p) == 0.0f);
}

TEST_CASE ("Ducker reaches the depth above threshold and recovers below", "[ducker]")
{
    Ducker d;
    d.prepare (48000.0);
    auto p = duckOn (-24.0f, -12.0f);
    CHECK (steadyGrDb (d, p, -6.0f) == Approx (-12.0f).margin (0.05));
    CHECK (steadyGrDb (d, p, -40.0f) == Approx (0.0f).margin (0.05));
    p.full = true;
    CHECK (steadyGrDb (d, p, -6.0f) < -150.0f);   // silence
    p.full = false;
    p.depthDb = -48.0f;
    CHECK (steadyGrDb (d, p, -6.0f) == Approx (-48.0f).margin (0.1));
}

TEST_CASE ("Ducker attack, hold and release timing", "[ducker]")
{
    Ducker d;
    d.prepare (48000.0);
    auto p = duckOn (-24.0f, -20.0f);
    p.attackMs = 5.0f;
    p.holdMs = 100.0f;
    p.releaseMs = 200.0f;

    // 0.5 s of loud, then silence
    const int n = 72000;
    std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n);
    for (int i = 0; i < 24000; ++i)
        x[(size_t) i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / 48000.0);
    for (int i = 0; i < n; i += 64)
        d.process (x.data() + i, x.data() + i, 64, p, g.data() + i);
    auto db = [&] (int i) { return juce::Decibels::gainToDecibels (g[(size_t) i], -200.0f); };

    // attack: after 5 ms (one time constant) ~63 % of the way, after 30 ms fully down
    CHECK (db (240) < -10.0f);
    CHECK (db (240) > -16.0f);
    CHECK (db (1440) == Approx (-20.0f).margin (0.1));
    // hold: still fully ducked 80 ms after the signal stopped (the detector needs ~0.1 s to fall)
    CHECK (db (24000 + 48 * 80) == Approx (-20.0f).margin (0.2));
    // release (200 ms time constant): recovering after the hold, back near 0 dB about 1 s later
    CHECK (db (24000 + 48 * 400) > -15.0f);
    CHECK (db (n - 1) > -1.0f);
}

TEST_CASE ("Detector high-pass ignores low end", "[ducker]")
{
    Ducker d;
    d.prepare (48000.0);
    auto p = duckOn (-24.0f, -12.0f);
    // 60 Hz palm-mute rumble at -10 dB
    std::vector<float> x (48000), g (48000);
    for (int i = 0; i < 48000; ++i)
        x[(size_t) i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * i / 48000.0);
    for (int i = 0; i < 48000; i += 64)
        d.process (x.data() + i, x.data() + i, 64, p, g.data() + i);
    CHECK (g.back() < 0.3f);     // ducked without the filter
    d.reset();
    p.hpHz = 1000.0f;
    for (int i = 0; i < 48000; i += 64)
        d.process (x.data() + i, x.data() + i, 64, p, g.data() + i);
    CHECK (g.back() > 0.95f);    // ignored with HP 1 kHz
}

TEST_CASE ("RMS detector follows loudness", "[ducker]")
{
    Ducker d;
    d.prepare (48000.0);
    auto p = duckOn (-20.0f, -12.0f);
    p.mode = DetMode::Rms;
    // sine at -18 dB peak = -21 dB RMS: below the threshold in RMS mode
    CHECK (steadyGrDb (d, p, -18.0f) == Approx (0.0f).margin (0.05));
    p.mode = DetMode::Peak;
    CHECK (steadyGrDb (d, p, -18.0f) == Approx (-12.0f).margin (0.05));
}

TEST_CASE ("Engine: post ducking lowers the wet while playing, pre keeps the loop clean", "[ducker]")
{
    auto makeEngine = [] (Engine& e) {
        e.prepare (48000.0, 512);
        publish (e, { { 0, patternWith ({ { straightTick (1, 8), ReflType::Straight, 1.0f, 0.0f } }) } });
    };
    // a loud burst for 0.1 s, then silence; tap at 1/8 = 12000 samples (still within the hold+release)
    juce::AudioBuffer<float> in (1, 48000 * 4);
    in.clear();
    for (int i = 0; i < 4800; ++i)
        in.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / 48000.0));
    // a second, continuous loud part from 1.0 s on, so the first burst's echo at 1.0 s + is ducked
    for (int i = 48000; i < 48000 * 4; ++i)
        in.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / 48000.0));

    auto s = wetOnly (120.0);
    s.feedback = 0.5f;
    s.fbBarMode = FbBarMode::Manual;  // 4/4 = 96000 samples
    s.duck = duckOn (-30.0f, -24.0f);
    s.duck.releaseMs = 20.0f;

    // reference without ducking
    Engine ref;
    makeEngine (ref);
    auto noDuck = s;
    noDuck.duck.on = false;
    auto outRef = run (ref, in, noDuck);

    Engine post;
    makeEngine (post);
    auto outPost = run (post, in, s);

    auto rms = [] (const juce::AudioBuffer<float>& b, int from, int len) {
        double sum = 0.0;
        for (int i = from; i < from + len; ++i)
            sum += (double) b.getSample (0, i) * b.getSample (0, i);
        return std::sqrt (sum / len);
    };
    // the first burst's echo (12000..16800) is in a gap: not ducked
    CHECK (rms (outPost, 12500, 3000) == Approx (rms (outRef, 12500, 3000)).epsilon (0.05));
    // the feedback repeat of the burst at 108000.. arrives while playing: ducked by ~24 dB
    CHECK (rms (outPost, 108500, 3000) < 0.1 * rms (outRef, 108500, 3000));

    // Pre ducking of stage 2: the loud part never enters the loop
    Engine pre;
    makeEngine (pre);
    auto sp = s;
    sp.duckPos = DuckPos::Pre;
    sp.duckTarget = DuckTarget::Stage2;
    sp.s1Db = -60.0f;  // listen to stage 2 only
    auto outPre = run (pre, in, sp);
    Engine ref2;
    makeEngine (ref2);
    auto noDuck2 = noDuck;
    noDuck2.s1Db = -60.0f;
    auto outRef2 = run (ref2, in, noDuck2);
    // the burst was echoed into the loop during a gap: its repeat is untouched ...
    CHECK (rms (outPre, 108500, 3000) == Approx (rms (outRef2, 108500, 3000)).epsilon (0.05));
    // ... but what was played from 1.0 s on never filled the loop (repeat from 1.0 s + 12000 + 96000)
    CHECK (rms (outPre, 48000 + 108000 + 2000, 3000) < 0.2 * rms (outRef2, 48000 + 108000 + 2000, 3000));
}

TEST_CASE ("Engine: external sidechain drives the ducker", "[ducker]")
{
    Engine e;
    e.prepare (48000.0, 512);
    publish (e, { { 0, patternWith ({ { straightTick (1, 8), ReflType::Straight, 1.0f, 0.0f } }) } });
    auto s = wetOnly (120.0);
    s.duck = duckOn (-30.0f, -30.0f);
    s.duckSource = DuckSource::Sidechain;

    const int len = 48000;
    juce::AudioBuffer<float> sc (2, 512);
    float lastWet = 0.0f;
    for (int start = 0; start < len; start += 512)
    {
        juce::AudioBuffer<float> block (2, 512);
        block.clear();
        if (start == 0)
            block.setSample (0, 0, 1.0f);  // impulse -> echo at 12000
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                sc.setSample (ch, i, 0.5f * (float) std::sin (0.1 * (start + i)));  // loud sidechain all the time
        e.process (block, 2, {}, s, std::nullopt, &sc);
        if (start <= 12000 && 12000 < start + 512)
            lastWet = block.getSample (0, 12000 - start);
    }
    CHECK (std::abs (lastWet) < std::sqrt (0.5f) * 0.05f);  // -30 dB
}

TEST_CASE ("Lookahead delays the dry signal and reports latency", "[ducker]")
{
    Engine e;
    e.prepare (48000.0, 256);
    publish (e, { { 0, Pattern {} } });
    Engine::Settings s;
    s.mix = 0.0f;  // dry only
    s.lookaheadMs = 5.0f;
    auto out = run (e, impulse (4800, 100), s, 256);
    CHECK (e.getLatencySamples() == 240);
    CHECK (nonZero (out, 0) == std::vector<int> { 100 + 240 });

    ProgDelayProcessor proc;
    auto* look = proc.getApvts().getParameter (pid::duckLook);
    look->setValueNotifyingHost (look->convertTo0to1 (2.0f));
    proc.prepareToPlay (48000.0, 512);
    CHECK (proc.getLatencySamples() == 96);
}

TEST_CASE ("Ducker display FIFO delivers frames", "[ducker]")
{
    Ducker d;
    d.prepare (48000.0);
    auto p = duckOn();
    std::vector<float> x (4800, 0.5f), g (4800);
    d.process (x.data(), x.data(), 4800, p, g.data());
    std::array<Ducker::Frame, 64> frames;
    const int n = d.readFrames (frames.data(), 64);
    CHECK (n == 10);  // one per 10 ms
    CHECK (frames[(size_t) n - 1].grDb < -11.0f);
    CHECK (frames[(size_t) n - 1].levelDb == Approx (juce::Decibels::gainToDecibels (0.5f)).margin (0.1));
    CHECK (d.readFrames (frames.data(), 64) == 0);
}
