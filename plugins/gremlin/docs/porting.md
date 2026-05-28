# gremlin porting notes

## UI direction

The LV2 X11 UI mirrored the MIDImix layout closely. The VST3 UI keeps the same
performance ideas but uses a DAW-friendly panel with tall, narrow fader blocks:

- mode buttons
- scene buttons
- action buttons
- macro faders
- source faders
- time/space faders
- momentary hold pads

This is intentionally a functional reinterpretation, not a literal X11 port.

## MIDI behavior

- note input still plays the synth
- mapped controller CCs update live, hidden, macro, and master controls
- mapped controller notes trigger momentaries, scenes, actions, and mode steps
- the old LV2 controller-output LED path is not implemented in the VST3 wrapper

## Current known gap

There is no custom state serialization yet. Hosts will persist parameters, but
there is no extra compatibility layer for future parameter-layout changes.

There is also no VST3 MIDI output from Gremlin for Akai MIDImix LED feedback.
`gremlin-driver` still emits controller MIDI for driving Gremlin, but Gremlin
itself reports live controller state through output/status parameters and the
UI.
