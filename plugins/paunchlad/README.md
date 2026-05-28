# PaunchLad

PaunchLad is a Launchpad-controlled dub performance effect. Put it on a snare,
drum bus, send return, or live input channel, then use the Launchpad for echo
throws, sirens, spring splashes, dropouts, and tape-style chops.

It is both practical and playable:

- stereo audio input/output passes the source through a dub delay path;
- Launchpad MIDI input triggers momentary performance actions;
- generated sirens and spring splashes are mixed into the audio output;
- optional MIDI output sends Launchpad Mini Mk3 Programmer Mode LED feedback.

LED feedback is off by default. Leave it off when the plugin's MIDI output is
routed onward to instruments, because Launchpad LED updates are ordinary MIDI
note/CC messages and can otherwise play notes. Enable it only when the track's
MIDI output is routed directly back to the Launchpad hardware.

PaunchLad accepts both the Launchpad programmer grid notes (`11..88`) and the
default custom/user grid layout (`36..67` on the left half, `68..99` on the
right half). With LED feedback enabled it assumes programmer mode; with LEDs
disabled it defaults to the custom/user layout used by the Launchpad as a simple
MIDI source.

## Grid layout

Bottom to top:

- rows 1-2: echo/snare throws, from tight slap to long runaway repeats;
- rows 3-4: sirens, lasers, and alarm sweeps;
- row 5: spring splashes and tone jumps;
- row 6: dry dropouts;
- row 7: rhythmic chops;
- row 8: freeze, panic, and big reset gestures.

The top Launchpad row nudges wet level, feedback, and tone. The right side
buttons set siren level from low to high.

## Reaper routing

1. Put `paunchlad.vst3` on an audio track or return track.
2. Send snare, percussion, vocal, or full drum bus audio into that track.
3. Enable the Launchpad MIDI input for that track, but avoid also routing the
   same Launchpad input directly to a synth track unless you want those notes.
4. Leave PaunchLad's `LED Feedback` button off for simple MIDI-source use.
5. For LEDs, set the track's `MIDI Hardware Output` to the Launchpad output
   port, then enable PaunchLad's `LED Feedback` button.

For classic snare echo, put PaunchLad on an aux/return and send only the snare
or rimshot to it. For live dub mixing, put it on a drum bus and use the dry,
wet, feedback, and output sliders as the safety controls.

## Build

PaunchLad is enabled by default with `DOWNSPOUT_BUILD_PAUNCHLAD=ON`.

```bash
cmake -S ../.. -B ../../build -DDOWNSPOUT_BUILD_PAUNCHLAD=ON
cmake --build ../../build --target paunchlad-vst3 downspout_paunchlad_core_tests
ctest --test-dir ../../build --output-on-failure -R paunchlad
```
