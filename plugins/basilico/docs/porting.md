# Basilico implementation notes

Basilico is an original Downspout instrument rather than a port from `flues`.

## Model contract

The public `Model` parameter selects a profile over one engine:

- `Upright`: triangle-like tone, body emphasis, low drive.
- `Electric`: balanced oscillator/sub/body path.
- `Dub`: muted low-frequency focus with rolled highs.
- `Acid`: resonant filter envelope and stronger drive.
- `Industrial`: hard drive/fold behavior with more bite.

The model can force a waveform or drive type internally, but the host-facing
parameter table remains stable.

## MIDI behavior

Basilico is monophonic with last-note priority. Overlapping notes glide when
the `Glide` parameter is active. Velocity affects output level, punch, and
filter accent.

## Tests

`downspout_basilico_core_tests` covers:

- defaults and clamping;
- all models rendering;
- note-off release;
- MIDI pitch tracking;
- glide behavior;
- velocity accent behavior;
- output boost and output bounding.
