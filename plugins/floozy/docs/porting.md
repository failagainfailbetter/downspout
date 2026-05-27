# Floozy porting notes

Floozy follows the current Downspout plugin pattern:

1. `FloozyEngine` owns the portable voice engine and MIDI handling.
2. `FloozyPlugin` maps DPF parameters, MIDI events, sample rate, and stereo
   outputs to the engine.
3. `FloozyUI` is a NanoVG editor over the same parameter table.
4. `downspout_floozy_core_tests` exercises core behavior without DPF or a DAW.

## Parameter contract

The parameter table lives in `include/floozy_params.hpp`. The symbols are
stable and trace the original LV2 control intent:

- source algorithm and source shaping;
- envelope attack/release and interface type/intensity;
- tuning, delay ratio, and feedback;
- filter and modulation;
- reverb and master gain.

## Host mapping

The DPF wrapper exposes Floozy as an instrument:

- no audio inputs;
- two audio outputs grouped as stereo;
- one MIDI input;
- VST3 category `Instrument|Synth`;
- metadata maker `danja`, brand/group `Downspout`.
