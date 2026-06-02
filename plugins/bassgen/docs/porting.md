# bassgen porting notes

Source plugin: `~/github/flues/lv2/bassgen`

Notable source concerns to preserve:

- MIDI note generation
- host transport sync
- rewind-aware playback reset
- persisted pattern state
- variation behavior across loop boundaries

Current `downspout`-specific port note:

- the portable core now persists a normalized `Meter` in pattern state and
  derives pulse accents, pickups, and longer landmark note lengths from it, so
  `bassgen` no longer treats every bar like a flat quarter-beat grid even
  though its genre vocabulary is still broad rather than folk-specific;
- the wrapper now exposes explicit `Style` modes for `Auto`, `Straight`,
  `Reel`, `Waltz`, `Jig`, and `Slip Jig`, and those modes are implemented in
  the portable pattern generator rather than only as UI labels;
- incoming MIDI influence is modeled in the portable engine. DPF only enables
  MIDI input and adapts host `MidiEvent`s into core `InputMidiEvent`s. The
  response controls listen to one MIDI channel/note, defaulting to channel 10
  note 36, and use a bipolar follow/dodge amount to either suppress matching
  bass onsets or inject a short bass note on matching input hits;
- Jazz is implemented as an appended genre value for state compatibility. It
  adds ii-V-I-turnaround roots, walking beat anchors, explicit chord-role
  targets, dominant color choices, and chromatic approach/enclosure behavior.
  Jazz color scales were appended after the existing scale IDs: Altered,
  Half-Whole Diminished, Whole-Half Diminished, Bebop Dominant, Bebop Major,
  and Bebop Minor. Explicitly selected Jazz scales remain constrained
  vocabularies, while ordinary scales use the role model for Dorian,
  Mixolydian/altered, and Major/Lydian color;
- the appended `Color` control is serialized in text state and exposed through
  DPF/UI. It is intentionally general rather than Jazz-only: Jazz uses it for
  dominant color intensity, and other genres use it to increase
  vocabulary-appropriate tension or motion without changing existing enum IDs.

Likely reusable source modules:

- pattern generation
- variation logic
- transport interpretation
- state serialization
