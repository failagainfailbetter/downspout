# bassgen port

This directory holds the `downspout` port of `~/github/flues/lv2/bassgen`.

`bassgen` is a transport-aware MIDI bass generator with persistent pattern
state, loop-boundary variation, and a custom DPF UI. In `downspout`, the core
now stores normalized meter data and shapes rhythm around pulse starts,
pickups, and longer pulse-spanning notes rather than assuming every bar is
effectively `4/4`. It also now exposes explicit style modes:
`Auto`, `Straight`, `Reel`, `Waltz`, `Jig`, and `Slip Jig`.

The DPF wrapper accepts incoming MIDI as context. The `Follow/Dodge` control
can make bass notes more likely on incoming drum/bass hits or suppress bass
notes on those beats, with listen controls for MIDI channel and note focus.

Implementation status:

- portable core library exists and builds;
- deterministic tests exist and pass;
- host-neutral block engine exists for MIDI scheduling;
- text state serialization exists for controls, pattern, and variation;
- the DPF wrapper and custom UI build as `bassgen.vst3`;
- the core now handles shared-meter transport input and regenerates on meter
  changes;
- the core now has explicit style controls for simple, reel, waltz, jig, and
  slip-jig phrasing on top of the shared meter model;
- scale choices now include Lydian, Melodic Minor, Whole Tone, Altered,
  Half-Whole Diminished, Whole-Half Diminished, Bebop Dominant, Bebop Major,
  and Bebop Minor in addition to the original minor/major/modal/pentatonic/
  blues options. These were appended to the scale enum to preserve existing
  saved-state values;
- the appended Jazz genre biases rhythm toward walking beat anchors and maps
  four-bar phrases across ii-V-I-turnaround roots, with role-specific Dorian,
  Mixolydian/altered, and Major/Lydian color for ordinary scales. Dominant
  bars choose from Mixolydian, altered, half-whole diminished, whole-tone, and
  bebop dominant material, and beat-start notes target roots, thirds, fifths,
  and sevenths while explicitly selected Jazz color scales remain constrained
  to their chosen vocabulary;
- incoming MIDI follow/dodge controls are wired through the wrapper, core, and
  UI.

Reference docs:

- `docs/audit.md`
- `docs/extraction-plan.md`
- `docs/porting.md`
