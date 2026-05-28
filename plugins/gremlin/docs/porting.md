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
- the old LV2 controller-output LED path is represented as VST3 MIDI output
  for MIDImix button LEDs

## Current known gap

There is no custom state serialization yet. Hosts will persist parameters, but
there is no extra compatibility layer for future parameter-layout changes.

Gremlin also reports live controller state through output/status parameters so
the UI can mirror controller gestures. For Reaper MIDI output routing, see
[reaper-midimix.md](reaper-midimix.md).
