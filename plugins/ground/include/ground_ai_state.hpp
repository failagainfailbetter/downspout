#pragma once

#include "ground_core_types.hpp"

#include <string>

namespace downspout::ground {

[[nodiscard]] const char* scaleName(ScaleId scale) noexcept;
[[nodiscard]] const char* styleName(StyleId style) noexcept;
[[nodiscard]] const char* phraseRoleName(PhraseRoleId role) noexcept;

[[nodiscard]] std::string summarizeGroundAiState(const Controls& controls,
                                                 const FormState& form,
                                                 int currentPhraseIndex);

}  // namespace downspout::ground
