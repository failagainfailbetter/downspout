#pragma once

#include "melgen_core_types.hpp"

namespace downspout::melgen {

void resetVariationProgress(VariationState& variation);
[[nodiscard]] bool applyLoopVariation(PatternState& pattern,
                                      VariationState& variation,
                                      const Controls& controls,
                                      const ::downspout::Meter& meter,
                                      double beatsPerBar);

}  // namespace downspout::melgen
