#pragma once

#include "counterpointer_core_types.hpp"

#include <optional>
#include <string>

namespace downspout::counterpointer {

[[nodiscard]] std::string serializeControls(const Controls& controls);
[[nodiscard]] std::optional<Controls> deserializeControls(const std::string& text);

[[nodiscard]] std::string serializePhraseState(const PhraseState& state);
[[nodiscard]] std::optional<PhraseState> deserializePhraseState(const std::string& text);

[[nodiscard]] std::string serializeVariationState(const VariationState& state);
[[nodiscard]] std::optional<VariationState> deserializeVariationState(const std::string& text);

}  // namespace downspout::counterpointer
