# Luma design notes

Luma is a new Downspout plugin inspired by the Launchpad experiments in
`flues/lv2`, especially:

- `arpiso`: held grid pads as active musical sources;
- `achord`: immediate harmonic pad-table behavior;
- `padseq`: clear Launchpad LED feedback and transport-following MIDI output;
- `quadrangle`: multiple musical roles on one 8x8 surface.

The design intentionally avoids a literal quadrant workstation. The player
should only need to remember that lower pads are safer and more grounded, upper
pads are brighter and busier, and the side buttons raise scene energy.

## Current layout

- rows 0-1: bass agents;
- rows 2-3: chord agents;
- rows 4-5: melody agents;
- rows 6-7: drum agents.

Columns imply harmonic distance and rhythmic phase. Active pads are agents, not
stored steps. The core decides when each agent fires from transport step,
density, energy, row, and column.

## Host mapping

The VST3 wrapper exposes one MIDI input and one MIDI output. The output carries
both generated notes and Launchpad LED feedback. Hosts that need separate synth
and LED destinations should duplicate or route the MIDI output at the track
level.
