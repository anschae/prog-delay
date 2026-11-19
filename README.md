# Prog Delay

A tempo-synced multi-tap delay for progressive rock and metal, made for music with frequent time-signature changes. You place every reflection exactly on a musical grid, with its own volume and pan, and switch between up to eight such *patterns* during a song.

Formats: **VST3** (macOS, Windows), **AU** (macOS), **Standalone**.

> **Status:** in development. See [Roadmap](#roadmap) for what is done.

## Download

All builds are made by GitHub Actions; you don't need any developer tools.

- **Releases (recommended once there is one):** open the [Releases page](https://github.com/anschae/prog-delay/releases) and download `ProgDelay-macOS.zip` or `ProgDelay-Windows.zip` from the newest release.
- **Latest development build:** open the [Actions tab](https://github.com/anschae/prog-delay/actions), click the newest run with a green tick, scroll down to **Artifacts** and download `ProgDelay-macOS.zip` or `ProgDelay-Windows.zip`. You need to be signed in to GitHub for this.

## Install

### macOS

1. Unzip. You get a folder `ProgDelay-macOS` containing `Prog Delay.vst3`, `Prog Delay.component` and `Prog Delay.app`.
2. Copy the plugins (in Finder, use **Go → Go to Folder…** and paste the path):
   - `Prog Delay.vst3` → `~/Library/Audio/Plug-Ins/VST3`
   - `Prog Delay.component` → `~/Library/Audio/Plug-Ins/Components`
3. The builds are only ad-hoc signed, not notarised by Apple, so macOS quarantines them after download (Gatekeeper then says the plugin "is damaged" or "can't be opened"). Remove the quarantine flag in **Terminal**:

   ```sh
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Prog Delay.vst3"
   xattr -cr ~/Library/Audio/Plug-Ins/Components/"Prog Delay.component"
   ```

   For the standalone app, do the same with the path to `Prog Delay.app`.

### Windows

1. Unzip. You get a folder `ProgDelay-Windows` containing `Prog Delay.vst3` (a folder) and `Prog Delay.exe`.
2. Copy the whole `Prog Delay.vst3` folder to `C:\Program Files\Common Files\VST3`.
3. `Prog Delay.exe` is the standalone version and can live anywhere. No Visual C++ redistributable is needed.

## Make your DAW find the plugin

- **Cubase:** *Studio → VST Plug-in Manager*, click the gear icon (bottom left) and then **Rescan All**. Prog Delay appears under the vendor *anschae*.
- **Logic Pro:** quit and reopen Logic; it validates new Audio Units on start. If Prog Delay doesn't show up, open *Logic Pro → Settings → Plug-in Manager*, select it and click **Reset & Rescan Selection**. It is listed under *Audio Units → anschae*.

## Making a release

Create a tag that starts with `v` (for example `v0.1.0`). GitHub Actions then builds everything, runs the checks and attaches `ProgDelay-macOS.zip` and `ProgDelay-Windows.zip` to the release.

- **On the GitHub website:** *Releases → Draft a new release → Choose a tag*, type e.g. `v0.1.0`, *Create new tag on publish*, then **Publish release**. The zips appear under the release's assets when the Actions run is finished.
- **With git:** `git tag v0.1.0 && git push origin v0.1.0`

To change the version number shown in the plugin, edit `PD_VERSION` at the top of `CMakeLists.txt` before tagging.

## Building it yourself

Needs CMake ≥ 3.24 and a C++20 compiler (Xcode on macOS, Visual Studio 2022 on Windows). JUCE and Catch2 are downloaded automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Roadmap

1. **Skeleton & CI** ← *done*
2. **Stage 1 DSP** (multi-tap, tempo sync, tap math tests) ← *done*
3. Volume and pan grids
4. Patterns: 8 slots, switching, state, undo/redo, randomisation
5. Stage 2 (feedback) and sound modes
6. Ducking
7. Polish: presets, tooltips, resizing, performance
8. Optional: MIDI pattern switching

## Licence note

JUCE is dual-licensed (AGPLv3 or a commercial JUCE licence). That matters only if you distribute the plugin to others.
