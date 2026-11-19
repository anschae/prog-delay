#include "TestHelpers.h"
#include "PluginProcessor.h"
#include "model/StateSerializer.h"
#include "model/FactoryPresets.h"

using namespace progdelay;
using namespace testutil;
using Catch::Approx;

namespace
{
// Pattern A: one tap at 1/4 (24000 samples at 120 BPM / 48 kHz), pattern B: one tap at 3/8 (36000)
Pattern tapAt (int tick, float pan = 0.0f) { return patternWith ({ { tick, classifyTick (tick), 1.0f, pan } }); }

juce::AudioBuffer<float> runWithSwitch (Engine& e, const juce::AudioBuffer<float>& in, int switchAtSample, int toPattern,
                                        int blockSize = 512)
{
    juce::AudioBuffer<float> out (2, in.getNumSamples());
    auto s = wetOnly (120.0);
    for (int start = 0; start < in.getNumSamples(); start += blockSize)
    {
        const int n = std::min (blockSize, in.getNumSamples() - start);
        s.activePattern = start + n > switchAtSample ? toPattern : 0;
        juce::AudioBuffer<float> block (2, n);
        block.copyFrom (0, 0, in, 0, start, n);
        block.copyFrom (1, 0, in, 0, start, n);
        e.process (block, 1, {}, s);
        out.copyFrom (0, start, block, 0, 0, n);
        out.copyFrom (1, start, block, 1, 0, n);
    }
    return out;
}
} // namespace

TEST_CASE ("Tail-preserving switching: notes before the switch echo with the old pattern", "[switching]")
{
    Engine e;
    e.prepare (48000.0, 512);
    publish (e, { { 0, tapAt (straightTick (1, 4)) }, { 1, tapAt (straightTick (3, 8)) } });

    // impulse at 1000 (before the switch at 10240), impulse at 20000 (after)
    juce::AudioBuffer<float> in (1, 96000);
    in.clear();
    in.setSample (0, 1000, 1.0f);
    in.setSample (0, 20000, 1.0f);
    auto out = runWithSwitch (e, in, 10240, 1);

    const float c = std::sqrt (0.5f);
    // old impulse -> old pattern (1/4 = 24000 later), even though its echo comes after the switch
    CHECK (out.getSample (0, 1000 + 24000) == Approx (c).margin (1e-4));
    CHECK (std::abs (out.getSample (0, 1000 + 36000)) < 1e-5f);
    // new impulse -> new pattern only
    CHECK (out.getSample (0, 20000 + 36000) == Approx (c).margin (1e-4));
    CHECK (std::abs (out.getSample (0, 20000 + 24000)) < 1e-5f);
    // nothing else
    CHECK (nonZero (out, 0) == std::vector<int> { 25000, 56000 });
}

TEST_CASE ("Switching back and forth keeps every tail and never clicks", "[switching]")
{
    Engine e;
    e.prepare (48000.0, 64);
    publish (e, { { 0, tapAt (straightTick (1, 8)) }, { 1, tapAt (straightTick (1, 4)) } });

    // continuous sine: the switch ramp must keep the output continuous
    const int len = 96000;
    juce::AudioBuffer<float> in (1, len);
    for (int i = 0; i < len; ++i)
        in.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * i / 48000.0));

    juce::AudioBuffer<float> out (2, len);
    auto s = wetOnly (120.0);
    for (int start = 0; start < len; start += 64)
    {
        s.activePattern = (start / 9600) % 2;  // switch every 0.2 s
        juce::AudioBuffer<float> block (2, 64);
        block.copyFrom (0, 0, in, 0, start, 64);
        block.copyFrom (1, 0, in, 0, start, 64);
        e.process (block, 1, {}, s);
        out.copyFrom (0, start, block, 0, 0, 64);
    }
    float maxStep = 0.0f;
    for (int i = 40000; i < len; ++i)
        maxStep = std::max (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
    // 110 Hz, amplitude 0.35: max slope ~0.005 per sample; a hard cut would jump by up to 0.35
    CHECK (maxStep < 0.02f);
    CHECK (e.getStage1().getNumSegments() <= MultiTapStage::kMaxSegments);
}

