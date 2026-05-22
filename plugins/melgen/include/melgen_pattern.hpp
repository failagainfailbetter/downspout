#pragma once

#include "melgen_core_types.hpp"

namespace downspout::melgen {

[[nodiscard]] Controls clampControls(const Controls& raw);
[[nodiscard]] bool structuralControlsChanged(const Controls& a, const Controls& b);
[[nodiscard]] int stepsPerBeatForSubdivision(SubdivisionId subdivision);
[[nodiscard]] int registerOffset(int reg);
[[nodiscard]] int nearestScaleNote(const Controls& controls, int targetNote, int minNote, int maxNote);

void regeneratePattern(PatternState& pattern,
                       const Controls& controls,
                       const ::downspout::Meter& meter,
                       bool regenRhythm,
                       bool regenNotes);

void partialNoteMutation(PatternState& pattern,
                         const Controls& controls,
                         float strength);

}  // namespace downspout::melgen
