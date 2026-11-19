#pragma once
#include "SoundMode.h"
#include "dsp/Filters.h"

namespace progdelay
{
/** Lo-Fi: sample-rate and bit reduction plus band-limiting. Age adds clock
    jitter and a slight wobble. */
class LoFiMode final : public SoundMode
{
public:
    juce::String getId() const override { return "lofi"; }
    juce::String getDisplayName() const override { return "Lo-Fi"; }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sr = spec.sampleRate;
        reset();
    }

    void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].reset();
            lp[ch].reset();
            lp2[ch].reset();
            held[ch] = 0.0f;
        }
        holdPhase = 0.0;
        wobblePhase = 0.0;
        mod = 0.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block, const ModeParams& p) noexcept override
    {
        const int n = (int) block.getNumSamples();
        const int chans = std::min (2, (int) block.getNumChannels());
        const float c = p.character, a = p.age;
        const double toneMul = std::pow (2.0, p.tone * 1.2);

        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].setup (sr, 90.0 + 350.0 * c);
            lp[ch].setup (sr, (9500.0 - 6500.0 * c) * toneMul);
            lp2[ch].setup (sr, (9500.0 - 6500.0 * c) * toneMul * 1.2, 0.6);
        }
        const double targetRate = 26000.0 - 21000.0 * c;  // 26 kHz .. 5 kHz
        const double inc = std::min (1.0, targetRate / sr);
        const float bits = 15.0f - 10.0f * c;              // 15 .. 5 bits
        const float q = std::pow (2.0f, bits - 1.0f);

        for (int i = 0; i < n; ++i)
        {
            holdPhase += inc * (1.0 + 0.35 * a * noise.next());
            const bool tick = holdPhase >= 1.0;
            if (tick)
                holdPhase -= std::floor (holdPhase);
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = block.getChannelPointer ((size_t) ch);
                float x = hp[ch].highpass (d[i]);
                if (tick || inc >= 1.0)
                    held[ch] = std::round (x * q) / q;
                x = lp2[ch].lowpass (lp[ch].lowpass (held[ch]));
                d[i] = x;
            }
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].flushDenormals(); lp[ch].flushDenormals(); lp2[ch].flushDenormals();
        }

        wobblePhase = std::fmod (wobblePhase + n * 3.1 / sr, 1.0);
        mod = (float) (a * 0.6 * 0.001 * sr * std::sin (juce::MathConstants<double>::twoPi * wobblePhase));
    }

    float getDelayModulationSamples (int) noexcept override { return mod; }

private:
    double sr = 48000.0;
    Svf hp[2], lp[2], lp2[2];
    float held[2] {};
    double holdPhase = 0.0, wobblePhase = 0.0;
    float mod = 0.0f;
    Noise noise;
};
} // namespace progdelay
