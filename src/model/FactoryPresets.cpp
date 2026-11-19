#include "FactoryPresets.h"
#include "Pattern.h"
#include "StateSerializer.h"

namespace progdelay
{
namespace
{
using Params = std::vector<std::pair<const char*, float>>;

Reflection S (int k, int res, float vol, float pan) { return { straightTick (k, res), ReflType::Straight, vol, pan }; }
Reflection T (int k, int res, float vol, float pan) { return { tripletTick (k, res), classifyTick (tripletTick (k, res)), vol, pan }; }

Pattern pat (const char* name, int slot, int res, GridMode mode, int len, TimeSig sig, std::vector<Reflection> refl)
{
    Pattern p = makeEmptyPattern (slot);
    p.name = name;
    p.resolution = res;
    p.mode = mode;
    p.lengthWholes = len;
    p.sig = sig;
    p.reflections = std::move (refl);
    return p;
}

juce::ValueTree makeState (const Params& params, const std::vector<std::pair<int, Pattern>>& patterns,
                           const char* soundModeId, int editedSlot = 0)
{
    juce::ValueTree root (StateSerializer::rootType);
    root.setProperty (StateSerializer::stateVersion, kStateVersion, nullptr);
    if (soundModeId != nullptr)
        root.setProperty (StateSerializer::soundModeId, soundModeId, nullptr);

    juce::ValueTree ps ("PARAMS");
    for (auto& [id, v] : params)
    {
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", id, nullptr);
        c.setProperty ("value", v, nullptr);
        ps.appendChild (c, nullptr);
    }
    root.appendChild (ps, nullptr);

    auto tree = makeDefaultPatternsTree();
    tree.removeChild (0, nullptr);
    tree.addChild (makeEmptyPattern (0).toTree(), 0, nullptr);
    for (auto& [slot, p] : patterns)
    {
        tree.removeChild (slot, nullptr);
        tree.addChild (p.toTree(), slot, nullptr);
    }
    tree.setProperty (ids::editedPattern, editedSlot, nullptr);
    root.appendChild (tree, nullptr);
    return root;
}

std::vector<FactoryPreset> build()
{
    std::vector<FactoryPreset> v;

    // ---------------------------------------------------------------- Init
    {
        juce::ValueTree root (StateSerializer::rootType);
        root.setProperty (StateSerializer::stateVersion, kStateVersion, nullptr);
        root.setProperty (StateSerializer::soundModeId, "digital", nullptr);
        root.appendChild (juce::ValueTree ("PARAMS"), nullptr);
        root.appendChild (makeDefaultPatternsTree(), nullptr);
        v.push_back ({ "Init", root });
    }

    // ---------------------------------------------------------------- 7/8 Djent Stab
    {
        const auto verse = pat ("7/8 Verse", 0, 8, GridMode::Combined, 2, { 7, 8 },
                                { T (2, 8, 0.65f, 0.45f), S (3, 8, 0.40f, -0.27f), S (5, 8, 0.55f, -0.60f),
                                  S (7, 8, 0.20f, 1.0f), T (13, 8, 0.30f, -0.10f), S (11, 8, 0.35f, 0.30f),
                                  T (20, 8, 0.18f, -0.80f), S (14, 8, 0.12f, -1.0f) });
        const auto bridge = pat ("10/8 Bridge", 1, 8, GridMode::Straight, 2, { 10, 8 },
                                 { S (3, 8, 0.6f, -0.5f), S (6, 8, 0.45f, 0.5f), S (10, 8, 0.35f, 0.0f), S (13, 8, 0.2f, -0.3f) });
        const auto chorus = pat ("4/4 Chorus", 2, 8, GridMode::Straight, 2, { 4, 4 },
                                 { S (3, 8, 0.55f, -0.4f), S (6, 8, 0.4f, 0.4f), S (9, 8, 0.28f, -0.2f), S (12, 8, 0.18f, 0.2f) });
        const auto solo = pat ("13/16 Solo", 3, 16, GridMode::Straight, 2, { 13, 16 },
                               { S (3, 16, 0.5f, -0.6f), S (7, 16, 0.4f, 0.6f), S (13, 16, 0.32f, 0.0f), S (20, 16, 0.2f, -0.4f) });
        const auto intro = pat ("Intro", 4, 4, GridMode::Triplet, 2, {},
                                { T (2, 4, 0.5f, -0.7f), T (4, 4, 0.4f, 0.7f), T (7, 4, 0.3f, 0.0f) });
        v.push_back ({ "7/8 Djent Stab",
                       makeState ({ { "mix", 35.0f }, { "width", 120.0f }, { "wet", -2.0f }, { "feedback", 38.0f },
                                    { "lowCut", 180.0f }, { "highCut", 6500.0f }, { "s2Level", -4.0f },
                                    { "character", 55.0f }, { "age", 30.0f }, { "tone", 1.5f }, { "colorRefl", 1.0f },
                                    { "duckOn", 1.0f }, { "duckThresh", -24.0f }, { "duckDepth", -14.0f },
                                    { "duckAttack", 4.0f }, { "duckHold", 60.0f }, { "duckRelSync", 1.0f },
                                    { "duckRelNote", 1.0f }, { "duckHP", 150.0f }, { "duckLook", 2.0f },
                                    { "switchSource", 1.0f } },
                                  { { 0, verse }, { 1, bridge }, { 2, chorus }, { 3, solo }, { 4, intro } }, "tape") });
    }

    // ---------------------------------------------------------------- Polyrhythm 3 over 4
    {
        // three evenly spaced hits over each 4/4 bar = half-note triplets
        std::vector<Reflection> r;
        const float pans[] = { -0.6f, 0.0f, 0.6f };
        for (int k = 1; k <= 6; ++k)
            r.push_back (T (2 * k, 4, 0.62f - 0.08f * (float) k, pans[(k - 1) % 3]));
        const auto three = pat ("3 over 4", 0, 4, GridMode::Triplet, 2, { 4, 4 }, r);
        // and the reverse feel in slot 2: four over three (dotted quarters over a 3/4 bar)
        const auto four = pat ("4 over 3", 1, 16, GridMode::Straight, 2, { 3, 4 },
                               { S (3, 16, 0.55f, -0.5f), S (6, 16, 0.45f, 0.5f), S (9, 16, 0.38f, -0.5f), S (12, 16, 0.3f, 0.5f) });
        v.push_back ({ "Polyrhythm 3 over 4",
                       makeState ({ { "mix", 40.0f }, { "feedback", 22.0f }, { "fbBarMode", 1.0f }, { "fbBarNum", 4.0f },
                                    { "fbBarDen", 1.0f }, { "highCut", 9000.0f }, { "lowCut", 120.0f } },
                                  { { 0, three }, { 1, four } }, "digital") });
    }

    // ---------------------------------------------------------------- Dotted Ambience
    {
        std::vector<Reflection> r;
        float vol = 0.7f;
        for (int k = 1; k <= 8; ++k, vol *= 0.8f)
            r.push_back (S (3 * k, 16, vol, k % 2 == 1 ? -0.7f : 0.7f));
        const auto amb = pat ("Dotted 8ths", 0, 16, GridMode::Straight, 2, {}, r);
        v.push_back ({ "Dotted Ambience",
                       makeState ({ { "mix", 32.0f }, { "feedback", 55.0f }, { "stereoMode", 1.0f }, { "lowCut", 250.0f },
                                    { "highCut", 4200.0f }, { "width", 150.0f }, { "character", 40.0f }, { "age", 45.0f },
                                    { "duckOn", 1.0f }, { "duckThresh", -30.0f }, { "duckDepth", -10.0f },
                                    { "duckRelease", 600.0f } },
                                  { { 0, amb } }, "analog") });
    }

    // ---------------------------------------------------------------- Gallop Slap
    {
        const auto gallop = pat ("Gallop", 0, 32, GridMode::Straight, 1, {},
                                 { S (3, 32, 0.55f, -0.8f), S (5, 32, 0.4f, 0.8f), S (8, 32, 0.3f, 0.0f) });
        v.push_back ({ "Gallop Slap",
                       makeState ({ { "mix", 30.0f }, { "feedback", 0.0f }, { "highCut", 8000.0f }, { "width", 140.0f } },
                                  { { 0, gallop } }, "digital") });
    }

    // ---------------------------------------------------------------- Odd Meter Switcher
    {
        const auto five = pat ("5/4 Groove", 0, 8, GridMode::Straight, 2, { 5, 4 },
                               { S (3, 8, 0.6f, -0.5f), S (5, 8, 0.45f, 0.5f), S (10, 8, 0.3f, 0.0f) });
        const auto seven = pat ("7/8 Riff", 1, 8, GridMode::Combined, 2, { 7, 8 },
                                { S (2, 8, 0.5f, -0.4f), T (5, 8, 0.45f, 0.4f), S (7, 8, 0.35f, 0.0f), S (11, 8, 0.2f, -0.6f) });
        const auto six = pat ("6/8 Swing", 2, 8, GridMode::Straight, 2, { 6, 8 },
                              { S (3, 8, 0.55f, -0.6f), S (6, 8, 0.4f, 0.6f), S (9, 8, 0.28f, 0.0f) });
        const auto four = pat ("4/4 Straight", 3, 8, GridMode::Straight, 2, { 4, 4 },
                               { S (3, 8, 0.5f, -0.5f), S (6, 8, 0.4f, 0.5f), S (8, 8, 0.3f, 0.0f) });
        v.push_back ({ "Odd Meter Switcher",
                       makeState ({ { "mix", 33.0f }, { "feedback", 30.0f }, { "switchSource", 1.0f }, { "switchQuant", 2.0f },
                                    { "highCut", 7000.0f } },
                                  { { 0, five }, { 1, seven }, { 2, six }, { 3, four } }, "tape") });
    }

    // ---------------------------------------------------------------- Lo-Fi Ghosts
    {
        const auto ghosts = pat ("Ghosts", 0, 16, GridMode::Combined, 2, {},
                                 { T (4, 16, 0.5f, -0.9f), S (5, 16, 0.35f, 0.9f), T (11, 16, 0.4f, 0.2f), S (14, 16, 0.25f, -0.4f),
                                   T (19, 16, 0.3f, 0.6f) });
        v.push_back ({ "Lo-Fi Ghosts",
                       makeState ({ { "mix", 38.0f }, { "feedback", 45.0f }, { "character", 70.0f }, { "age", 50.0f },
                                    { "tone", -2.0f }, { "colorRefl", 1.0f }, { "stereoMode", 1.0f } },
                                  { { 0, ghosts } }, "lofi") });
    }
    return v;
}
} // namespace

const std::vector<FactoryPreset>& FactoryPresets::all()
{
    static const auto presets = build();
    return presets;
}
} // namespace progdelay
