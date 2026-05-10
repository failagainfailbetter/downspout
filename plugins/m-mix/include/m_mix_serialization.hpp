#pragma once

#include "m_mix_core_types.hpp"

#include <optional>
#include <string>

namespace downspout::mmix {

[[nodiscard]] std::string serializeParameters(const Parameters& parameters);
[[nodiscard]] std::optional<Parameters> deserializeParameters(const std::string& text);

}  // namespace downspout::mmix
