# Floozy

Floozy is a corrected VST3 port of `/home/danny/github/flues/lv2/floozy-poly`.
It is an 8-voice stereo instrument that combines Disyn-style distortion
sources with PM-Synth-style excitation, delay, feedback, filtering,
modulation, and reverb.

The port follows the normal Downspout split:

- portable core in `include/` and `src/`;
- deterministic tests in `tests/`;
- thin DPF wrapper and NanoVG UI in `src/dpf/`;
- migration notes in `docs/`.

## Build

Floozy is enabled by default with `DOWNSPOUT_BUILD_FLOOZY=ON` and builds as
`floozy.vst3` when DPF is available.

```bash
cmake -S ../.. -B ../../build -DDOWNSPOUT_BUILD_FLOOZY=ON
cmake --build ../../build --target floozy-vst3 downspout_floozy_core_tests
ctest --test-dir ../../build --output-on-failure -R floozy
```

## Status

This is intentionally a corrected continuation rather than a strict binary or
behavior clone. The LV2 version had unstable edges, so the Downspout core clamps
parameters, bounds audio output, uses deterministic voice/source behavior, and
tests the core before host integration.
