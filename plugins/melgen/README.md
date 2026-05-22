# melgen

`melgen` is a phrase-aware MIDI melody generator derived from the proven
`bassgen` transport, state, and DPF wrapper architecture.

The musical core is new:

- scale-degree melody generation rather than bassline generation;
- explicit phrase metadata in pattern state;
- period forms: free, `A A`, `A B`, `A A'`, call/answer, and `A B A`;
- contour control: wandering, flat, rising, falling, arch, inverted arch;
- answer transforms: related, same, transpose, invert, compress, expand;
- a `Structure` control that moves from stochastic line generation toward
  stricter phrase derivation and cadence behavior.

Current status:

- portable core exists;
- deterministic core tests exist;
- DPF/VST3 MIDI-output wrapper exists;
- custom NanoVG UI exists, with `Line Shape` controls separated from
  `Phrase Structure` selectors and action buttons;
- install script enables `melgen` by default.

UI notes:

- `Line Shape` contains root, length, phrase length, density, structure,
  range, leap, rest, cadence, hold, accent, register, and seed sliders.
- `Phrase Structure` contains scale, period, contour, answer, grid, and MIDI
  channel selectors plus `New`, `Notes`, and `Rhythm` action buttons.
- The default UI height is intentionally taller than `bassgen` because the
  melody surface has more shaping controls.