TEST_CASE ("Switch quantize waits for the next beat / bar", "[switching]")
{
    TempoClock clock;
    HostTiming h;
    h.bpm = 120.0;
    h.sigNum = 7;
    h.sigDen = 8;
    h.isPlaying = true;
    h.ppqLastBarStart = 0.0;
    h.ppqPosition = 0.3;  // an eighth = 0.5 ppq; next beat at 0.5 ppq
    clock.update (h, false, 120.0);

    const double sr = 48000.0;  // 1 ppq = 24000 samples at 120 BPM
    auto beat = PatternSwitcher::switchOffset (SwitchQuant::Beat, clock, sr, 8192);
    REQUIRE (beat.has_value());
    CHECK (*beat == 4800);  // 0.2 ppq
    // next 7/8 bar at 3.5 ppq: not in this block
    CHECK_FALSE (PatternSwitcher::switchOffset (SwitchQuant::Bar, clock, sr, 8192).has_value());
    h.ppqPosition = 3.4;
    clock.update (h, false, 120.0);
    auto bar = PatternSwitcher::switchOffset (SwitchQuant::Bar, clock, sr, 8192);
    REQUIRE (bar.has_value());
    CHECK (*bar == 2400);
    // stopped transport: immediate
    h.isPlaying = false;
    clock.update (h, false, 120.0);
    CHECK (PatternSwitcher::switchOffset (SwitchQuant::Bar, clock, sr, 512) == std::optional<int> (0));
}

TEST_CASE ("Follow time signature selects the matching pattern, else keeps or falls back", "[switching]")
{
    BankSnapshot bank;
    bank.patterns[2].sigNum = 7;
    bank.patterns[2].sigDen = 8;
    bank.patterns[5].sigNum = 13;
    bank.patterns[5].sigDen = 16;

    PatternSwitcher sw;
    sw.reset (0);
    TempoClock clock;
    HostTiming h;
    h.sigNum = 7;
    h.sigDen = 8;
    clock.update (h, false, 120.0);

    PatternSwitcher::Input in;
    in.source = SwitchSource::FollowSig;
    auto off = sw.process (in, clock, bank, 0, 48000.0, 512);
    REQUIRE (off.has_value());
    CHECK (sw.getTarget() == 2);

    h.sigNum = 13;
    h.sigDen = 16;
    clock.update (h, false, 120.0);
    sw.process (in, clock, bank, 2, 48000.0, 512);
    CHECK (sw.getTarget() == 5);

    // 4/4: no match -> keep 5
    h.sigNum = 4;
    h.sigDen = 4;
    clock.update (h, false, 120.0);
    CHECK_FALSE (sw.process (in, clock, bank, 5, 48000.0, 512).has_value());
    CHECK (sw.getTarget() == 5);

    // ... unless a fallback is set
    in.fallbackPattern = 0;
    REQUIRE (sw.process (in, clock, bank, 5, 48000.0, 512).has_value());
    CHECK (sw.getTarget() == 0);
}

TEST_CASE ("Automation parameter switches the playing pattern in the processor", "[switching]")
{
    ProgDelayProcessor proc;
    proc.getBank().setPattern (1, tapAt (straightTick (1, 8)), "test");
    proc.prepareToPlay (48000.0, 256);
    auto* p = proc.getApvts().getParameter (pid::activePattern);
    p->setValueNotifyingHost (p->convertTo0to1 (2.0f));
    juce::AudioBuffer<float> buf (4, 256);
    buf.clear();
    juce::MidiBuffer midi;
    proc.processBlock (buf, midi);
    CHECK (proc.getEngine().playingPattern.load() == 1);
}

