# Lifeform design notes

`Lifeform` is an original Downspout performance generator rather than a direct
LV2 port.

## Core behavior

- The 8x8 board follows normal finite-edge Conway Game of Life rules.
- A generation is triggered once per musical beat.
- With valid host BBT transport, beat boundaries come from the host.
- Without host BBT, `Auto` falls back to a free 120 BPM clock.
- Living cells emit notes before the next generation is calculated.
- Newly born cells receive a velocity lift and white Launchpad LEDs for one
  generation.

## Musical mapping

The first mapping is deliberately simple and playable:

- columns walk through scale degrees;
- row pairs move by octave;
- `Voices` mode splits low/mid/high rows across adjacent channels;
- `Single` mode keeps all events on the base channel;
- `Drums` mode maps cells to a small GM drum note set.

This keeps the board readable while leaving room for later selectable mappings,
rotation, wrapping, probability, and external rhythm influence.

## Launchpad mapping

The grid uses Novation programmer-mode note coordinates. UI drawing is inverted
vertically so the bottom row in the UI corresponds to Launchpad note row `1`.
Top-row and side buttons are Launchpad control-change messages and provide
run/step/random/clear/seed/performance shortcuts.
