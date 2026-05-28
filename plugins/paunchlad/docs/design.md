# PaunchLad design notes

PaunchLad is a fixed-layout Launchpad dub effect. It deliberately avoids deep
menus: the grid is a row-based performance surface and the UI mirrors the same
state.

Core roles:

- bottom echo and visible row 4 snare gestures push incoming audio or generated
  snare hits into a delay line with boosted feedback;
- visible row 2 generates car-alarm tones;
- visible row 3 generates internal oscillator sweeps;
- spring row injects decaying noisy resonator splashes and tone jumps;
- dropout/chop rows momentarily remove or rhythmically interrupt dry signal;
- top row gestures clear or freeze performance state.

The first pass supports two Launchpad input layouts:

- programmer grid mode, using notes `11..88`, is the default because LED
  feedback is on by default and sends the same programmer-mode setup used by
  Luma;
- linear chromatic note mode, using notes `36..99`, is accepted when LED
  feedback is off. This deliberately takes priority over split custom/user maps
  because the note ranges overlap and a single pad press cannot disambiguate
  them reliably.

LED feedback is useful but must be routed deliberately. The LED messages are
normal MIDI note and CC output, so they should go directly back to the Launchpad
hardware instead of into another instrument.
