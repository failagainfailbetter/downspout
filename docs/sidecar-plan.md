# Sidecar implementation checklist

Sidecar is the Downspout VST client for AI-assisted phrase playback. The local
`downspout-ai-coordinator` process will own OpenAI API calls later; Sidecar
stays realtime-safe and only queues validated MIDI phrases.

## First slice

- [x] Add `plugins/sidecar` scaffold.
- [x] Add local phrase protocol structs and validation.
- [x] Add deterministic fallback phrase generation.
- [x] Add transport-aware phrase playback engine.
- [x] Add text serialization for accepted phrase state.
- [x] Add DPF/VST3 wrapper and compact UI.
- [x] Add deterministic core tests.
- [x] Wire CMake build target and root build option.
- [x] Add coordinator CLI at `tools/ai-coordinator`.
- [ ] Add OpenAI API client in coordinator.
- [x] Add `state.json -> solo.mid` workflow.
- [ ] Add localhost live request/response.
- [ ] Add state summaries for Ground and Cadence.
- [ ] Add Sidecar to release packaging and screenshot documentation once the
  first plugin slice is accepted.

## Implemented plugin slice

- `plugins/sidecar` builds `sidecar.vst3`.
- The core validates phrase version, length, event count, register, duration,
  velocity, and monophonic event ordering.
- The fallback generator provides deterministic local phrases for testing and
  offline use before the coordinator exists.
- Accepted phrases are serialized into plugin state as text.
- The engine schedules note-on/note-off MIDI from host BBT transport and clears
  active notes when playback stops, the phrase is unavailable, or Mute is on.
- The compact UI exposes Channel, Bars, Register, custom range, Density, Risk,
  Humanize, Mute/Open, Generate, Accept, and Retry.

## Current non-goals

- No network calls in the plugin.
- No API keys in plugin state.
- No MCP layer.
- No automatic model calls during playback.

## Implemented coordinator slice

- `downspout-ai-coordinator generate state.json --out solo.mid` reads a small,
  hand-writable tune-state JSON file.
- The current generator is deterministic and local. It is a placeholder for
  validated model output, not an API client.
- The output path writes a Standard MIDI File that can be imported into a DAW.
- Optional `--phrase phrase.txt` writes Sidecar text phrase state for inspection
  and future plugin import work.
