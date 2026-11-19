# CLAUDE.md – Prog Delay

Tempo-synced multi-tap delay (VST3/AU/Standalone, JUCE 8.0.15, C++20, Catch2 v3). Keep this file
up to date with every change: later sessions rely on it instead of re-reading the code.

## Status
- M1 skeleton & CI: done (CI green on Linux/macOS/Windows).
- M2 stage 1 DSP: done. Engine with multi-tap stage, tempo sync/manual, pattern switcher, state
  serializer, pattern bank model. Editor is still a placeholder.

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
tests/                  Catch2 (own main with ScopedJuceInitialiser_GUI)
```

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

## Parameters (APVTS, version hint 1)
`mix dry wet killDry width feedback fbBarMode fbBarNum fbBarDen s1Level s2Level lowCut highCut
stereoMode soundMode character age tone colorRefl duckOn duckSource duckHP duckLP duckDetMode
duckRms duckThresh duckDepth duckFull duckKnee duckAttack duckHold duckRelease duckRelSync
duckRelNote duckLook duckTarget duckPos tempoMode manualBpm activePattern switchSource switchQuant
fallbackPattern`. Never rename IDs. Choice enums in `Parameters.h`.

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
