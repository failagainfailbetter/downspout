# Floozy source audit

Source plugin: `/home/danny/github/flues/lv2/floozy-poly`

Floozy Poly was a hybrid 8-voice instrument built from two earlier experiment
families:

- Disyn distortion oscillator algorithms;
- PM-Synth physical-model-style envelope, interface, delay, feedback, filter,
  modulation, and reverb modules.

The LV2 plugin exposed one MIDI input and one mono audio output. The Downspout
port deliberately moves to stereo output while keeping one primary MIDI input.

## Porting decisions

- Use `Floozy` as the public plugin name and `floozy.vst3` as the bundle.
- Keep copied first-party `flues` helper modules plugin-local under
  `plugins/floozy/include/flues/` so the build does not depend on the external
  flues checkout.
- Use deterministic random seeds for repeatable tests and host behavior.
- Clamp parameters at the core boundary and soft-limit audio output.
- Preserve the 23 LV2-derived controls, with integer enum metadata for source
  algorithm and interface type.
- Treat the port as corrected behavior, not a strict clone, because the source
  was known to be unreliable.
