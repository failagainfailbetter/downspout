# Fugue generation plan

This plan tracks the first steps toward Bach-like contrapuntal behavior in the
plugins that can use it musically.

## Goals

- Append enum values only, preserving existing saved-state IDs.
- Treat `Fugue` as subject/answer/episode/cadence behavior, not just another
  scale preset.
- Keep drum behavior deliberately sparse when a drum plugin exposes the genre.
- Prefer portable core changes with focused deterministic tests.

## Checklist

- [x] Add appended `Fugue` genre values to `bassgen` and `drumgen`.
- [x] Add `Fugue` to the BassGen and DrumGen custom UIs.
- [x] Give BassGen a first subject/answer model with tonic subject, dominant
      answer, episode, and tonic pedal/cadence behavior.
- [x] Give DrumGen a sparse metrical Fugue pulse that removes normal fills,
      crashes, and kit excess.
- [x] Add deterministic tests for appended genre IDs and core Fugue behavior.
- [x] Add subject-answer behavior to `counterpointer` using existing strict
      counterpoint controls instead of a new genre selector.

## First Implementation Slice

`bassgen` now treats Fugue as a continuo-like bass mode. It reinforces flowing
subject steps, starts the subject on tonic, answers on dominant, uses a short
episode bar, and resolves into a tonic pedal/cadence bar. `Color` can add
leading-tone pickup behavior without changing the selected scale enum.

`drumgen` exposes the same appended genre for UI consistency, but does not try
to make a drum style out of Bach. Its Fugue output is a sparse pulse: downbeat
kick, light closed-hat beat markers, and optional quiet clave on the middle
beat.

`counterpointer` now treats high `Regularity`, high `Counter`, and low
randomness as a strict fugal answering region. In that state the incoming MIDI
cycle is the subject, and the generated line preserves subject intervals around
the dominant. High color on Jazz-capable scales can invert those intervals.

## Next Steps

- Validate BassGen Fugue in a DAW with major, minor, and harmonic-minor scales.
- Validate Counterpointer's strict fugal answering in a DAW, especially with
  BassGen Fugue driving its input.
- Add Fugue-friendly phrase behavior to `melgen`, probably through existing
  `Period`, `Answer`, `Structure`, and `Color` controls rather than a new genre
  enum.
- Add longer-form subject-entry and pedal-planning behavior to `ground`.
- Consider Cadence support for circle-of-fifths and suspension-heavy harmonic
  support after the melodic subject behavior is stable.
