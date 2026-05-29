# Launchpad integration notes

These notes capture the current Downspout assumptions for Novation Launchpad
Mini MK3 / X style programmer-mode hardware.

## Programmer mode layout

The `flues/lv2/padseq` mapping is the reference.

- Enter programmer mode with SysEx:
  `F0 00 20 29 02 0D 0E 01 F7`
- The 8x8 grid uses note-on/note-off messages.
- Grid row 0 is the bottom hardware row.
- Grid row 7 is the top hardware row.
- Grid notes:
  - row 0: `11..18`
  - row 1: `21..28`
  - row 2: `31..38`
  - row 3: `41..48`
  - row 4: `51..58`
  - row 5: `61..68`
  - row 6: `71..78`
  - row 7: `81..88`
- Right side buttons are CCs `19, 29, 39, 49, 59, 69, 79, 89`.
- Top buttons are CCs `91..98`; `99` is the logo button on Mini MK3 style
  mappings.

UI grids should normally draw row 0 at the bottom and row 7 at the top, matching
`padseq`.

## LED messages

In programmer mode, the Launchpad treats normal MIDI messages as LED commands:

- note-on channel 1 (`0x90`) sets static grid LED colour;
- note-on channel 2 (`0x91`) flashes grid LED colour;
- note-on channel 3 (`0x92`) pulses grid LED colour;
- CC channel 1 (`0xB0`) sets static side/top button LED colour;
- velocity/value `0` turns an LED off.

Controller release messages can therefore look like LED-off commands if echoed
back to the device. Launchpad pad/control input should be consumed, not echoed,
unless a plugin intentionally wants to use it as LED output.

Generated musical MIDI should avoid channels 1-3 when the same MIDI output is
routed back to the Launchpad for LEDs. Lifeform defaults musical output to
channel 4 for this reason.

## Clearing and panic

`padseq` clears the surface with a bulk LED-lighting SysEx:

- header: `F0 00 20 29 02 0D`
- command: `03`
- repeated triplets: lighting type `00`, LED id, colour `00`
- terminator: `F7`

For the Mini MK3 programmer layout, a full clear should cover all 64 grid notes,
the 8 side-button CC ids, and the 9 top/logo CC ids.

Because some VST3 hosts or hardware routes may filter SysEx, plugins should
also be able to clear an already-programmer-mode Launchpad with ordinary
3-byte note/CC LED-off messages.

## DPF/VST3 note

DPF's VST3 bridge converts outgoing DPF MIDI events to VST3 note/CC-style
events. In this repository the vendored DPF bridge has been explicitly approved
for a local SysEx forwarding patch so programmer-mode and bulk LED SysEx can be
sent as VST3 data events. Future shared/framework changes still require explicit
approval before implementation.

## Routing

For VST3 plugins with one MIDI output carrying both musical MIDI and LED
feedback, the host must route that output both to the target instrument and to
the Launchpad hardware. Use separate channels or filters so generated musical
notes do not drive Launchpad LED channels accidentally.
