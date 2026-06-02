# Jazz generation plan

This plan tracks the next steps for making the generator plugins handle Jazz
more musically. The current focus is `bassgen`; other melodic generators can
reuse the same ideas after the bass behavior is stable.

## Goals

- Keep existing saved-state values stable by appending enum values only.
- Preserve the current simple Jazz mode while adding more harmonic awareness.
- Prefer explicit musical models over longer selector lists that do not affect
  generation behavior.
- Keep changes in plugin-local core/UI/tests unless shared behavior is approved
  separately.

## Checklist

- [x] Add a Jazz genre to `bassgen` and `drumgen`.
- [x] Add basic ii-V-I-turnaround roots to `bassgen` Jazz generation.
- [x] Append Jazz color scales to `bassgen` without changing existing scale IDs.
- [x] Add tests pinning appended scale IDs and generated pitch membership.
- [x] Introduce an internal Jazz chord-role model for ii, V, I, and turnaround
      bars.
- [x] Prefer Dorian-style tones over ii, Mixolydian/altered tones over V, and
      Major/Lydian tones over I.
- [x] Add dominant-bar color selection for altered, diminished, whole-tone,
      and bebop dominant material.
- [x] Target chord tones explicitly on strong beats, especially roots, thirds,
      fifths, and sevenths.
- [x] Add approach-tone and enclosure behavior around chord tones.
- [x] Add a general `Color` control after the core model proved useful.
- [x] Mirror useful Jazz scales into the other plugins that already expose
      scale selectors.
- [x] Make Cadence's custom UI expose the expanded scale list explicitly.
- [x] Mirror the general `Color` idea into `melgen` as melodic tension,
      chromatic approach, and enclosure behavior.
- [x] Mirror `Color` into `cadence` as learned-harmony tension, ii-V-I bias,
      seventh preference, and dominant/diminished color.
- [x] Mirror `Color` into `counterpointer` as adventurous counterline motion,
      reduced strict consonance, and chromatic approach behavior.
- [x] Mirror `Color` into `ground` as long-form bass tension, phrase-motion
      bias, and occasional chromatic pickup behavior.
- [x] Expand Cadence chord slots and add an `Extended` chord-size mode for
      generated 9th, 11th, and 13th voicings.
- [x] Add Cadence phrasing controls for continuous voicing `Spread` and
      `Arpeggio`, thinning playback from solid chords toward broken fragments
      and rotating single-note chord tones.

## First implementation slice

Append the following scale choices to `bassgen`:

- Altered
- Half-Whole Dim
- Whole-Half Dim
- Bebop Dominant
- Bebop Major
- Bebop Minor

These scales are useful immediately as selectable pitch vocabularies. Jazz bars
now carry explicit chord roles, and dominant bars can choose altered,
diminished, whole-tone, or bebop dominant color internally. Beat starts now
prefer root, third, fifth, and seventh targets while late pickup notes handle
motion into the next bar. One- and two-subdivision pickups now use chromatic
approaches and simple above/below enclosures into the next chord-tone target.
The UI now exposes a general `Color` control. In Jazz it controls dominant
color intensity, from inside Mixolydian/bebop behavior toward altered,
diminished, and whole-tone tension. In other genres it gently increases
non-root movement, blues/tritone tension, or wider modal motion according to
the existing genre vocabulary.

## Next Steps

- Validate `bassgen` Jazz and `Color` behavior in a DAW across several seeds,
  meters, and installed/reloaded plugin states.
- Validate `melgen` Color behavior in a DAW with conservative and high settings
  across inside scales, bebop scales, and altered/diminished scales.
- Validate `cadence` Color behavior in a DAW using learned cycles with clear,
  sparse, and ambiguous input material.
- Validate Cadence `Extended` chord mode in a DAW, especially whether dense
  six-note voicings remain useful across register, spread, comp, and arpeggio
  settings.
- Validate `counterpointer` and `ground` Color behavior in a DAW, especially
  that default zero preserves the current feel while high values remain useful
  rather than simply busier.
- Decide whether Jazz behavior should remain automatic or gain a second
  advanced control for dominant-color range after real host use.
- Add audio/MIDI example fixtures once the preferred Jazz behavior is stable
  enough to compare generated output by ear and by deterministic event tests.
