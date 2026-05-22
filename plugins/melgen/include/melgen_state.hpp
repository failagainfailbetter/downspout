#pragma once

#include "melgen_core_types.hpp"

namespace downspout::melgen {

[[nodiscard]] PatternState sanitizePatternState(const PatternState& raw, bool* valid = nullptr);
[[nodiscard]] VariationState sanitizeVariationState(const VariationState& raw);

}  // namespace downspout::melgen
