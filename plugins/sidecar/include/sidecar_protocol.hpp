#pragma once

#include "sidecar_core_types.hpp"

namespace downspout::sidecar {

[[nodiscard]] Controls clampControls(const Controls& raw);
[[nodiscard]] ValidationResult validatePhrase(const Phrase& phrase, const Controls& controls);
[[nodiscard]] Phrase makeFallbackPhrase(const Controls& controls, std::uint32_t seed);

}  // namespace downspout::sidecar
