#pragma once
// Plain data types for patterns and their ValueTree representation.
//
// The ValueTree (see IDs below) is the editable model: the UI edits it through
// PatternBank (with an UndoManager); the processor turns it into an immutable
// BankSnapshot for the audio thread. Plain structs are used for conversions,
// tests and factory presets.

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include "Timing.h"

namespace progdelay
{
inline constexpr int kNumPatterns = 8;
inline constexpr int kMaxReflections = 256;   // 1/32 combined over 4 whole notes = 256 positions
inline constexpr int kMinLengthWholes = 1;
inline constexpr int kMaxLengthWholes = 4;

namespace ids
{
#define PD_ID(name) inline const juce::Identifier name { #name };
PD_ID (PATTERNS)      // root of all patterns, also carries UI-only state
PD_ID (PATTERN)
PD_ID (REFL)
PD_ID (name)
PD_ID (colour)
PD_ID (resolution)
PD_ID (gridMode)
PD_ID (lengthWholes)
PD_ID (sigNum)        // assigned time signature, 0 = none
PD_ID (sigDen)
PD_ID (tick)
PD_ID (type)
PD_ID (volume)
PD_ID (pan)
// UI state (not undoable, stored with the plugin state)
PD_ID (editedPattern)
PD_ID (snapMode)
PD_ID (randAmount)
PD_ID (randDensity)
#undef PD_ID
}

struct Reflection
{
    int tick = 0;                       // position in ticks of a whole note (> 0)
    ReflType type = ReflType::Straight; // derived from the tick when created / moved
    float volume = 0.7f;                // 0..1
    float pan = 0.0f;                   // -1 (L) .. +1 (R)

    bool operator== (const Reflection&) const = default;
};

struct TimeSig
{
    int num = 0, den = 0;  // 0/0 = none
    bool isValid() const noexcept { return num > 0 && den > 0; }
    bool operator== (const TimeSig&) const = default;
    juce::String toString() const { return isValid() ? juce::String (num) + "/" + juce::String (den) : juce::String ("-"); }
};

struct Pattern
{
    juce::String name { "Pattern" };
    juce::Colour colour { 0xff2ec4d6 };
    int resolution = 8;
    GridMode mode = GridMode::Straight;
    int lengthWholes = 2;
    TimeSig sig;
    std::vector<Reflection> reflections;

    int lengthTicks() const noexcept { return lengthWholes * TICKS_PER_WHOLE; }

    juce::ValueTree toTree() const;
    static Pattern fromTree (const juce::ValueTree&);
};

/** Default accent colours for the 8 slots. */
juce::Colour defaultPatternColour (int slot);

/** A new empty pattern for slot `slot` (0-based). */
Pattern makeEmptyPattern (int slot);

/** The pattern in slot 1 of a fresh instance. */
Pattern makeStarterPattern();

/** Fresh PATTERNS tree with 8 empty patterns and default UI state. */
juce::ValueTree makeDefaultPatternsTree();

/** Makes sure a loaded PATTERNS tree has exactly 8 valid patterns (fills / clamps). */
juce::ValueTree sanitisePatternsTree (const juce::ValueTree& loaded);

/** Position label in whole-note fractions, e.g. "3/8" or "5/12". */
juce::String positionLabel (int tick);

/** "L27", "C", "R100" */
juce::String panLabel (float pan);
} // namespace progdelay
