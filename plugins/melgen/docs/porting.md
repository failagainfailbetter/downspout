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
second depending the `Cadence` control and phrase role.
