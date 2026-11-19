#pragma once
#include "SoundMode.h"
#include "dsp/Filters.h"

namespace progdelay
{
/** Tape: soft asymmetric saturation, wow and flutter, progressive HF loss and
    (with Age) a little hiss that only appears while there is signal. Tempo
    changes glide (pitch bend), like a real tape echo. */
class TapeMode final : public SoundMode
{
public:
    juce::String getId() const override { return "tape"; }
    juce::String getDisplayName() const override { return "Tape"; }
    bool prefersGlide() const noexcept override { return true; }

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sr = spec.sampleRate;
        for (auto& e : env)
            e.setTime (sr, 0.05);
        drift.setCutoff (sr, 0.3);
        reset();
    }

    void reset() override
    {
        for (auto& f : lp) f.reset();
        for (auto& f : lp2) f.reset();
        for (auto& f : hissHp) f.reset();
        for (auto& e : env) e.reset();
        drift.reset();
        wowPhase = flutterPhase = 0.0;
        mod = 0.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block, const ModeParams& p) noexcept override
    {
        const int n = (int) block.getNumSamples();
        const int chans = std::min (2, (int) block.getNumChannels());
        const float c = p.character, a = p.age;

        // HF loss: darker with character and age, tone tilts it
        const double cutoff = (15000.0 - 10500.0 * (0.6 * c + 0.4 * a)) * std::pow (2.0, p.tone * 1.3);
        for (int ch = 0; ch < 2; ++ch)
        {
            lp[ch].setup (sr, cutoff, 0.6);
            lp2[ch].setup (sr, cutoff * 1.6, 0.5);
            hissHp[ch].setup (sr, 3000.0);
        }

        const float drive = 1.0f + 5.0f * c;
        const float bias = 0.18f * c;
        const float norm = 1.0f / (drive * (1.0f - std::pow (std::tanh (drive * bias), 2.0f)));
        const float off = std::tanh (drive * bias);
        const float hiss = a > 0.3f ? (a - 0.3f) / 0.7f * 0.004f : 0.0f;

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                const float e = env[ch].process (std::abs (x));
                x = (std::tanh (drive * (x + bias)) - off) * norm;
                x = lp2[ch].lowpass (lp[ch].lowpass (x));
                if (hiss > 0.0f)
                    x += hissHp[ch].highpass (noise.next()) * hiss * std::min (1.0f, e * 10.0f);
                d[i] = x;
            }
            lp[ch].flushDenormals();
            lp2[ch].flushDenormals();
        }

        // wow + flutter + slow random drift (delay-time modulation, in samples)
        const double wowHz = 0.55, flutterHz = 6.3;
        wowPhase = std::fmod (wowPhase + n * wowHz / sr, 1.0);
        flutterPhase = std::fmod (flutterPhase + n * flutterHz / sr, 1.0);
        float dr = 0.0f;
        for (int i = 0; i < n; i += 8)
            dr = drift.process (noise.next());
        const double wowMs = 1.6 * a, flutterMs = 0.09 * a;
        mod = (float) ((wowMs * (std::sin (juce::MathConstants<double>::twoPi * wowPhase) + 2.0 * dr)
                        + flutterMs * std::sin (juce::MathConstants<double>::twoPi * flutterPhase))
                       * 0.001 * sr);
    }

    float getDelayModulationSamples (int) noexcept override { return mod; }

private:
    double sr = 48000.0;
    Svf lp[2], lp2[2], hissHp[2];
    OnePole env[2], drift;
    Noise noise;
    double wowPhase = 0.0, flutterPhase = 0.0;
    float mod = 0.0f;
};
} // namespace progdelay
