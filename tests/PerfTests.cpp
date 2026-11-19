#include "TestHelpers.h"
#include <chrono>
#include <iostream>

using namespace progdelay;
using namespace testutil;

// Hidden by default (run with: ProgDelayTests "[.perf]")
TEST_CASE ("Performance: worst-case pattern", "[.perf]")
{
    for (int numTaps : { 16, 64, 256 })
    {
        Engine e;
        e.prepare (48000.0, 512);
        Pattern p;
        p.resolution = 32;
        p.mode = GridMode::Combined;
        p.lengthWholes = 4;
        const auto pos = PatternBank::gridPositions (p);
        for (int i = 0; i < numTaps && i < (int) pos.size(); ++i)
            p.reflections.push_back ({ pos[(size_t) i * pos.size() / (size_t) numTaps], ReflType::Straight, 0.5f, 0.0f });
        publish (e, { { 0, p } });
        auto s = wetOnly (120.0);
        s.feedback = 0.6f;
        s.modeIndex = 1;  // tape
        s.colorRefl = true;
        s.duck.on = true;
        s.lowCut = 100.0f;
        s.highCut = 8000.0f;

        juce::AudioBuffer<float> block (2, 512);
        juce::Random rng;
        const int blocks = 48000 * 10 / 512;  // 10 s of audio
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    block.setSample (ch, i, rng.nextFloat() - 0.5f);
            e.process (block, 2, {}, s);
        }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::cout << numTaps << " taps: " << sec / 10.0 * 100.0 << " % of one core (48 kHz stereo)" << std::endl;
    }
}
