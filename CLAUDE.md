# CLAUDE.md – Prog Delay

Tempo-synced multi-tap delay (VST3/AU/Standalone, JUCE 8.0.15, C++20, Catch2 v3). Keep this file
up to date with every change: later sessions rely on it instead of re-reading the code.

## Status
- M1 skeleton & CI: done (CI green on Linux/macOS/Windows).
- M2 stage 1 DSP: done. Engine with multi-tap stage, tempo sync/manual, pattern switcher, state
  serializer, pattern bank model.
- M4 patterns: done (8 slots, automation / follow-sig switching, quantize, tail-preserving switching,
  state round trip, undo/redo, randomization; tests in tests/PatternTests.cpp).
- M5 stage 2 + sound modes: done (FeedbackStage, ModeHost crossfade, Color reflections; tests in
  tests/FeedbackTests.cpp).
- M6 ducking: done (Ducker + DuckingDisplay, sidechain bus, lookahead latency; tests in tests/DuckerTests.cpp).
- M7 polish: done (PresetManager + PresetBar with factory/user presets and A/B, tooltips, repeats
  preview, pluginval strictness 10 in CI). Perf (Linux, RelWithDebInfo, 48 kHz stereo, fb + tape +
  colour + ducking): 16 taps 1.5 %, 64 taps 3.5 %, 256 taps 10 % of one core (`ProgDelayTests "[.perf]"`).
- M8 MIDI switching: done behind `PD_MIDI_SWITCHING` (processor parses program change 0-7 and notes
  36-43 = C1-G1; test in PatternTests.cpp under `#if PD_MIDI_SWITCHING`). UI disables the MIDI
  button in the default build.
- M3 grids: done. Full editor layout (header, tabs, toolbar, volume/pan grids, randomize panel, bottom
  sections). Sound mode classes are registered (DSP used from M5 on).

## Build
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=$PWD/.deps
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
- Options: `PD_BUILD_TESTS` (ON), `PD_BUILD_PLUGIN` (ON), `PD_BUILD_TOOLS` (OFF, snapshot renderer),
  `PD_MIDI_SWITCHING` (OFF; ON = MIDI input + AU type `aumf`).
- Linux builds work (VST3 + Standalone + tests). Release uses LTO (slow links); for iteration use a
  separate dir: `cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCHCONTENT_BASE_DIR=$PWD/.deps`.
  Test binary: `build-dev/ProgDelayTests_artefacts/RelWithDebInfo/ProgDelayTests`.
- All sources under `src/` are globbed into the INTERFACE library `PDShared` (except `PluginEntry.cpp`);
  tests link it too. New files are picked up on the next build (CONFIGURE_DEPENDS).

## Identity (never change after release)
Product name `Prog Delay`, target `ProgDelay`, manufacturer `anschae`, manufacturer code `Ansc`
(same as IRene), plugin code `PrDl`, bundle id `ch.anschae.progdelay`. All at the top of `CMakeLists.txt`.

## File layout
```
src/PluginProcessor.*   APVTS, PatternBank, Engine; ValueTree listener publishes snapshots; state I/O
src/Parameters.*        parameter IDs (namespace pid), choice enums, layout
src/dsp/Engine.*        the complete per-block DSP (testable without the processor)
src/dsp/MultiTapStage.* stage 1 (taps, tail-preserving switch segments, edit crossfade)
src/dsp/DelayLine.h     pow2 ring buffer, absolute int64 time, Hermite reads
src/dsp/TempoClock.h    HostTiming (from AudioPlayHead), TempoClock (fallbacks), DelayTimeXfade
src/dsp/PatternSwitcher.h  which pattern + when (quantize)
src/dsp/modes/          SoundMode interface, SoundModeRegistry, modes
src/model/Timing.h      tick maths (TICKS_PER_WHOLE = 6720), pan law
src/model/Pattern.*     Reflection/Pattern structs, ValueTree IDs (namespace ids), labels
src/model/PatternBank.* undoable editing operations on the PATTERNS tree, snapshot building
src/model/BankSnapshot.h audio-thread pattern data + TripleBuffer
src/model/StateSerializer.* state format
src/model/FactoryPresets.* built-in presets as full state trees
src/ui/                 PluginEditor's MainView children (see UI below)
tests/                  Catch2 (own main with ScopedJuceInitialiser_GUI)
tools/Snapshot.cpp      renders the editor to PNG (`xvfb-run -a .../ProgDelaySnapshot /tmp/snap [preset]`)
```

