#pragma once
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dsp/Engine.h"
#include "model/PatternBank.h"

namespace testutil
{
using namespace progdelay;

/** Publishes `p` as pattern `slot` (other slots empty) into the engine. */
inline void publish (Engine& e, const std::vector<std::pair<int, Pattern>>& patterns)
{
    auto& tb = e.getBankBuffer();
    auto& snap = tb.writeBuffer();
    snap = BankSnapshot {};
    for (auto& [slot, p] : patterns)
        snap.patterns[(size_t) slot] = BankSnapshot::makePatternAudio (p);
    tb.publish();
}

inline Pattern patternWith (std::initializer_list<Reflection> refl, int resolution = 8)
{
    Pattern p;
    p.resolution = resolution;
    p.reflections = refl;
    return p;
}

/** Wet-only settings: dry killed, mix 50 % (wet gain 1), width 100 %. */
inline Engine::Settings wetOnly (double bpm = 120.0)
{
    Engine::Settings s;
    s.tempoMode = TempoMode::Manual;
    s.manualBpm = bpm;
    s.mix = 0.5f;
    s.killDry = true;
    s.width = 1.0f;
    return s;
}

/** Runs `numSamples` of stereo input through the engine in blocks, returns the output. */
inline juce::AudioBuffer<float> run (Engine& e, const juce::AudioBuffer<float>& input, const Engine::Settings& s,
                                     int blockSize = 512, HostTiming host = {})
{
    juce::AudioBuffer<float> out (2, input.getNumSamples());
    for (int start = 0; start < input.getNumSamples(); start += blockSize)
    {
        const int n = std::min (blockSize, input.getNumSamples() - start);
        juce::AudioBuffer<float> block (2, n);
        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom (ch, 0, input, std::min (ch, input.getNumChannels() - 1), start, n);
        e.process (block, input.getNumChannels(), host, s);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, ch, 0, n);
    }
    return out;
}

inline juce::AudioBuffer<float> impulse (int length, int at = 0, int channels = 1, float amp = 1.0f)
{
    juce::AudioBuffer<float> b (channels, length);
    b.clear();
    for (int ch = 0; ch < channels; ++ch)
        b.setSample (ch, at, amp);
    return b;
}

/** Indices of samples with |x| > threshold. */
inline std::vector<int> nonZero (const juce::AudioBuffer<float>& b, int ch, float threshold = 1.0e-4f)
{
    std::vector<int> idx;
    for (int i = 0; i < b.getNumSamples(); ++i)
        if (std::abs (b.getSample (ch, i)) > threshold)
            idx.push_back (i);
    return idx;
}
} // namespace testutil
