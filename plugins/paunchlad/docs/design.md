# PaunchLad design notes

PaunchLad is a fixed-layout Launchpad dub effect. It deliberately avoids deep
menus: the grid is a row-based performance surface and the UI mirrors the same
state.

Core roles:

- echo throw rows push incoming audio into a delay line with boosted feedback;
- siren rows generate internal oscillator sweeps;
- spring row injects decaying noisy resonator splashes and tone jumps;
- dropout/chop rows momentarily remove or rhythmically interrupt dry signal;
- top row gestures clear or freeze performance state.

The first pass supports two Launchpad input layouts:

- custom/user note mode, using notes `36..67` on the left half and `68..99` on
  the right half, is the default because it matches the Launchpad's plain
  MIDI-source behaviour;
- programmer grid mode, using notes `11..88`, is assumed when LED feedback is
  enabled.

LED feedback is intentionally off by default. The LED messages are normal MIDI
note and CC output, so enabling them is only practical when the host routes the
plugin MIDI output directly back to the Launchpad hardware instead of into
another instrument.
