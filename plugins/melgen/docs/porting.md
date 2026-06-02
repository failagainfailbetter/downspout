# melgen design notes

`melgen` is not an LV2 port. It intentionally reuses the working `bassgen`
architecture where that architecture is host-facing rather than musical:

- transport snapshots;
- MIDI scheduling;
- loop-boundary variation;
- text state serialization;
- DPF VST3 wrapper and custom UI pattern.

The melody generator replaces `bassgen` note/rhythm vocabulary with a phrase
model. A pattern contains realized MIDI events plus phrase metadata so saved
state can preserve the musical form that produced the events.

The first implementation does not model chord progressions. Cadence behavior is
scale-degree based, with strong endings gravitating toward root, fifth, or
second depending the `Cadence` control and phrase role. `Color` is intentionally
not a chord-extension model yet; it increases melodic tension by making scalar
movement looser, answer phrases more mutable, and weak/pickup notes more likely
to use chromatic approaches or simple above/below enclosures.

`Follow` adds one MIDI input role to the generator. Incoming note-on events set
an active influence note; generated notes are pulled toward the nearest octave
of that note, capped below a full copy and quantized back to the selected scale.
Note-off and all-notes-off messages clear the influence. The control is applied
at playback/scheduling time, so changing it does not regenerate the saved
pattern and the extra work is skipped when `Follow` is zero.

UI decisions:

- The UI is not a direct `bassgen` reskin. It is split into a left `Line Shape`
  panel and a right `Phrase Structure` panel so stochastic controls and form
  controls do not compete for the same space.
- Selector labels are drawn inside their rows with a fixed value gutter. This
  avoids the label/box overlap that happened when labels floated above compact
  selector boxes.
- The default window height is `1040x760`, giving the full slider list enough
  room down to `Register` and `Seed` without relying on host-side resizing.
