#pragma once

#include "sidecar_core_types.hpp"

#include <optional>
#include <string>

namespace downspout::sidecar {

[[nodiscard]] std::string serializePhrase(const Phrase& phrase);
[[nodiscard]] std::optional<Phrase> deserializePhrase(const std::string& text);

}  // namespace downspout::sidecar
