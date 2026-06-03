# Sidecar

Sidecar is the Downspout MIDI plugin intended to sit beside the deterministic
ensemble plugins and play validated phrase material. The first implementation is
local-only: it does not call an API from the realtime plugin thread.

## Current behavior

- Generates deterministic fallback phrases from the UI constraints.
- Captures routed MIDI note-ons as optional local context for Generate/Retry.
- Stores the accepted phrase in plugin state.
- Plays the phrase against host BBT transport.
- Emits MIDI note-on/note-off events on the selected channel.
- Does not pass routed input MIDI through to the output.
- Clears active notes immediately when transport stops, Mute is enabled, or the
  phrase is unavailable.

## Controls

- `Channel`: output MIDI channel.
- `Bars`: phrase length in bars.
- `Register`: low, mid, high, or custom pitch range.
- `Low Note` / `High Note`: custom register bounds.
- `Density`: number of phrase events.
- `Risk`: chance of wider or more surprising note choices in fallback mode.
- `Humanize`: reserved for timing/velocity shaping.
- `Output`: open or muted.
- `Generate`: creates a new deterministic fallback phrase.
- `Accept`: marks the current phrase as accepted for session state.
- `Retry`: creates an alternate deterministic fallback phrase.

If MIDI has been routed into Sidecar, Generate and Retry use the captured note
range, density, and event seed as hints. If no MIDI has been routed in, the
plugin keeps the original local fallback behavior.

## Planned coordinator work

The plugin-side contract is intentionally small so a separate
`downspout-ai-coordinator` process can own API calls, validation, and prompt
construction later. See `docs/ai-api.md` and `docs/sidecar-plan.md`.
