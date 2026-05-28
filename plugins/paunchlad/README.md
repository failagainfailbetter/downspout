# PaunchLad

PaunchLad is a Launchpad-controlled dub performance effect. Put it on a snare,
drum bus, send return, or live input channel, then use the Launchpad for echo
throws, sirens, spring splashes, dropouts, and tape-style chops.

It is both practical and playable:

- stereo audio input/output passes the source through a dub delay path;
- Launchpad MIDI input triggers momentary performance actions;
- generated sirens and spring splashes are mixed into the audio output;
- MIDI output sends Launchpad Mini Mk3 Programmer Mode LED feedback by default.

LED feedback follows the same approach as Luma: it is on by default so the
Launchpad enters programmer mode and the grid uses the reliable `11..88` pad
map. Leave it on when the plugin's MIDI output is routed directly back to the
Launchpad hardware. Turn it off when the plugin's MIDI output is routed onward
to instruments, because Launchpad LED updates are ordinary MIDI note/CC
messages and can otherwise play notes.

PaunchLad accepts the Launchpad programmer grid notes (`11..88`) while LED
feedback is enabled. With LEDs disabled it falls back to a linear chromatic grid
(`36..99`) because that is the common simple MIDI-source layout.

## Grid layout

Visible top to bottom:

- row 1: panic, freeze, and reset gestures;
- row 2: car alarms and warning tones, for example pad `(2,7)`;
- row 3: sirens and laser sweeps;
- row 4: echoing snare throws, for example pad `(4,3)`;
- row 5: spring splashes and tone jumps;
- row 6: dry dropouts that cut the incoming audio;
- row 7: rhythmic chops and stutters;
- row 8: dub echo throws, from tight slap to long runaway repeats.

The top Launchpad buttons trigger common gestures and safety actions. The right
side buttons set siren/alarm level and fire matching alarm accents.

## Reaper routing

1. Put `paunchlad.vst3` on an audio track or return track.
2. Send snare, percussion, vocal, or full drum bus audio into that track.
3. Enable the Launchpad MIDI input for that track, but avoid also routing the
   same Launchpad input directly to a synth track unless you want those notes.
4. Set the track's `MIDI Hardware Output` to the Launchpad output port for LED
   feedback. This is the recommended setup and matches Luma.
5. If you route PaunchLad's MIDI output onward to instruments instead, turn
   `LED Feedback` off.

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