TEST_CASE ("State save / load round trip", "[state]")
{
    ProgDelayProcessor a;
    auto& bank = a.getBank();
    bank.setPattern (3, patternWith ({ { straightTick (3, 8), ReflType::Straight, 0.4f, -0.27f },
                                       { tripletTick (5, 16), ReflType::Triplet, 0.8f, 0.5f } }, 16), "x");
    bank.setName (3, "7/8 Verse");
    bank.setTimeSig (3, { 7, 8 });
    bank.setGridMode (3, GridMode::Combined);
    bank.setLength (3, 3);
    bank.setColour (3, juce::Colour (0xffff5f7e));
    bank.setEditedSlot (3);
    a.getApvts().getParameter (pid::feedback)->setValueNotifyingHost (0.42f);
    auto* mode = a.getApvts().getParameter (pid::soundMode);
    mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));  // analog

    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    ProgDelayProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    const auto p = b.getBank().getPattern (3);
    CHECK (p.name == "7/8 Verse");
    CHECK (p.sig == TimeSig { 7, 8 });
    CHECK (p.mode == GridMode::Combined);
    CHECK (p.lengthWholes == 3);
    CHECK (p.resolution == 16);
    CHECK (p.colour == juce::Colour (0xffff5f7e));
    REQUIRE (p.reflections.size() == 2);
    CHECK (p.reflections[0].tick == tripletTick (5, 16));
    CHECK (p.reflections[0].type == ReflType::Triplet);
    CHECK (p.reflections[0].volume == Approx (0.8f));
    CHECK (p.reflections[1].pan == Approx (-0.27f));
    CHECK (b.getBank().getEditedSlot() == 3);
    CHECK (b.getApvts().getParameter (pid::feedback)->getValue() == Approx (0.42f).margin (0.002));
    CHECK (juce::roundToInt (b.getApvts().getRawParameterValue (pid::soundMode)->load()) == 2);

    // the state carries a version and the sound mode ID
    const auto tree = StateSerializer::readBinary (mb.getData(), (int) mb.getSize());
    CHECK ((int) tree.getProperty (StateSerializer::stateVersion) == kStateVersion);
    CHECK (tree.getProperty (StateSerializer::soundModeId).toString() == "analog");

    // the snapshot reaches the audio side
    juce::MemoryBlock mb2;
    b.getStateInformation (mb2);
    CHECK (mb2.getSize() > 0);
}

TEST_CASE ("Garbage state is ignored", "[state]")
{
    ProgDelayProcessor p;
    const char junk[] = "not a state at all";
    p.setStateInformation (junk, (int) sizeof (junk));
    CHECK (p.getBank().getPattern (0).name == "Starter");
}

TEST_CASE ("Undo / redo of grid edits", "[bank]")
{
    PatternBank bank;
    bank.clearPattern (0);
    bank.beginEdit ("add");
    auto r = bank.addReflection (0, straightTick (3, 8), 0.5f);
    REQUIRE (r.isValid());
    bank.beginEdit ("vol");
    bank.setVolume (r, 0.9f);
    CHECK (bank.getPattern (0).reflections[0].volume == Approx (0.9f));
    bank.undoEdit();
    CHECK (bank.getPattern (0).reflections[0].volume == Approx (0.5f));
    bank.undoEdit();
    CHECK (bank.getPattern (0).reflections.empty());
    bank.redoEdit();
    CHECK (bank.getPattern (0).reflections.size() == 1);

    // one reflection per position
    bank.beginEdit ("dup");
    CHECK_FALSE (bank.addReflection (0, straightTick (3, 8), 0.2f).isValid());
}

TEST_CASE ("Pattern tools: mirror, shift, duplicate, copy/paste, resolution keeps absolute time", "[bank]")
{
    PatternBank bank;
    bank.setPattern (0, patternWith ({ { straightTick (1, 8), ReflType::Straight, 0.5f, -0.5f },
                                       { straightTick (16, 8), ReflType::Straight, 0.3f, 1.0f } }), "x");
    bank.mirrorPan (0);
    auto p = bank.getPattern (0);
    CHECK (p.reflections[0].pan == Approx (0.5f));
    CHECK (p.reflections[1].pan == Approx (-1.0f));

    bank.shiftPattern (0, +1);  // 1/8 -> 2/8, 16/8 wraps to 1/8
    p = bank.getPattern (0);
    REQUIRE (p.reflections.size() == 2);
    CHECK (p.reflections[0].tick == straightTick (1, 8));
    CHECK (p.reflections[1].tick == straightTick (2, 8));

    bank.duplicatePattern (0, 5);
    CHECK (bank.getPattern (5).reflections.size() == 2);
    CHECK (bank.getPattern (5).colour == defaultPatternColour (5));
    bank.copyPattern (0);
    bank.clearPattern (0);
    CHECK (bank.getPattern (0).reflections.empty());
    bank.pastePattern (0);
    CHECK (bank.getPattern (0).reflections.size() == 2);

    // changing resolution keeps the absolute time; 3/16 is then off-grid at 1/8
    bank.setPattern (1, patternWith ({ { straightTick (3, 16), ReflType::Straight, 0.5f, 0.0f } }, 16), "x");
    bank.setResolution (1, 8);
    p = bank.getPattern (1);
    CHECK (p.reflections[0].tick == straightTick (3, 16));
    CHECK_FALSE (isOnGrid (p.reflections[0].tick, p.resolution, p.mode));
}

