# gremlin

`gremlin` is the `downspout` VST3 port of the `flues` LV2 malfunction
instrument. It keeps the source engine as a portable core, wraps it in DPF, and
exposes a custom performance UI instead of relying on the original X11 panel.

## Current VST3 shape

- synth-style VST3 with stereo audio output
- one MIDI input that accepts both note performance and controller gestures
- MIDI output for Akai MIDImix button LED feedback
- custom UI with conventional fader blocks for scenes, actions, macros, focused live controls, and momentary holds
- six sound modes: Shard, Servo, Spray, Collapse, Ring, and Vapor
- no custom saved-state layer yet beyond normal host parameter persistence

## Practical test note

`gremlin` is a note-driven instrument, not an audio insert effect. Silence with
no MIDI input is expected.

For a first DAW sanity check:

- put it on an instrument track;
- send it ordinary MIDI notes;
- avoid using the very low controller-note range as the musical test range.

The lowest mapped MIDImix/controller note IDs are reserved for performance
button gestures rather than pitch, so a normal musical test should start above
that range.

## Important mapping decision

The LV2 plugin had separate `midi_in`, `controller_in`, and controller LED
feedback ports. The VST3 port intentionally collapses input into a single MIDI
path and exposes one MIDI output for controller LEDs. The processor
differentiates notes, CCs, and controller-button notes internally.

That keeps the host-facing wrapper simple enough for mainstream DAWs while
preserving the practical Akai MIDImix-style behavior.

## Akai MIDImix control surface

The intended hardware controller is the Akai MIDImix. It has eight channel
faders, one master fader, 24 knobs in three rows, and mute/solo/record-arm
buttons. Gremlin follows the original `flues` factory-style mapping:

- channel faders control the eight macro parameters
- the master fader controls `Master Trim`
- the top knob row controls source and damage parameters
- the middle knob row controls delay, stutter, tone, damping, space, and level
- the bottom knob row remains available as host parameters and for
  `gremlin-driver` randomisation, but is not shown on the simplified panel
- record-arm buttons select modes and scenes
- mute buttons act as momentary hold pads
- solo/mute combinations trigger actions such as reseed, burst, randomise, and
  panic

Gremlin sends MIDImix LED feedback from its VST3 MIDI output. Reaper needs the
track hosting Gremlin to route MIDI hardware output back to the MIDImix; see
[docs/reaper-midimix.md](docs/reaper-midimix.md) for a suggested track, send,
and receive layout.

## UI note

The UI is intentionally macro-first and fader-based. Deeper breakage parameters
remain exposed to the host and to `gremlin-driver`, but the panel keeps the
common performance surface focused on modes, scenes, actions, macros, source
controls, time/space controls, and hold pads.
