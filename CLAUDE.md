# CLAUDE.md – Prog Delay

Tempo-synced multi-tap delay (VST3/AU/Standalone, JUCE 8, C++20). Keep this file
up to date with every change: later sessions rely on it instead of re-reading the code.

## Status
- Milestone 1 (skeleton & CI) done. Pass-through processor, placeholder editor.

## Build
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=$PWD/.deps
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
- Options: `PD_BUILD_TESTS` (ON), `PD_BUILD_PLUGIN` (ON), `PD_BUILD_TOOLS` (OFF, snapshot renderer),
  `PD_MIDI_SWITCHING` (OFF; ON = MIDI input + AU type `aumf`).
- Linux builds work too (VST3 + Standalone + tests), handy for local checks; Release uses LTO, so
  for quick iteration use `-DCMAKE_BUILD_TYPE=RelWithDebInfo` in a separate build dir.
- All sources under `src/` are globbed into the INTERFACE library `PDShared` (except `PluginEntry.cpp`);
  tests link it too. New files need a re-configure (CONFIGURE_DEPENDS handles it with Ninja).

## Identity (never change after release)
Product name `Prog Delay`, target `ProgDelay`, manufacturer `anschae`, manufacturer code `Ansc`
(same as IRene), plugin code `PrDl`, bundle id `ch.anschae.progdelay`. All at the top of `CMakeLists.txt`.

## CI (`.github/workflows/build.yml`)
Jobs: linux (tests + pluginval under xvfb), macos (universal, tests, ad-hoc codesign, pluginval, `auval -strict -v aufx PrDl Ansc`),
windows (x64, static runtime check, tests, pluginval), package (collects `ProgDelay-macOS.zip` /
`ProgDelay-Windows.zip`; on `v*` tags creates a GitHub Release). JUCE/Catch2 downloads are cached in `.deps/`.
Pluginval strictness: `PLUGINVAL_STRICTNESS` env var.

## Known issues
- none yet
