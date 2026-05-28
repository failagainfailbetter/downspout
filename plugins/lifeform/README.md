# Lifeform

`Lifeform` is a Launchpad-oriented MIDI generator based on Conway's Game of
Life. The 8x8 Launchpad grid is the Life board, each musical beat advances one
generation, and living cells emit MIDI before the board evolves.

## Controls

- Click a cell in the UI, or press the matching Launchpad grid pad, to flip it.
- `Run` starts or stops beat-synced evolution.
- `Step` advances one generation immediately.
- `Random` fills the grid using the `Density` control.
- `Clear` empties the board.
- `LED` enables Launchpad feedback.
- `Scale`, `Root`, and `Output` choose how cells become notes.
- `Mutation` adds a small amount of random cellular drift.

## Launchpad Controls

The grid uses Novation programmer-mode note numbers, where note `11` is the
bottom-left cell and note `88` is the top-right cell.

Top-row buttons:

- `91`: run/stop
- `92`: step
- `93`: randomize
- `94`: clear
- `95`: next seed pattern
- `96`/`97`: mutation down/up
- `98`/`99`: density down/up

Side buttons `19` through `89` load seed patterns.

## MIDI Mapping

In `Voices` mode, low, middle, and high rows are sent to adjacent MIDI channels
starting from `Base Channel`. In `Single` mode all notes use the base channel.
In `Drums` mode living cells emit a compact GM-style drum palette.

The plugin has no audio I/O. Route its MIDI output to an instrument track, and
route the same MIDI output back to the Launchpad if you want hardware LEDs.

See [docs/design.md](docs/design.md) for implementation notes.
