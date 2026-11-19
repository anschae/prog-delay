# Prog Delay

A tempo-synced multi-tap delay for progressive rock and metal, made for music with frequent time-signature changes. You place every reflection exactly on a musical grid, with its own volume and pan, and switch between up to eight such *patterns* during a song.

Formats: **VST3** (macOS, Windows), **AU** (macOS), **Standalone**.

> **Status:** all milestones are implemented, including the optional MIDI switching (as a build option). The builds pass the automated checks (unit tests, pluginval at strictness 10, Apple's auval). The plugin has not yet been tried by a person in Cubase or Logic.

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

## Using it

- **Patterns:** eight slots (tabs). Each pattern has its own grid (1/4 … 1/32; straight, triplet or both), a length of 1–4 whole notes and, optionally, an assigned time signature. The pattern you edit and the one that plays can differ: the green LED marks the playing one.
- **Volume grid (top):** click an empty step to add a reflection (the height sets the volume), drag a bar for volume (Shift = fine), Alt/Option-drag to move it in time, double-click or right-click to delete, click-drag across empty steps to paint several. In *Combined* mode straight steps are cyan, triplets orange, and *Snap* chooses what a click lands on.
- **Pan grid (bottom):** drag a reflection up (left) or down (right); double-click centres it.
- **Dashed green line:** the end of the host's current bar (e.g. 7/8). **Solid line:** the pattern's own time signature.
- **Switching patterns** (header, *Pattern Switch*):
  - *Automation*: automate the parameter **Active Pattern** (1–8) in Cubase or Logic. No MIDI routing needed.
  - *Follow sig.*: the pattern whose assigned time signature matches the host's is played automatically (set it with *Pattern sig.*). *Else* chooses what happens when nothing matches.
  - *Q*: switch immediately, on the next beat or on the next bar. Echoes of notes played before a switch keep the old pattern.
- **Stage 2 (feedback):** repeats the whole pattern every bar (the host bar, or a manual one such as 7/8 or 13/16), darker or thinner each time with Low/High Cut.
- **Sound modes:** Digital, Tape, Analog (BBD), Lo-Fi, with Character, Age/Mod and Tone. They always colour the repeats; *Color reflections* colours stage 1 too.
- **Ducking:** lowers the delay while you play. The detector can listen to the input or to a sidechain (Cubase: activate the side-chain input in the plugin header; Logic: choose the side chain in the plugin window's top bar). *More* opens detector low-pass, knee, peak/RMS, full duck and tempo-synced release. *Look* (lookahead) adds a little latency, which the host compensates.
- **Presets:** click the preset name for factory and user presets, *Save preset…* and *Delete*. User presets are files in `~/Library/Application Support/anschae/Prog Delay/Presets` (macOS) or `%APPDATA%\anschae\Prog Delay\Presets` (Windows). **A/B** keeps two complete settings to compare.
- Double-click any knob to type a value. Cmd/Ctrl+Z undoes grid edits.

### MIDI pattern switching (optional build)

The normal build is a plain audio effect. A build with `-DPD_MIDI_SWITCHING=ON` also accepts MIDI: program change 0–7 or the notes C1–G1 select patterns 1–8 (with *Pattern Switch* set to *MIDI*).

- **Cubase:** insert it as usual and route a MIDI track's output to the plugin.
- **Logic:** this build is an "AU MIDI-controlled effect" (`aumf`), which Logic treats as a different plugin. Logic only sends MIDI to such effects when they sit in the **instrument slot** of a software instrument track; the audio to process is then chosen in the plugin window's side-chain menu. In that setup the external sidechain for ducking may not be available. This routing has not been tested yet; *Automation* switching works without any of this.

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
3. **Volume and pan grids** ← *done*
4. **Patterns**: 8 slots, switching, state, undo/redo, randomisation ← *done*
5. **Stage 2 (feedback) and sound modes** ← *done*
6. **Ducking** ← *done*
7. **Polish**: presets, A/B, tooltips, resizing, performance ← *done*
8. **Optional: MIDI pattern switching** (CMake option `PD_MIDI_SWITCHING`) ← *done*

## Licence note

JUCE is dual-licensed (AGPLv3 or a commercial JUCE licence). That matters only if you distribute the plugin to others.
