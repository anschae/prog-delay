#include "StateSerializer.h"
#include "Parameters.h"
#include "dsp/modes/SoundModeRegistry.h"

namespace progdelay::StateSerializer
{
juce::ValueTree makeState (juce::AudioProcessorValueTreeState& apvts, const PatternBank& bank)
{
    juce::ValueTree root (rootType);
    root.setProperty (stateVersion, kStateVersion, nullptr);

    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (pid::soundMode)))
    {
        const int idx = modeParam->getIndex();
        const auto& reg = SoundModeRegistry::get();
        if (idx >= 0 && idx < reg.size())
            root.setProperty (soundModeId, reg[idx].id, nullptr);
    }

    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (const_cast<PatternBank&> (bank).getTree().createCopy(), nullptr);
    return root;
}

bool applyState (const juce::ValueTree& state, juce::AudioProcessorValueTreeState& apvts, PatternBank& bank)
{
    if (! state.isValid())
        return false;

    juce::ValueTree params, patterns;
    if (state.hasType (rootType))
    {
        // ---- migrations go here, keyed on (int) state[stateVersion]
        params = state.getChildWithName (apvts.state.getType());
        patterns = state.getChildWithName (ids::PATTERNS);
    }
    else if (state.hasType (apvts.state.getType()))
        params = state;
    else
        return false;

    if (params.isValid())
        apvts.replaceState (params);

    // The sound mode is identified by its stable ID, not the parameter index.
    const auto modeId = state.getProperty (soundModeId).toString();
    if (modeId.isNotEmpty())
    {
        const int idx = SoundModeRegistry::get().indexOf (modeId);
        if (idx >= 0)
            if (auto* p = apvts.getParameter (pid::soundMode))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
    }

    bank.replaceAll (patterns.isValid() ? patterns : makeDefaultPatternsTree());
    return true;
}

void writeBinary (const juce::ValueTree& state, juce::MemoryBlock& dest)
{
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

juce::ValueTree readBinary (const void* data, int size)
{
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (data, size))
        return juce::ValueTree::fromXml (*xml);
    return {};
}
} // namespace progdelay::StateSerializer
