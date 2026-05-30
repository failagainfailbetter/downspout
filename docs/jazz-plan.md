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
- [ ] Prefer Dorian-style tones over ii, Mixolydian/altered tones over V, and
      Major/Lydian tones over I.
- [ ] Add dominant-bar color selection for altered, diminished, whole-tone,
      and bebop dominant material.
- [ ] Target chord tones explicitly on strong beats, especially roots, thirds,
      fifths, and sevenths.
- [ ] Add approach-tone and enclosure behavior around chord tones.
- [ ] Decide whether the UI needs a simple `Jazz Color` control after the core
      model proves useful.
- [ ] Consider mirroring useful Jazz scales into `melgen` and `counterpointer`
      after `bassgen` behavior is tested in a host.

## First implementation slice

Append the following scale choices to `bassgen`:

- Altered
- Half-Whole Dim
- Whole-Half Dim
- Bebop Dominant
- Bebop Major
- Bebop Minor

These scales are useful immediately as selectable pitch vocabularies. They will
become more musically meaningful once Jazz bars carry explicit chord roles and
dominant bars can choose altered/diminished colors intentionally.
