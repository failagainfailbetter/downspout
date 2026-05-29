# Lifeform design notes

`Lifeform` is an original Downspout performance generator rather than a direct
LV2 port.

## Core behavior

- The 8x8 board follows normal toroidal Conway Game of Life rules: cells wrap
  across left/right and top/bottom edges.
- A generation is triggered once per musical beat.
- With valid host BBT transport, beat boundaries come from the host.
- Without host BBT, `Auto` falls back to a free 120 BPM clock.
- Living cells emit notes before the next generation is calculated.
- Newly born cells receive a velocity lift and white Launchpad LEDs for one
  generation.

## Musical mapping

Each generation emits MIDI from the current board before calculating the next
board state. This makes the visible pattern the audible pattern for that beat.

The first mapping is deliberately simple and playable:

- columns walk through scale degrees;
- row pairs move by octave;
- `Voices` mode splits low/mid/high rows across adjacent channels;
- `Single` mode keeps all events on the base channel;
- `Drums` mode maps cells to a small GM drum note set.

Implementation details:

- melodic degree is `col + (row % 2) * 2`;
- octave shift is `(row / 2) - 1`;
- selected scale intervals are added to `Root Note`;
- drum mode uses `kDrumNotes[(row + col) % 8]`;
- the current drum palette is `36, 38, 42, 46, 45, 50, 39, 51`;
- velocity starts at `40 + Velocity * 56`;
- higher rows add a small velocity lift;
- the column matching `generation % 8` gets an accent;
- newly born cells get an additional accent.

This keeps the board readable while leaving room for later selectable mappings,
rotation, wrapping, probability, and external rhythm influence.

## LED mapping

Hardware grid colours are generated from the current Life state:

- dead cells send colour `0` / off;
- newly born live cells send colour `3` / white;
- live rows 0-1 send colour `45` / blue;
- live rows 2-4 send colour `21` / green;
- live rows 5-6 send colour `9` / orange;
- live row 7 sends colour `53` / purple.

Side buttons show seed selection: selected seed is colour `57` / pink, other
seeds are colour `2` / dim. Top buttons use fixed action colours, except the
run button becomes colour `14` / yellow when stopped.

## Launchpad mapping

The grid uses Novation programmer-mode note coordinates. UI drawing is inverted
vertically so the bottom row in the UI corresponds to Launchpad note row `1`.
Top-row and side buttons are Launchpad control-change messages and provide
run/step/random/clear/seed/performance shortcuts. `CC 99` is treated as the
logo/panic control, matching `flues/lv2/padseq`. Launchpad pad and control
release messages are consumed rather than echoed, because note/CC messages with
zero values are also LED-off commands in programmer mode.

The panic path follows the `padseq` pattern: emit programmer-mode SysEx
(`F0 00 20 29 02 0D 0E 01 F7`), emit a bulk LED-lighting SysEx with every grid,
side, top, and logo LED set to colour 0, clear all cells, clear pending notes,
reset clock state, and stop the generator.

The vendored DPF VST3 bridge is patched locally to forward SysEx as VST3 data
events, because upstream DPF's default VST3 MIDI output only forwards note, CC,
pressure, and pitch-bend events. Panic also sends ordinary 3-byte note/CC LED
off messages after the SysEx clear, so a Launchpad that is already in programmer
mode can still be cleared if the host filters SysEx on hardware outputs.

Musical MIDI defaults to channel 4 so generated notes do not overlap the
programmer-mode LED channels 1-3. Setting the base channel back to 1 is still
allowed, but routing that output to the Launchpad will make generated notes act
as LED commands.
