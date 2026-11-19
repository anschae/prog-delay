#pragma once
// Message-thread model of the 8 pattern slots: a PATTERNS ValueTree edited
// through undoable operations. The processor listens to the tree and publishes
// a BankSnapshot to the audio thread after every change.

#include <juce_data_structures/juce_data_structures.h>
#include "Pattern.h"
#include "BankSnapshot.h"

namespace progdelay
{
class PatternBank
{
public:
    PatternBank();

    juce::ValueTree& getTree() noexcept { return tree; }
    juce::UndoManager& getUndoManager() noexcept { return undo; }

    /** Replaces everything (state load / preset). Clears the undo history. */
    void replaceAll (const juce::ValueTree& patternsTree);

    juce::ValueTree getPatternTree (int slot) const { return tree.getChild (std::clamp (slot, 0, kNumPatterns - 1)); }
    Pattern getPattern (int slot) const { return Pattern::fromTree (getPatternTree (slot)); }

    int getEditedSlot() const { return std::clamp ((int) tree.getProperty (ids::editedPattern, 0), 0, kNumPatterns - 1); }
    void setEditedSlot (int slot) { tree.setProperty (ids::editedPattern, std::clamp (slot, 0, kNumPatterns - 1), nullptr); }
    SnapMode getSnapMode() const { return (SnapMode) std::clamp ((int) tree.getProperty (ids::snapMode, 0), 0, 2); }
    void setSnapMode (SnapMode m) { tree.setProperty (ids::snapMode, (int) m, nullptr); }
    float getRandAmount() const { return (float) tree.getProperty (ids::randAmount, 0.7f); }
    float getRandDensity() const { return (float) tree.getProperty (ids::randDensity, 0.35f); }
    void setRandAmount (float v) { tree.setProperty (ids::randAmount, juce::jlimit (0.0f, 1.0f, v), nullptr); }
    void setRandDensity (float v) { tree.setProperty (ids::randDensity, juce::jlimit (0.0f, 1.0f, v), nullptr); }

    // ---- undo
    void beginEdit (const juce::String& name) { undo.beginNewTransaction (name); }
    bool canUndo() const { return undo.canUndo(); }
    bool canRedo() const { return undo.canRedo(); }
    void undoEdit() { undo.undo(); }
    void redoEdit() { undo.redo(); }

    // ---- reflections (callers start a transaction with beginEdit() first)
    juce::ValueTree findReflection (int slot, int tick) const;
    juce::ValueTree addReflection (int slot, int tick, float volume, float pan = 0.0f);
    void setVolume (juce::ValueTree refl, float volume);
    void setPan (juce::ValueTree refl, float pan);
    bool moveReflection (int slot, juce::ValueTree refl, int newTick);
    void removeReflection (int slot, juce::ValueTree refl);

    // ---- pattern settings (each is one undoable step)
    void setResolution (int slot, int resolution);
    void setGridMode (int slot, GridMode mode);
    void setLength (int slot, int wholes);
    void setTimeSig (int slot, TimeSig sig);
    void setName (int slot, const juce::String& name);
    void setColour (int slot, juce::Colour colour);

    // ---- tools (each is one undoable step)
    void copyPattern (int slot);
    bool canPaste() const { return clipboard.isValid(); }
    void pastePattern (int slot);
    void clearPattern (int slot);
    void duplicatePattern (int from, int to);
    void mirrorPan (int slot);
    void shiftPattern (int slot, int direction);  // -1 / +1 grid steps (wraps)
    void randomizeAll (int slot, float density, float amount, juce::Random& rng);
    void randomizeVolume (int slot, float amount, juce::Random& rng);
    void randomizePan (int slot, float amount, juce::Random& rng);

    /** Replaces the content (grid settings + reflections) of a slot by `p`, keeping
        nothing else. Undoable. */
    void setPattern (int slot, const Pattern& p, const juce::String& undoName);

    /** All clickable positions of the pattern's grid (sorted). In Combined mode the
        snap mode restricts them. Coinciding positions count as straight. */
    static std::vector<int> gridPositions (const Pattern& p, SnapMode snap = SnapMode::Both);
    /** Step used for shifting / randomising. */
    static int stepTicks (const Pattern& p);

    /** Builds the audio snapshot of all 8 patterns. */
    void fillSnapshot (BankSnapshot& dest) const;

private:
    juce::ValueTree tree;
    juce::UndoManager undo { 0, 200 };
    juce::ValueTree clipboard;
};
} // namespace progdelay
