#include "m_mix_serialization.hpp"

#include "m_mix_engine.hpp"

#include <cstdlib>
#include <sstream>

namespace downspout::mmix {
namespace {

[[nodiscard]] bool parseFloat(const std::string& text, float& value) {
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return end != text.c_str() && *end == '\0';
}

}  // namespace

std::string serializeParameters(const Parameters& rawParameters) {
    const Parameters parameters = clampParameters(rawParameters);
    return "version=1\n"
           "totalBars=" + std::to_string(parameters.totalBars) + "\n"
           "division=" + std::to_string(parameters.division) + "\n"
           "steps=" + std::to_string(parameters.steps) + "\n"
           "offset=" + std::to_string(parameters.offset) + "\n"
           "fadeBars=" + std::to_string(parameters.fadeBars) + "\n"
           "granularity=" + std::to_string(parameters.granularity) + "\n"
           "maintain=" + std::to_string(parameters.maintain) + "\n"
           "fade=" + std::to_string(parameters.fade) + "\n"
           "cut=" + std::to_string(parameters.cut) + "\n"
           "fadeDurMax=" + std::to_string(parameters.fadeDurMax) + "\n"
           "bias=" + std::to_string(parameters.bias) + "\n"
           "velocityFades=" + std::to_string(parameters.velocityFades) + "\n"
           "mute=" + std::to_string(parameters.mute) + "\n";
}

std::optional<Parameters> deserializeParameters(const std::string& text) {
    Parameters parameters {};
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        float parsed = 0.0f;
        if (key == "version") {
            continue;
        }
        if (!parseFloat(value, parsed)) {
            return std::nullopt;
        }

        if (key == "totalBars") {
            parameters.totalBars = parsed;
        } else if (key == "division") {
            parameters.division = parsed;
        } else if (key == "steps") {
            parameters.steps = parsed;
        } else if (key == "offset") {
            parameters.offset = parsed;
        } else if (key == "fadeBars") {
            parameters.fadeBars = parsed;
        } else if (key == "granularity") {
            parameters.granularity = parsed;
        } else if (key == "maintain") {
            parameters.maintain = parsed;
        } else if (key == "fade") {
            parameters.fade = parsed;
        } else if (key == "cut") {
            parameters.cut = parsed;
        } else if (key == "fadeDurMax") {
            parameters.fadeDurMax = parsed;
        } else if (key == "bias") {
            parameters.bias = parsed;
        } else if (key == "velocityFades") {
            parameters.velocityFades = parsed;
        } else if (key == "mute") {
            parameters.mute = parsed;
        }
    }

    return clampParameters(parameters);
}

}  // namespace downspout::mmix
