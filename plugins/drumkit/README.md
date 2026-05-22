# drumkit port

`drumkit` is the Downspout VST3/DPF port of `flues/lv2/drumkit`.

The port keeps the LV2 synth behavior in a portable core and exposes it through a thin DPF instrument wrapper:

- stereo audio output;
- one MIDI input, responding to the original note map;
- 43 inherited synthesis and master parameters with the LV2 ranges/defaults preserved;
- 11 added per-instrument mute parameters;
- a new custom NanoVG UI built around instrument strips, focused voice editing, and master bus controls.

Mute behavior:

- muted instruments ignore incoming note-ons;
- enabling a mute resets that instrument voice immediately;
- muted instruments are skipped in the per-sample voice loop, reducing CPU for disabled voices;
- closed-hat note-ons still choke the open hat, even when the closed hat itself is muted.

Current MIDI map:

- Kick 36
- Clap 39
- Snare 40
- Crash 41
- Closed HH 42
- Low Tom 45
- Open HH 46
- High Tom 50
- Bash 51
- Cowbell 52
- Clave 53
