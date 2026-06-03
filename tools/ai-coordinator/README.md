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
