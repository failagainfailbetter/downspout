#pragma once

#include "m_mix_core_types.hpp"

namespace downspout::mmix {

[[nodiscard]] Parameters clampParameters(const Parameters& raw);
void activate(EngineState& state);
void deactivate(EngineState& state);

[[nodiscard]] BlockResult processBlock(EngineState& state,
                                       const Parameters& rawParameters,
                                       const TransportSnapshot& transport,
                                       std::uint32_t nframes,
                                       double sampleRate,
                                       const InputMidiEvent* midiEvents,
                                       std::uint32_t midiEventCount);

}  // namespace downspout::mmix
