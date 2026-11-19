#pragma once
#include "SoundMode.h"

namespace progdelay
{
/** Clean: no colouration at all. */
class DigitalMode final : public SoundMode
{
public:
    juce::String getId() const override { return "digital"; }
    juce::String getDisplayName() const override { return "Digital"; }
    void prepare (const juce::dsp::ProcessSpec&) override {}
    void reset() override {}
    void process (juce::dsp::AudioBlock<float>&, const ModeParams&) noexcept override {}
};
} // namespace progdelay