## UI
- `MainView` is laid out at a fixed design size 1280×820; `ProgDelayEditor` scales it with an
  AffineTransform (resizable 75–150 %, fixed aspect ratio). All coordinates in `MainView::resized()`.
- `GridModel` (ui/GridModel.*) is shared by both grids: cached edited `Pattern`, selection, geometry
  (`tickToX` with gutters `kGutterLeft/Right`, so both grids' columns line up), live info (host sig,
  flashes). Its `onChange` repaints tabs/toolbar/grids.
- `VolumeGrid` interactions: click empty → add (volume from y) and enter paint mode (drag across
  empty positions paints, staying in a column sets its volume); drag a bar → relative volume
  (Shift fine); Alt-drag → move in time (snaps, skips occupied); double/right-click → delete;
  Delete key → delete selection. `PanGrid`: drag existing only, double-click centres.
- Bar markers: host signature dashed green (`GridBase::drawVerticalLines`), pattern signature solid
  in the pattern colour. Off-grid reflections are drawn outlined.
- Flash: engine `tapLevels[i]` (peak of tap i of the playing pattern, index = sorted tick order) →
  `GridPanel::timerCallback` decays them into `GridModel::flash`.
- Widgets: `Knob` (param or callback, double-click = type value), `Segmented` (choice param or
  callback), `ChipToggle`, `SwitchToggle`. Look in `ui/LookAndFeel.*`, colours in `ui/Theme.h`.
- Buttons can carry an "accent" property (colour string) used by the LookAndFeel when toggled on.
- Non-ASCII UI strings must be written as `juce::CharPointer_UTF8 ("\x..")` (MSVC + JUCE assert).

## Core concepts
- **Ticks:** every position is an int tick of a whole note, `TICKS_PER_WHOLE = 6720` (divisible for
  straight down to 1/64, triplets, quintuplets, septuplets, dotted 1/32). Straight step k at 1/r =
  `k*6720/r`; triplet step = `k*4480/r`. Coinciding positions count as straight (`classifyTick`).
  BPM counts quarters: whole note = 240/BPM s.
- **Stage 1 input is mono** (`0.5*(L+R)`), each tap is panned with constant power (-3 dB centre).
- **Lock-free pattern hand-over:** UI edits the PATTERNS ValueTree → processor listener →
  `PatternBank::fillSnapshot` into `TripleBuffer<BankSnapshot>::writeBuffer()` → `publish()`
  (writer side guarded by a SpinLock, never touched by the audio thread). Engine `acquire()`s at
  block start (only when no edit crossfade is running) and copies into one of two `BankSnapshot`s;
  stage 1 crossfades old→new snapshot over 10 ms.
- **Tail-preserving switching** (`MultiTapStage`): list of segments `{start (input time), pattern}`,
  max 8. A tap of pattern P weights the input sample at time m by
  `r(m - start_k) - r(m - start_{k+1})`, r = 5 ms linear ramp centred on the boundary. Segments are
  pruned once `now - maxDelay(P) > next.start + ramp`. Switch time = `stage1.now() + offset`.
- **Tempo changes:** `DelayTimeXfade` crossfades old/new read positions over 20 ms (no pitch glide);
  glide mode (tape) slides the value instead.
