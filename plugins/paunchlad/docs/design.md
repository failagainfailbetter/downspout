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

The first pass targets Novation Launchpad Mini Mk3 Programmer Mode. The mapping
should stay behind the processor's grid helpers so other Launchpad layouts can
be added without changing the audio behavior.
