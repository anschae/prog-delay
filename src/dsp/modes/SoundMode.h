#pragma once
// Sound modes colour the feedback loop (and optionally stage 1).
// To add a mode: write one class derived from SoundMode and add ONE line to
// registerAllModes() in SoundModeRegistry.cpp. Always append, never reorder:
// the state stores the mode ID, the parameter stores the index.

#include <juce_dsp/juce_dsp.h>

namespace progdelay
{
struct ModeParams
{
    float character = 0.5f;  // 0..1 amount of colouration
    float age = 0.3f;        // 0..1 modulation and wear
    float tone = 0.0f;       // -1 (dark) .. +1 (bright)
    double bpm = 120.0;
};

class SoundMode
{
public:
    virtual ~SoundMode() = default;
    virtual juce::String getId() const = 0;           // stable, stored in state ("tape")
    virtual juce::String getDisplayName() const = 0;  // "Tape"
    virtual void prepare (const juce::dsp::ProcessSpec&) = 0;
    virtual void reset() = 0;
    virtual void process (juce::dsp::AudioBlock<float>&, const ModeParams&) noexcept = 0;
    /** Delay-time modulation (wow/flutter, chorus) in samples for the current
        position; sampled once per processing chunk (the loop interpolates). */
    virtual float getDelayModulationSamples (int /*channel*/) noexcept { return 0.0f; }
    /** True if tempo / bar-length changes should glide (pitch bend) instead of crossfade. */
    virtual bool prefersGlide() const noexcept { return false; }
};
} // namespace progdelay