- **Chunks:** the engine processes in chunks of ≤ 64 samples (any host block size works).
- **Mix law:** dry amount `min(1, 2(1-mix))`, wet `min(1, 2 mix)` (dry stays at unity up to 50 %).
- Reflections beyond the grid length stay in the data (non-destructive) but are not played/drawn.
- **Stage 2** (`FeedbackStage`): stereo buffer holds `s1 + s2`; per sample: read at `t - B - mod`
  (B crossfaded via DelayTimeXfade, glide if the mode `prefersGlide()`; `mod` = mode's
  `getDelayModulationSamples`, sampled once per chunk and interpolated) → ping-pong swap / mono →
  × feedback → low cut / high cut (TPT SVF 12 dB/oct) → `ModeHost` → `softClip` → s2; write s1+s2.
  B = feedback bar (host signature or manual num/den) × samples per whole, clamped to
  [72 samples, 32 s] (spec allows 16 wholes at 30 BPM = 128 s; capped to keep memory sane).
- **Sound modes:** `ModeHost` owns one prepared instance of every registered mode (two hosts: the
  loop and "Color reflections" on stage 1), switching crossfades over 30 ms. Mode parameter index
  = registry index; the state stores the mode ID too.
  **Adding a mode:** write `class FooMode : public SoundMode` in `src/dsp/modes/FooMode.h`
  (getId stable lower-case, getDisplayName, prepare/reset/process, optional
  getDelayModulationSamples / prefersGlide) and append `add<FooMode>();` in
  `SoundModeRegistry.cpp`. The parameter choice list and the UI buttons are built from the registry.
  Never reorder or remove entries (old projects store the index). Macros: character/age 0..1,
  tone -1..1 (param ±6 dB / 6).
- Modes: Digital (none), Tape (asym. tanh saturation, 2×SVF HF loss, wow 0.55 Hz + flutter 6.3 Hz
  + drift, signal-gated hiss, glide), Analog/BBD (pre/post LP, S&H clock 46→16 kHz, compander with
  fast/slow envelope mismatch, triangle chorus 90° apart, gated noise), Lo-Fi (HP/LP band-limit,
  S&H 26→5 kHz with age jitter, 15→5 bits, wobble).

- **Ducking** (`dsp/Ducker.*`): `DuckDetector` (HP/LP SVF on the detector only, stereo-linked
  peak envelope with 20 ms decay, or RMS = one-pole mean square over the window) → `Ducker`
  gain computer `targetReductionDb` = depth × smoothstep through the knee (binary at knee 0;
  full duck = -80 dB floor → gain 0) → ballistics in dB (attack one-pole; hold counter reset
  while reduction increases; release one-pole; tempo-synced release resolved in the engine from
  `kRelNoteWholes`, last entry = one host bar) → linear gain per sample.
  Application: `DuckApplier` interface, `BroadbandApplier` multiplies. **Band-limited ducking
  later:** add a `BandApplier` (crossover split, gain on one band, sum) and use it for
  `duckS1/duckS2` in `Engine`; detector and gain computer stay unchanged.
  Engine wiring: detector sees the *undelayed* input (or the sidechain bus); everything else
  (dry too) goes through the lookahead delay (`Engine::lookaheadSamplesFor`, ≤ 10 ms). The
  processor reports it with `setLatencySamples` in `prepareToPlay` and from a 10 Hz Timer on the
  message thread (never from the audio thread). Post = gains on the s1/s2 output contributions
  (the loop is not affected); Pre + S1/Both = gain on the mono stage-1 input; Pre + S2 = gain on
  the s1 feed into the loop (`FeedbackStage::process` feedGain).
  Display: `Ducker` pushes one {levelDb, grDb} frame per 10 ms into an `AbstractFifo`;
  `ui/DuckingDisplay` reads it at 30 Hz (4 s history). The FIFO drops frames when nobody reads.
- Sidechain: third bus "Sidechain" (stereo, disabled by default) in the processor; used when
  Duck Source = Sidechain and the bus is enabled, else the input is the detector.

