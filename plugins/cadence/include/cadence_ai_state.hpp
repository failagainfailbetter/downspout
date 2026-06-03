#pragma once

#include "cadence_core_types.hpp"

#include <string>

namespace downspout::cadence {

[[nodiscard]] const char* scaleName(int scale) noexcept;
[[nodiscard]] const char* chordQualityName(int quality) noexcept;
[[nodiscard]] const char* chordSizeName(int chordSize) noexcept;
[[nodiscard]] const char* registerName(int reg) noexcept;

[[nodiscard]] std::string summarizeCadenceAiState(const Controls& controls,
                                                  const ProgressionState& progression);

}  // namespace downspout::cadence
