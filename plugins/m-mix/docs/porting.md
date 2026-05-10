# m-mix design notes

`m-mix` is not a direct LV2 port. It adapts the shared behavior of `p-mix` and
`e-mix` to a MIDI pipeline:

- `e-mix` supplies the deterministic Euclidean block mask over a transport
  cycle.
- `p-mix` supplies probabilistic open/closed transitions at bar-granularity
  boundaries.
- The final gate is `probabilistic_gain * euclidean_gain`.
- MIDI fades are implemented by scaling note-on velocity. Existing note
  velocity cannot be changed after note-on, so the processor emits note-offs
  for held notes when the gate closes instead of trying continuous per-note
  fades.
- Non-note MIDI events pass through unchanged.
- Stopped transport passes MIDI through so live input is not unexpectedly
  silenced.
- In VST3 hosts where BBT is unavailable but rolling frame position and tempo
  are available, the DPF wrapper derives bar position from frame time.
