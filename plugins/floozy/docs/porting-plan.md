# Port Floozy to Downspout VST3

## Summary

Port `/home/danny/github/flues/lv2/floozy-poly` into Downspout as **Floozy**, a corrected, robust VST3 synth rather than an exact LV2 clone.

Floozy follows existing Downspout patterns: portable C++ core, deterministic tests, thin DPF wrapper, NanoVG UI, screenshot support, install/release packaging, and GitHub Pages documentation.

## Key Changes

- Add new plugin target `floozy`:
  - build option: `DOWNSPOUT_BUILD_FLOOZY`, default `ON`
  - bundle: `floozy.vst3`
  - plugin name: `Floozy`
  - metadata: creator `danja`, group/brand `Downspout`, homepage `https://danja.github.io/downspout/`
  - VST3 category: `Instrument|Synth`
  - MIDI input, no audio input, stereo audio output
- Keep the LV2 control surface as the initial public parameter contract:
  - source: `sourceAlgorithm`, `sourceParam1`, `sourceParam2`, `sourceLevel`, `noiseLevel`, `dcLevel`
  - envelope/interface: `attack`, `release`, `interfaceType`, `interfaceIntensity`
  - delays/feedback: `tuning`, `ratio`, `delay1Feedback`, `delay2Feedback`, `filterFeedback`
  - filter/mod/reverb/output: `filterFrequency`, `filterQ`, `filterShape`, `lfoFrequency`, `modulationTypeLevel`, `reverbSize`, `reverbLevel`, `masterGain`
- Use LV2 ranges/defaults as traceability anchors, but fix broken behavior where needed:
  - correct Disyn oscillator calling/mapping
  - deterministic RNG seed path for tests and repeatable initialization
  - velocity scales note amplitude
  - note-on velocity zero is note-off
  - all-notes-off/all-sounds-off clears voices and reverb safely
  - output remains finite and bounded under extreme feedback/modulation settings
- Make stereo real in the core from the first port:
  - expose `processStereo()`
  - pan voices deterministically across the stereo field
  - make shared reverb stereo/decorrelated enough that the two outputs are not just duplicated mono
  - do not add new public stereo-width parameters in the first pass

## Implementation Plan

- Create `plugins/floozy/` with the existing Downspout layout:
  - plugin-local `include/`, `src/`, `src/dpf/`, `tests/`, `docs/`
  - static core library `downspout_floozy_core`
  - optional DPF target `floozy` using `${DOWNSPOUT_DPF_PLUGIN_TARGETS}`
- Port/adapt only the needed first-party `flues` logic into the Floozy plugin-local core:
  - `FloozyEngine`/voice/source concepts
  - needed PM Synth modules
  - needed Disyn primitive algorithms used by Floozy's 0-6 source modes
  - use `downspout::floozy` namespace and avoid dependencies on `/home/danny/github/flues`
- Build the corrected portable core around:
  - `FloozyParams` with `ParamSpec` table
  - `FloozyVoice`
  - `FloozyEngine`
  - `MidiMessage`
  - `StereoFrame`
  - explicit reset/sample-rate methods
- Add DPF wrapper:
  - maps parameter table to DPF parameters
  - handles MIDI block scheduling with per-frame event timing
  - outputs stereo frames
  - implements correct metadata/category/unique ID
- Add NanoVG UI:
  - grouped layout based on LV2 rows: Source, Interface/Envelope, Delay/Feedback, Filter/Mod/Reverb, Output
  - enum selectors for source algorithm and interface type
  - compact enough for screenshot automation
- Update project integration:
  - root `CMakeLists.txt`
  - `install.sh`
  - `scripts/package-release.sh`
  - release/install docs
  - root README plugin table
  - GitHub Pages `_products/floozy.md`
  - screenshot capture plugin list

## Test Plan

- Core unit tests:
  - parameter defaults, ranges, clamping, enum rounding
  - all seven source algorithms produce finite non-silent output at normal settings
  - attack/release envelope behavior
  - note-on/note-off lifecycle and velocity scaling
  - repeated note retrigger behavior
  - 8-voice polyphony and voice stealing
  - all-notes-off/all-sounds-off clears active voices and reverb tail
  - sample-rate reset/reinitialization
  - stereo output is finite and not hard-identical under normal polyphonic render
  - stress render at extreme feedback/filter/reverb/modulation values stays finite and bounded
- Integration/build tests:
  - `downspout_floozy_core_tests`
  - `floozy-dsp`, `floozy-ui`, and `floozy-vst3` build
  - full `ctest --test-dir build --output-on-failure`
  - `./install.sh` includes `floozy.vst3`
  - release package script expects and packages `floozy.vst3`
  - screenshot script can build/capture `floozy`
- Manual host validation checklist:
  - load in DAW as instrument
  - MIDI note input produces stereo audio
  - automation updates all parameters without glitching/crashing
  - saved project restores parameter values
  - panic/all-notes-off behavior works

## Assumptions

- Floozy is a corrected continuation of `floozy-poly`, not a strict compatibility port.
- The first public parameter set remains traceable to the LV2 controls; no extra width/macro/preset parameters are added in v1.
- The Downspout repo is self-contained after the port; it must not include paths into `/home/danny/github/flues` at build time.
