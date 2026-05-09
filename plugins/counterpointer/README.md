# counterpointer

`counterpointer` is an original transport-aware MIDI processor for generating a
monophonic counter-melody from the incoming MIDI pattern.

Current scope:

- capture incoming MIDI against the host transport cycle;
- generate a deterministic complementary melodic line from that captured pattern;
- expose controls for follow/counter movement, rhythm following, density,
  consonance, register, short randomness, and long cycle variation;
- keep the implementation portable before adding DPF/VST3 glue.

The input MIDI stream is the source pattern. There is intentionally no separate
source selector; this should behave like `cadence` and follow whatever upstream
MIDI generator or performer is routed into it.

Current status:

- portable core types, transport helpers, and engine exist;
- deterministic core tests exist;
- a first DPF-backed `counterpointer.vst3` wrapper and custom UI exist.
