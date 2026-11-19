#include "PatternBank.h"

namespace progdelay
{
PatternBank::PatternBank() : tree (makeDefaultPatternsTree()) {}

void PatternBank::replaceAll (const juce::ValueTree& patternsTree)
{
    const auto clean = sanitisePatternsTree (patternsTree);
    tree.copyPropertiesAndChildrenFrom (clean, nullptr);
    undo.clearUndoHistory();
}

juce::ValueTree PatternBank::findReflection (int slot, int tick) const
{
    for (const auto& c : getPatternTree (slot))
        if (c.hasType (ids::REFL) && (int) c.getProperty (ids::tick) == tick)
            return c;
    return {};
}

juce::ValueTree PatternBank::addReflection (int slot, int tick, float volume, float pan)
{
    auto pt = getPatternTree (slot);
    if (tick <= 0 || findReflection (slot, tick).isValid() || pt.getNumChildren() >= kMaxReflections)
        return {};
    juce::ValueTree c (ids::REFL);
    c.setProperty (ids::tick, tick, nullptr);
    c.setProperty (ids::type, (int) classifyTick (tick), nullptr);
    c.setProperty (ids::volume, juce::jlimit (0.0f, 1.0f, volume), nullptr);
    c.setProperty (ids::pan, juce::jlimit (-1.0f, 1.0f, pan), nullptr);
    pt.appendChild (c, &undo);
    return c;
}

void PatternBank::setVolume (juce::ValueTree refl, float volume)
{
    if (refl.isValid())
        refl.setProperty (ids::volume, juce::jlimit (0.0f, 1.0f, volume), &undo);
}

void PatternBank::setPan (juce::ValueTree refl, float pan)
{
    if (refl.isValid())
        refl.setProperty (ids::pan, juce::jlimit (-1.0f, 1.0f, pan), &undo);
}

bool PatternBank::moveReflection (int slot, juce::ValueTree refl, int newTick)
{
    if (! refl.isValid() || newTick <= 0)
        return false;
    if ((int) refl.getProperty (ids::tick) == newTick)
        return true;
    if (findReflection (slot, newTick).isValid())
        return false;
    refl.setProperty (ids::tick, newTick, &undo);
    refl.setProperty (ids::type, (int) classifyTick (newTick), &undo);
    return true;
}

void PatternBank::removeReflection (int slot, juce::ValueTree refl)
{
    if (refl.isValid())
        getPatternTree (slot).removeChild (refl, &undo);
}

void PatternBank::setResolution (int slot, int resolution)
{
    if (! isValidResolution (resolution))
        return;
    beginEdit ("Grid resolution");
    getPatternTree (slot).setProperty (ids::resolution, resolution, &undo);
}

void PatternBank::setGridMode (int slot, GridMode mode)
{
    beginEdit ("Grid mode");
    getPatternTree (slot).setProperty (ids::gridMode, (int) mode, &undo);
}

void PatternBank::setLength (int slot, int wholes)
{
    beginEdit ("Grid length");
    getPatternTree (slot).setProperty (ids::lengthWholes, juce::jlimit (kMinLengthWholes, kMaxLengthWholes, wholes), &undo);
}

void PatternBank::setTimeSig (int slot, TimeSig sig)
{
    beginEdit ("Pattern time signature");
    auto pt = getPatternTree (slot);
    pt.setProperty (ids::sigNum, sig.isValid() ? sig.num : 0, &undo);
    pt.setProperty (ids::sigDen, sig.isValid() ? sig.den : 0, &undo);
}

void PatternBank::setName (int slot, const juce::String& name)
{
    beginEdit ("Rename pattern");
    getPatternTree (slot).setProperty (ids::name, name.substring (0, 32), &undo);
}

void PatternBank::setColour (int slot, juce::Colour colour)
{
    beginEdit ("Pattern colour");
    getPatternTree (slot).setProperty (ids::colour, colour.toString(), &undo);
}

void PatternBank::setPattern (int slot, const Pattern& p, const juce::String& undoName)
{
    beginEdit (undoName);
    auto pt = getPatternTree (slot);
    const auto src = p.toTree();
    for (int i = pt.getNumChildren(); --i >= 0;)
        pt.removeChild (i, &undo);
    for (int i = 0; i < src.getNumProperties(); ++i)
    {
        const auto name = src.getPropertyName (i);
        pt.setProperty (name, src.getProperty (name), &undo);
    }
    for (const auto& c : src)
        pt.appendChild (c.createCopy(), &undo);
}

void PatternBank::copyPattern (int slot)
{
    clipboard = getPatternTree (slot).createCopy();
}

void PatternBank::pastePattern (int slot)
{
    if (! clipboard.isValid())
        return;
    auto p = Pattern::fromTree (clipboard);
    const auto target = getPattern (slot);
    p.name = target.name;
    p.colour = target.colour;
    setPattern (slot, p, "Paste pattern");
}

void PatternBank::clearPattern (int slot)
{
    auto p = getPattern (slot);
    p.reflections.clear();
    setPattern (slot, p, "Clear pattern");
}

void PatternBank::duplicatePattern (int from, int to)
{
    if (from == to)
        return;
    auto p = getPattern (from);
    p.colour = getPattern (to).colour;
    setPattern (to, p, "Duplicate pattern");
}

void PatternBank::mirrorPan (int slot)
{
    auto p = getPattern (slot);
    for (auto& r : p.reflections)
        r.pan = -r.pan;
    setPattern (slot, p, "Mirror pan");
}

int PatternBank::stepTicks (const Pattern& p)
{
    return p.mode == GridMode::Triplet ? tripletUnit (p.resolution) : straightUnit (p.resolution);
}

void PatternBank::shiftPattern (int slot, int direction)
{
    auto p = getPattern (slot);
    const int step = stepTicks (p) * (direction < 0 ? -1 : 1);
    const int len = p.lengthTicks();
    std::vector<Reflection> moved;
    for (auto r : p.reflections)
    {
        if (r.tick > len)
            continue;   // hidden beyond the grid length: leave out of the shift
        int t = r.tick + step;
        // wrap inside (0, len]
        while (t <= 0) t += len;
        while (t > len) t -= len;
        r.tick = t;
        r.type = classifyTick (t);
        if (std::none_of (moved.begin(), moved.end(), [t] (auto& o) { return o.tick == t; }))
            moved.push_back (r);
    }
    p.reflections = moved;
    setPattern (slot, p, direction < 0 ? "Shift left" : "Shift right");
}

void PatternBank::randomizeAll (int slot, float density, float amount, juce::Random& rng)
{
    auto p = getPattern (slot);
    const auto positions = gridPositions (p, SnapMode::Both);
    std::vector<Reflection> result;
    for (int tick : positions)
    {
        if (rng.nextFloat() >= density)
            continue;
        Reflection r;
        r.tick = tick;
        r.type = classifyTick (tick);
        // "current" values for new reflections: 70 %, centre. Amount 100 % = fully random.
        const float rv = 0.08f + 0.92f * rng.nextFloat();
        const float rp = rng.nextFloat() * 2.0f - 1.0f;
        r.volume = juce::jlimit (0.0f, 1.0f, 0.7f + amount * (rv - 0.7f));
        r.pan = juce::jlimit (-1.0f, 1.0f, amount * rp);
        result.push_back (r);
    }
    // keep anything hidden beyond the grid length
    for (const auto& r : p.reflections)
        if (r.tick > p.lengthTicks())
            result.push_back (r);
    p.reflections = result;
    setPattern (slot, p, "Randomize all");
}

void PatternBank::randomizeVolume (int slot, float amount, juce::Random& rng)
{
    auto p = getPattern (slot);
    for (auto& r : p.reflections)
        r.volume = juce::jlimit (0.0f, 1.0f, r.volume + amount * ((0.05f + 0.95f * rng.nextFloat()) - r.volume));
    setPattern (slot, p, "Randomize volume");
}

void PatternBank::randomizePan (int slot, float amount, juce::Random& rng)
{
    auto p = getPattern (slot);
    for (auto& r : p.reflections)
        r.pan = juce::jlimit (-1.0f, 1.0f, r.pan + amount * ((rng.nextFloat() * 2.0f - 1.0f) - r.pan));
    setPattern (slot, p, "Randomize pan");
}

std::vector<int> PatternBank::gridPositions (const Pattern& p, SnapMode snap)
{
    std::vector<int> out;
    const int len = p.lengthTicks();
    const bool wantStraight = p.mode == GridMode::Straight || (p.mode == GridMode::Combined && snap != SnapMode::Triplet);
    const bool wantTriplet = p.mode == GridMode::Triplet || (p.mode == GridMode::Combined && snap != SnapMode::Straight);
    const int su = straightUnit (p.resolution), tu = tripletUnit (p.resolution);
    if (wantStraight)
        for (int t = su; t <= len; t += su)
            out.push_back (t);
    if (wantTriplet)
        for (int t = tu; t <= len; t += tu)
            if (! (wantStraight && t % su == 0))
                out.push_back (t);
    std::sort (out.begin(), out.end());
    return out;
}

void PatternBank::fillSnapshot (BankSnapshot& dest) const
{
    for (int i = 0; i < kNumPatterns; ++i)
    {
        auto p = getPattern (i);
        const int len = p.lengthTicks();
        p.reflections.erase (std::remove_if (p.reflections.begin(), p.reflections.end(),
                                             [len] (auto& r) { return r.tick > len; }),
                             p.reflections.end());
        dest.patterns[(size_t) i] = BankSnapshot::makePatternAudio (p);
    }
}
} // namespace progdelay
