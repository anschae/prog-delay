#pragma once
// Host parameters (AudioProcessorValueTreeState). IDs are stable: never rename
// an ID; add new parameters with a higher version hint (kParamVersion).

#include <juce_audio_processors/juce_audio_processors.h>

namespace progdelay
{
namespace pid
{
inline constexpr const char* mix          = "mix";
inline constexpr const char* dry          = "dry";
inline constexpr const char* wet          = "wet";
inline constexpr const char* killDry      = "killDry";
inline constexpr const char* feedback     = "feedback";
inline constexpr const char* fbBarMode    = "fbBarMode";
inline constexpr const char* fbBarNum     = "fbBarNum";
inline constexpr const char* fbBarDen     = "fbBarDen";
inline constexpr const char* s1Level      = "s1Level";
inline constexpr const char* s2Level      = "s2Level";
inline constexpr const char* lowCut       = "lowCut";
inline constexpr const char* highCut      = "highCut";
inline constexpr const char* stereoMode   = "stereoMode";
inline constexpr const char* width        = "width";
inline constexpr const char* soundMode    = "soundMode";
inline constexpr const char* character    = "character";
inline constexpr const char* age          = "age";
inline constexpr const char* tone         = "tone";
inline constexpr const char* colorRefl    = "colorRefl";
inline constexpr const char* duckOn       = "duckOn";
inline constexpr const char* duckSource   = "duckSource";
inline constexpr const char* duckHP       = "duckHP";
inline constexpr const char* duckLP       = "duckLP";
inline constexpr const char* duckDetMode  = "duckDetMode";
inline constexpr const char* duckRms      = "duckRms";
inline constexpr const char* duckThresh   = "duckThresh";
inline constexpr const char* duckDepth    = "duckDepth";
inline constexpr const char* duckFull     = "duckFull";
inline constexpr const char* duckKnee     = "duckKnee";
inline constexpr const char* duckAttack   = "duckAttack";
inline constexpr const char* duckHold     = "duckHold";
inline constexpr const char* duckRelease  = "duckRelease";
inline constexpr const char* duckRelSync  = "duckRelSync";
inline constexpr const char* duckRelNote  = "duckRelNote";
inline constexpr const char* duckLook     = "duckLook";
inline constexpr const char* duckTarget   = "duckTarget";
inline constexpr const char* duckPos      = "duckPos";
inline constexpr const char* tempoMode    = "tempoMode";
inline constexpr const char* manualBpm    = "manualBpm";
inline constexpr const char* activePattern = "activePattern";
inline constexpr const char* switchSource = "switchSource";
inline constexpr const char* switchQuant  = "switchQuant";
inline constexpr const char* fallback     = "fallbackPattern";
} // namespace pid

inline constexpr int kParamVersion = 1;

// Choice indices
enum class FbBarMode : int { Host = 0, Manual = 1 };
enum class StereoMode : int { Keep = 0, PingPong = 1, Mono = 2 };
enum class DuckSource : int { Input = 0, Sidechain = 1 };
enum class DetMode : int { Peak = 0, Rms = 1 };
enum class DuckTarget : int { Stage1 = 0, Stage2 = 1, Both = 2 };
enum class DuckPos : int { Pre = 0, Post = 1 };
enum class TempoMode : int { Sync = 0, Manual = 1 };
enum class SwitchSource : int { Automation = 0, FollowSig = 1, Midi = 2 };
enum class SwitchQuant : int { Immediate = 0, Beat = 1, Bar = 2 };

inline constexpr int kFbDenominators[] = { 2, 4, 8, 16 };
/** Tempo-synced release values in whole notes; the last entry (0) = one bar of the host signature. */
inline constexpr double kRelNoteWholes[] = { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 0.0 };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** dB value where the slider bottom means silence. */
inline float dbToGainWithFloor (float db, float floorDb = -60.0f) noexcept
{
    return db <= floorDb ? 0.0f : juce::Decibels::decibelsToGain (db);
}
} // namespace progdelay