TEST_CASE ("Grid positions per mode; coincident positions count as straight", "[bank]")
{
    Pattern p;
    p.resolution = 8;
    p.lengthWholes = 1;
    p.mode = GridMode::Straight;
    CHECK (PatternBank::gridPositions (p).size() == 8);
    p.mode = GridMode::Triplet;
    CHECK (PatternBank::gridPositions (p).size() == 12);
    p.mode = GridMode::Combined;
    // 8 straight + 12 triplet - 4 coinciding (every quarter)
    CHECK (PatternBank::gridPositions (p).size() == 16);
    CHECK (PatternBank::gridPositions (p, SnapMode::Triplet).size() == 12);  // incl. the 4 on the beat
    CHECK (PatternBank::gridPositions (p, SnapMode::Straight).size() == 8);
}

TEST_CASE ("Randomization stays on the grid, respects density and amount, and is undoable", "[bank]")
{
    PatternBank bank;
    juce::Random rng (42);
    bank.clearPattern (0);
    bank.setResolution (0, 16);
    bank.randomizeAll (0, 0.5f, 1.0f, rng);
    auto p = bank.getPattern (0);
    const auto n = p.reflections.size();
    CHECK (n > 8);
    CHECK (n < 24);  // 32 positions at density 0.5
    for (auto& r : p.reflections)
        CHECK (isOnGrid (r.tick, 16, GridMode::Straight));

    const auto before = bank.getPattern (0).reflections;
    bank.randomizeVolume (0, 0.0f, rng);  // amount 0: nothing moves
    CHECK (bank.getPattern (0).reflections == before);
    bank.randomizePan (0, 1.0f, rng);
    CHECK (bank.getPattern (0).reflections.size() == n);
    CHECK (bank.getPattern (0).reflections != before);
    bank.undoEdit();
    bank.undoEdit();
    CHECK (bank.getPattern (0).reflections == before);
}

TEST_CASE ("Factory presets load", "[state]")
{
    ProgDelayProcessor p;
    for (const auto& fp : FactoryPresets::all())
    {
        p.restoreState (fp.state);
        INFO (fp.name);
        CHECK (StateSerializer::makeState (p.getApvts(), p.getBank()).isValid());
    }
    REQUIRE (FactoryPresets::all().size() >= 3);
    p.restoreState (FactoryPresets::all()[1].state);  // 7/8 Djent Stab
    CHECK (p.getBank().getPattern (0).sig == TimeSig { 7, 8 });
    CHECK (juce::roundToInt (p.getApvts().getRawParameterValue (pid::switchSource)->load()) == 1);
}

#if PD_MIDI_SWITCHING
TEST_CASE ("MIDI program change and notes C1-G1 select patterns", "[switching][midi]")
{
    ProgDelayProcessor proc;
    auto* src = proc.getApvts().getParameter (pid::switchSource);
    src->setValueNotifyingHost (src->convertTo0to1 (2.0f));  // MIDI
    proc.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buf (4, 256);
    buf.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (1, 2), 0);
    proc.processBlock (buf, midi);
    CHECK (proc.getEngine().playingPattern.load() == 2);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36 + 6, (juce::uint8) 100), 10);  // F#1 -> pattern 7
    proc.processBlock (buf, midi);
    CHECK (proc.getEngine().playingPattern.load() == 6);
}
#endif
