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
- custom NanoVG UI exists;
- install script enables `melgen` by default.
