# Downspout AI Coordinator

`downspout-ai-coordinator` is the out-of-plugin process planned for AI-assisted
phrase generation. The current implementation is a local deterministic stub:
it reads a small tune-state JSON file, generates a validated Sidecar phrase,
and writes a Standard MIDI File.

## Command

```bash
downspout-ai-coordinator generate tools/ai-coordinator/examples/state.json --out /tmp/solo.mid --phrase /tmp/phrase.txt
```

The `--phrase` output is optional. It writes Sidecar's text phrase state so the
same phrase can be inspected or used in future plugin import work.

## State Format

The first JSON shape is intentionally small and hand-writable:

```json
{
  "key": "D",
  "scale": "dorian",
  "genre": "jazz",
  "tempo": 132,
  "bars": 4,
  "beats_per_bar": 4,
  "channel": 1,
  "register_low": 60,
  "register_high": 84,
  "density": 0.62,
  "risk": 0.38,
  "seed": 7
}
```

No API key, HTTP server, or model call is involved yet. The next step is to
replace the deterministic generator with validated OpenAI output while keeping
the MIDI writer and phrase validation path unchanged.

## MIDI-First Context

The coordinator should primarily derive tune context from MIDI routed to
Sidecar or captured into a local request file. That keeps the system modular:
any existing plugin, external sequencer, hardware input, or manually played
track can become source material without changing the source plugin.

Useful derived context:

- pitch-class histogram for key/scale hints;
- note density and rests for phrase density;
- register range for solo bounds;
- recent bass/chord tones for guide targets;
- bar/beat positions for call-and-response timing.

## Optional State Summary Helpers

Ground and Cadence now expose core-library summary helpers:

- `downspout::ground::summarizeGroundAiState(...)`
- `downspout::cadence::summarizeCadenceAiState(...)`

They produce compact JSON fragments for the coordinator: Ground supplies form,
phrase-role, meter, and guide-bass context; Cadence supplies learned harmony,
voicing, and chord-quality context. These are optional diagnostics/hints and
are not required for the MIDI-first workflow.