## Parameters (APVTS, version hint 1)
`mix dry wet killDry width feedback fbBarMode fbBarNum fbBarDen s1Level s2Level lowCut highCut
stereoMode soundMode character age tone colorRefl duckOn duckSource duckHP duckLP duckDetMode
duckRms duckThresh duckDepth duckFull duckKnee duckAttack duckHold duckRelease duckRelSync
duckRelNote duckLook duckTarget duckPos tempoMode manualBpm activePattern switchSource switchQuant
fallbackPattern`. Never rename IDs. Choice enums in `Parameters.h`.

## Presets / A-B
- `model/PresetManager.*` (owned by the processor, so A/B survives closing the editor): factory
  presets from `FactoryPresets::all()` (built in code), user presets = XML state files `*.pdpreset`
  in `userApplicationDataDirectory/anschae/Prog Delay/Presets` (tests use `setUserFolderOverride`).
  Current preset name ("Factory / X", "User / Y") lives in the processor and in the state
  (`presetName` property on the root). A/B = two captured state trees; switching captures the
  active one and restores the other (B starts as a copy). Loading a preset or A/B clears pattern
  undo history (PatternBank::replaceAll).
- Factory presets: Init, 7/8 Djent Stab, Polyrhythm 3 over 4, Dotted Ambience, Gallop Slap,
  Odd Meter Switcher, Lo-Fi Ghosts. Params not listed in a preset fall back to defaults.

## State format
See `StateSerializer.h`: root `ProgDelayState` (stateVersion, soundModeId) with the APVTS child
`PARAMS` and the `PATTERNS` tree. Binary XML. The sound mode is restored by its ID.

## CI (`.github/workflows/build.yml`)
Jobs: linux (tests + pluginval under xvfb), macos (universal, tests, ad-hoc codesign, pluginval,
`auval -strict -v aufx PrDl Ansc`), windows (x64, static runtime check, tests, pluginval), package
(collects `ProgDelay-macOS.zip` / `ProgDelay-Windows.zip`; on `v*` tags creates a GitHub Release).
Dependency downloads cached in `.deps/`. Pluginval strictness: `PLUGINVAL_STRICTNESS` env var.

## Known issues / decisions
- JUCE 8.0.15 as the spec asks (IRene uses JUCE 9).
- Not yet tested by a person in Cubase / Logic; only pluginval, auval and unit tests.
- Stage-2 bar length capped at 32 s (memory); longer manual bars at slow tempos are clamped.
- Stage 1 sums the input to mono before the taps (each tap is panned); stereo width afterwards.
- `setStateInformation` touches the PATTERNS ValueTree; hosts normally call it on the message
  thread (ValueTree is not thread-safe).
- Latency changes (lookahead) are reported from a 10 Hz message-thread timer.
- Follow sig. matching is exact (num and den): 6/8 does not match 3/4.
- Quantize Beat/Bar when the transport is stopped switches immediately.
- Snap "Trip." in Combined mode includes triplet steps that coincide with beats (created as straight).
- Peak detector = instant attack, 20 ms decay envelope; RMS = one-pole mean square (approximate window).
- The MIDI build's Logic routing (instrument slot, side-chain audio) is untested.
- System fonts (no embedded font), so text widths differ slightly between platforms.

## Backlog hooks (from the spec, not implemented)
Per-reflection extras (filters, feedback send, probability, micro offset, reverse, pitch): extend
`Reflection` + `TapData` and the tap loop in `MultiTapStage::process`. Euclidean fill / dotted /
quintuplet / septuplet grids: `GridMode` + `PatternBank::gridPositions` (ticks already divisible).
Per-pattern feedback: add fields to `Pattern`/`PatternAudio`, read in `Engine::process`.
Freeze: feedback 1 + bypass softclip/filters + stop writing s1 in `FeedbackStage`. Band-limited
ducking: see Ducking above. Spillover on bypass, MIDI learn.
