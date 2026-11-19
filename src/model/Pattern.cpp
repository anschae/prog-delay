#include "Pattern.h"

namespace progdelay
{
juce::ValueTree Pattern::toTree() const
{
    juce::ValueTree t (ids::PATTERN);
    t.setProperty (ids::name, name, nullptr);
    t.setProperty (ids::colour, colour.toString(), nullptr);
    t.setProperty (ids::resolution, resolution, nullptr);
    t.setProperty (ids::gridMode, (int) mode, nullptr);
    t.setProperty (ids::lengthWholes, lengthWholes, nullptr);
    t.setProperty (ids::sigNum, sig.num, nullptr);
    t.setProperty (ids::sigDen, sig.den, nullptr);
    for (const auto& r : reflections)
    {
        juce::ValueTree c (ids::REFL);
        c.setProperty (ids::tick, r.tick, nullptr);
        c.setProperty (ids::type, (int) r.type, nullptr);
        c.setProperty (ids::volume, r.volume, nullptr);
        c.setProperty (ids::pan, r.pan, nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

Pattern Pattern::fromTree (const juce::ValueTree& t)
{
    Pattern p;
    p.name = t.getProperty (ids::name, "Pattern").toString();
    const auto col = t.getProperty (ids::colour).toString();
    if (col.isNotEmpty())
        p.colour = juce::Colour::fromString (col);
    p.resolution = (int) t.getProperty (ids::resolution, 8);
    if (! isValidResolution (p.resolution))
        p.resolution = 8;
    p.mode = (GridMode) std::clamp ((int) t.getProperty (ids::gridMode, 0), 0, 2);
    p.lengthWholes = std::clamp ((int) t.getProperty (ids::lengthWholes, 2), kMinLengthWholes, kMaxLengthWholes);
    p.sig.num = std::clamp ((int) t.getProperty (ids::sigNum, 0), 0, 32);
    p.sig.den = (int) t.getProperty (ids::sigDen, 0);
    if (p.sig.num == 0 || ! (p.sig.den == 2 || p.sig.den == 4 || p.sig.den == 8 || p.sig.den == 16))
        p.sig = {};

    for (const auto& c : t)
    {
        if (! c.hasType (ids::REFL))
            continue;
        Reflection r;
        r.tick = (int) c.getProperty (ids::tick, 0);
        if (r.tick <= 0 || r.tick > kMaxLengthWholes * TICKS_PER_WHOLE)
            continue;
        r.type = classifyTick (r.tick);
        r.volume = std::clamp ((float) c.getProperty (ids::volume, 0.7f), 0.0f, 1.0f);
        r.pan = std::clamp ((float) c.getProperty (ids::pan, 0.0f), -1.0f, 1.0f);
        if ((int) p.reflections.size() < kMaxReflections)
            p.reflections.push_back (r);
    }
    std::sort (p.reflections.begin(), p.reflections.end(), [] (auto& a, auto& b) { return a.tick < b.tick; });
    // one reflection per time position
    p.reflections.erase (std::unique (p.reflections.begin(), p.reflections.end(),
                                      [] (auto& a, auto& b) { return a.tick == b.tick; }),
                         p.reflections.end());
    return p;
}

juce::Colour defaultPatternColour (int slot)
{
    static const juce::uint32 cols[kNumPatterns] = { 0xff2ec4d6, 0xffff8a3d, 0xffa58bff, 0xff4cd68a,
                                                     0xfff2c94c, 0xffff5f7e, 0xff5fa8ff, 0xffc0c6d0 };
    return juce::Colour (cols[std::clamp (slot, 0, kNumPatterns - 1)]);
}

Pattern makeEmptyPattern (int slot)
{
    Pattern p;
    p.name = "Pattern " + juce::String (slot + 1);
    p.colour = defaultPatternColour (slot);
    return p;
}

Pattern makeStarterPattern()
{
    auto p = makeEmptyPattern (0);
    p.name = "Starter";
    p.resolution = 8;
    p.lengthWholes = 2;
    // a dotted-eighth style bounce: 3/8, 6/8, 9/8, 12/8, fading, alternating sides
    const float vols[] = { 0.7f, 0.5f, 0.36f, 0.24f };
    const float pans[] = { -0.4f, 0.4f, -0.25f, 0.25f };
    for (int i = 0; i < 4; ++i)
        p.reflections.push_back ({ straightTick (3 * (i + 1), 8), ReflType::Straight, vols[i], pans[i] });
    return p;
}

juce::ValueTree makeDefaultPatternsTree()
{
    juce::ValueTree root (ids::PATTERNS);
    root.setProperty (ids::editedPattern, 0, nullptr);
    root.setProperty (ids::snapMode, 0, nullptr);
    root.setProperty (ids::randAmount, 0.7f, nullptr);
    root.setProperty (ids::randDensity, 0.35f, nullptr);
    for (int i = 0; i < kNumPatterns; ++i)
        root.appendChild ((i == 0 ? makeStarterPattern() : makeEmptyPattern (i)).toTree(), nullptr);
    return root;
}

juce::ValueTree sanitisePatternsTree (const juce::ValueTree& loaded)
{
    auto root = makeDefaultPatternsTree();
    if (! loaded.isValid() || ! loaded.hasType (ids::PATTERNS))
        return root;

    root.setProperty (ids::editedPattern, std::clamp ((int) loaded.getProperty (ids::editedPattern, 0), 0, kNumPatterns - 1), nullptr);
    root.setProperty (ids::snapMode, std::clamp ((int) loaded.getProperty (ids::snapMode, 0), 0, 2), nullptr);
    root.setProperty (ids::randAmount, std::clamp ((float) loaded.getProperty (ids::randAmount, 0.7f), 0.0f, 1.0f), nullptr);
    root.setProperty (ids::randDensity, std::clamp ((float) loaded.getProperty (ids::randDensity, 0.35f), 0.0f, 1.0f), nullptr);

    int slot = 0;
    for (const auto& c : loaded)
    {
        if (slot >= kNumPatterns)
            break;
        if (! c.hasType (ids::PATTERN))
            continue;
        root.removeChild (slot, nullptr);
        root.addChild (Pattern::fromTree (c).toTree(), slot, nullptr);
        ++slot;
    }
    return root;
}

juce::String positionLabel (int tick)
{
    if (tick <= 0)
        return "0";
    // Express as k/den with den a straight note value when possible (3/8, 11/16),
    // otherwise reduced (5/12 for triplets).
    for (int den : { 4, 8, 16, 32 })
        if (tick % straightUnit (den) == 0)
        {
            // prefer the eighth-note view for coarse values (2/8 rather than 1/4 looks odd next to 3/8)
            const int k = tick / straightUnit (den);
            return juce::String (k) + "/" + juce::String (den);
        }
    const int g = gcdInt (tick, TICKS_PER_WHOLE);
    return juce::String (tick / g) + "/" + juce::String (TICKS_PER_WHOLE / g);
}

juce::String panLabel (float pan)
{
    const int v = juce::roundToInt (pan * 100.0f);
    if (v == 0)
        return "C";
    return (v < 0 ? "L" : "R") + juce::String (std::abs (v));
}
} // namespace progdelay
