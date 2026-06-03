#pragma once

#include "sidecar_core_types.hpp"

#include <array>

namespace downspout::sidecar {

struct EngineState {
    Controls controls {};
    Phrase phrase {};
    bool phraseReady = false;
    int activeNote = -1;
    bool wasPlaying = false;
};

struct BlockResult {
    std::array<ScheduledMidiEvent, kMaxScheduledMidiEvents> events {};
    int eventCount = 0;
    bool phraseReady = false;
};

void activate(EngineState& state, const Controls& controls);
void setPhrase(EngineState& state, const Phrase& phrase);
void clearPhrase(EngineState& state);
[[nodiscard]] BlockResult processBlock(EngineState& state,
                                       const Controls& controls,
                                       const TransportSnapshot& transport,
                                       std::uint32_t nframes,
                                       double sampleRate);

}  // namespace downspout::sidecar
