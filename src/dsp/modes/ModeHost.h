#pragma once
// Owns one prepared instance of every registered sound mode (no allocation when
// switching) and crossfades between modes over ~30 ms.

#include <memory>
#include <vector>
#include <array>
#include "SoundModeRegistry.h"

namespace progdelay
{
class ModeHost
{
public:
    static constexpr int kMaxChunk = 64;

    ModeHost()
    {
        const auto& reg = SoundModeRegistry::get();
        for (int i = 0; i < reg.size(); ++i)
            modes.push_back (reg[i].create());
    }

    void prepare (double sampleRate)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) kMaxChunk, 2 };
        for (auto& m : modes)
            m->prepare (spec);
        fadeLen = std::max (1, (int) (sampleRate * 0.03));
        fadePos = fadeLen;
    }

    void reset()
    {
        for (auto& m : modes)
            m->reset();
        fadePos = fadeLen;
        prev = cur;
    }

    void setMode (int index) noexcept
    {
        index = std::clamp (index, 0, (int) modes.size() - 1);
        if (index == cur)
            return;
        if (fadePos < fadeLen)
            return;  // finish the running crossfade first; picked up next chunk
        prev = cur;
        cur = index;
        modes[(size_t) cur]->reset();
        fadePos = 0;
    }

    int getMode() const noexcept { return cur; }
    bool prefersGlide() const noexcept { return modes[(size_t) cur]->prefersGlide(); }

    float getDelayModulationSamples (int ch) noexcept
    {
        const float a = (float) fadePos / (float) fadeLen;
        const float mc = modes[(size_t) cur]->getDelayModulationSamples (ch);
        if (a >= 1.0f)
            return mc;
        return a * mc + (1.0f - a) * modes[(size_t) prev]->getDelayModulationSamples (ch);
    }

    /** In-place processing of a stereo chunk (n <= kMaxChunk). */
    void process (float* L, float* R, int n, const ModeParams& p) noexcept
    {
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);

        if (fadePos >= fadeLen)
        {
            modes[(size_t) cur]->process (block, p);
            return;
        }
        std::copy (L, L + n, oldL.data());
        std::copy (R, R + n, oldR.data());
        float* oc[2] = { oldL.data(), oldR.data() };
        juce::dsp::AudioBlock<float> oldBlock (oc, 2, (size_t) n);
        modes[(size_t) prev]->process (oldBlock, p);
        modes[(size_t) cur]->process (block, p);
        for (int i = 0; i < n; ++i)
        {
            const float a = (float) std::min (fadeLen, fadePos + i) / (float) fadeLen;
            L[i] = a * L[i] + (1.0f - a) * oldL[(size_t) i];
            R[i] = a * R[i] + (1.0f - a) * oldR[(size_t) i];
        }
        fadePos = std::min (fadeLen, fadePos + n);
    }

private:
    std::vector<std::unique_ptr<SoundMode>> modes;
    int cur = 0, prev = 0;
    int fadePos = 1, fadeLen = 1;
    std::array<float, kMaxChunk> oldL {}, oldR {};
};
} // namespace progdelay
