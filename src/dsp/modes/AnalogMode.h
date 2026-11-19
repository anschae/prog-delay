#pragma once
#include "SoundMode.h"
#include "dsp/Filters.h"

namespace progdelay
{
/** Analog (BBD): band-limited and darker, a sample-and-hold "clock" that adds
    slight aliasing grit, compander breathing, gentle chorus-like modulation
    (stereo, 90 degrees apart) and a faint clock noise while there is signal. */
class AnalogMode final : public SoundMode
{
public:
    juce::String getId() const override { return "analog"; }
    juce::String getDisplayName() const override { return "Analog (BBD)"; }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sr = spec.sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            envFast[ch].setTime (sr, 0.008);
            envSlow[ch].setTime (sr, 0.09);
        }
        reset();
    }

    void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            pre[ch].reset();
            post[ch].reset();
            pre2[ch].reset();
            post2[ch].reset();
            envFast[ch].reset (0.05);
            envSlow[ch].reset (0.05);
            held[ch] = 0.0f;
        }
        clockPhase = 0.0;
        lfoPhase = 0.0;
        mod[0] = mod[1] = 0.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block, const ModeParams& p) noexcept override
    {
        const int n = (int) block.getNumSamples();
        const int chans = std::min (2, (int) block.getNumChannels());
        const float c = p.character, a = p.age;

        const double fc = (8500.0 - 5500.0 * c) * std::pow (2.0, p.tone * 1.2);
        for (int ch = 0; ch < 2; ++ch)
        {
            pre[ch].setup (sr, fc, 0.7);
            pre2[ch].setup (sr, fc * 1.3, 0.55);
            post[ch].setup (sr, fc, 0.7);
            post2[ch].setup (sr, fc * 1.3, 0.55);
        }
        // the BBD clock: lower clock = more aliasing. Above the host rate it does nothing.
        const double clockHz = 46000.0 - 30000.0 * c;
        const double clockInc = clockHz / sr;
        const bool useClock = clockInc < 1.0;
        const float compAmt = 0.35f * c;
        const float noiseLvl = 0.0012f * (0.3f + a);

        for (int i = 0; i < n; ++i)
        {
            bool tickClock = true;
            if (useClock)
            {
                clockPhase += clockInc;
                tickClock = clockPhase >= 1.0;
                if (tickClock)
                    clockPhase -= 1.0;
            }
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = block.getChannelPointer ((size_t) ch);
                float x = d[i];
                const float ef = envFast[ch].process (std::abs (x));
                const float es = envSlow[ch].process (std::abs (x));
                // compressor before the "chip" (fast envelope) ...
                const float gPre = std::clamp (std::pow (std::max (ef, 1.0e-4f) / 0.25f, -compAmt), 0.5f, 2.0f);
                x = pre2[ch].lowpass (pre[ch].lowpass (x * gPre));
                x = std::tanh (x * 1.2f) / 1.2f;
                if (tickClock)
                    held[ch] = x;
                x = useClock ? held[ch] : x;
                x = post2[ch].lowpass (post[ch].lowpass (x));
                // ... expander after it (slow envelope): the mismatch is the breathing
                const float gPost = std::clamp (std::pow (std::max (es, 1.0e-4f) / 0.25f, compAmt), 0.5f, 2.0f);
                x *= gPost;
                x += noise.next() * noiseLvl * std::min (1.0f, es * 12.0f);
                d[i] = x;
            }
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            pre[ch].flushDenormals(); pre2[ch].flushDenormals();
            post[ch].flushDenormals(); post2[ch].flushDenormals();
        }

        // chorus-like modulation of the delay time
        lfoPhase = std::fmod (lfoPhase + n * 0.7 / sr, 1.0);
        const double depth = (0.15 + 1.1 * a) * 0.001 * sr;
        for (int ch = 0; ch < 2; ++ch)
        {
            const double ph = lfoPhase + ch * 0.25;
            const double tri = 4.0 * std::abs (ph - std::floor (ph + 0.5)) - 1.0;  // triangle -1..1
            mod[ch] = (float) (depth * tri);
        }
    }

    float getDelayModulationSamples (int channel) noexcept override { return mod[std::clamp (channel, 0, 1)]; }

private:
    double sr = 48000.0;
    Svf pre[2], pre2[2], post[2], post2[2];
    OnePole envFast[2], envSlow[2];
    float held[2] {};
    double clockPhase = 0.0, lfoPhase = 0.0;
    float mod[2] {};
    Noise noise;
};
} // namespace progdelay
