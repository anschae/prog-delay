#pragma once
// Plugin state format (stored by the host and used for presets):
//
//   <ProgDelayState stateVersion="1" soundModeId="tape">
//     <PARAMS ...APVTS state...>  (PARAM id=... value=...)
//     <PATTERNS editedPattern=.. snapMode=.. randAmount=.. randDensity=..>
//       <PATTERN name colour resolution gridMode lengthWholes sigNum sigDen>
//         <REFL tick type volume pan/> ...
//
// Saved as JUCE binary XML (copyXmlToBinary). Increase kStateVersion when the
// format changes and add a migration in applyState().

#include <juce_audio_processors/juce_audio_processors.h>
#include "PatternBank.h"

namespace progdelay
{
inline constexpr int kStateVersion = 1;

namespace StateSerializer
{
inline const juce::Identifier rootType { "ProgDelayState" };
inline const juce::Identifier stateVersion { "stateVersion" };
inline const juce::Identifier soundModeId { "soundModeId" };

juce::ValueTree makeState (juce::AudioProcessorValueTreeState& apvts, const PatternBank& bank);

/** Applies a state tree (or a legacy bare APVTS tree). Returns false if unusable. */
bool applyState (const juce::ValueTree& state, juce::AudioProcessorValueTreeState& apvts, PatternBank& bank);

void writeBinary (const juce::ValueTree& state, juce::MemoryBlock& dest);
juce::ValueTree readBinary (const void* data, int size);
} // namespace StateSerializer
} // namespace progdelay
