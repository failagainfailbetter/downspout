# Gremlin Reaper MIDImix setup

Gremlin can send Akai MIDImix LED feedback from the VST3 MIDI output. The
controller still sends its normal factory-style note and CC messages into
Gremlin; Gremlin sends note messages back to the MIDImix for button LEDs.

The Akai MIDImix user guide describes the hardware as eight channel faders, one
master fader, 24 knobs in three rows, and mute/solo/record-arm buttons. Akai's
editor can also remap controls to CCs, notes, and MIDI channels. This Gremlin
mapping assumes the factory-style identifiers already used by the original
`flues` LV2 version.

## Reaper device setup

1. Open `Options > Preferences > MIDI Devices`.
2. Enable the MIDImix input for MIDI input.
3. Enable the MIDImix output for MIDI output.
4. Do not also configure the same MIDImix as a Reaper control surface for this
   track unless you know that setup will not intercept or echo the same MIDI.

Reaper track routing can send MIDI to hardware via a track's routing window.
The Reaper user guide calls this `MIDI Hardware Output` and describes sends as
being able to carry audio, MIDI, or both.

## Simple one-track setup

Use this when both your note source and MIDImix can arrive on one Reaper MIDI
input path.

1. Create a track named `Gremlin`.
2. Insert `gremlin.vst3` on that track.
3. Arm the track and enable input monitoring.
4. Set the track input to your keyboard, the MIDImix, or `All MIDI Inputs`.
5. Open the track routing window.
6. Set `MIDI Hardware Output` to the MIDImix output port.

The plugin consumes the MIDImix controller notes and CCs, then emits LED note
messages back out of the same track.

## Cleaner three-track setup

Use this when you want to avoid mixing performance notes and controller gestures
on one armed track.

1. Create `Gremlin Notes`.
   Set input to your keyboard or sequencer MIDI source, arm it, and enable input
   monitoring.
2. Create `Gremlin MIDImix`.
   Set input to the MIDImix, arm it, and enable input monitoring.
3. Create `Gremlin Synth`.
   Insert `gremlin.vst3` here.
4. On `Gremlin Notes`, create a send to `Gremlin Synth`.
   Set audio to `None` and MIDI to `All => All`.
5. On `Gremlin MIDImix`, create a send to `Gremlin Synth`.
   Set audio to `None` and MIDI to `All => All`.
6. On `Gremlin Synth`, open the routing window and set `MIDI Hardware Output`
   to the MIDImix output port.
7. Leave `Gremlin Synth` routed to the master for audio.
   Do not send its MIDI output back to either input track.

This gives Gremlin two independent MIDI feeds and one hardware MIDI feedback
path.

## With Gremlin Driver

For automated macro movement:

```text
[keyboard or sequencer] -> [gremlin_driver.vst3] -> [gremlin.vst3] -> audio
                                                    -> MIDImix LED output
```

Put `gremlin_driver.vst3` on a MIDI utility track, then send MIDI only from that
track to the `Gremlin Synth` track. Keep the MIDImix hardware output on the
Gremlin track, because Gremlin is the plugin that knows the current mode, scene,
momentary, and solo-bank LED state.

## LED mapping

- `MUTE` row: lit while the matching hold pad is active.
- `SOLO + MUTE` row: lit while the MIDImix `SOLO` modifier is held.
- `REC ARM` 1-4: current source mode for `Shard`, `Servo`, `Spray`, and
  `Collapse`.
- `REC ARM` 5-8: current scene when a scene is loaded.
- `BANK LEFT`: lit when the extended `Ring` mode is active.
- `BANK RIGHT`: lit when the extended `Vapor` mode is active.
- `SOLO`: lit while the `SOLO` modifier is held.

Gremlin refreshes the LED state periodically and also emits immediate updates
when a mapped controller message changes the state.
