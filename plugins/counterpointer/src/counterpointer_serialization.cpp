#include "counterpointer_serialization.hpp"

#include <charconv>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace downspout::counterpointer {
namespace {

bool parseFloat(std::string_view text, float& value)
{
    std::string local(text);
    char* end = nullptr;
    value = std::strtof(local.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool parseDouble(std::string_view text, double& value)
{
    std::string local(text);
    char* end = nullptr;
    value = std::strtod(local.c_str(), &end);
    return end != nullptr && *end == '\0';
}

template <typename T>
bool parseInteger(std::string_view text, T& value)
{
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc() && result.ptr == end;
}

std::vector<std::string_view> split(std::string_view text, const char delimiter)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;

    while (start <= text.size())
    {
        const std::size_t pos = text.find(delimiter, start);
        if (pos == std::string_view::npos)
        {
            parts.push_back(text.substr(start));
            break;
        }

        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }

    return parts;
}

}  // namespace

std::string serializeControls(const Controls& controls)
{
    return "version=1\n"
           "key=" + std::to_string(controls.key) + "\n"
           "scale=" + std::to_string(controls.scale) + "\n"
           "cycle_bars=" + std::to_string(controls.cycle_bars) + "\n"
           "granularity=" + std::to_string(controls.granularity) + "\n"
           "follow=" + std::to_string(controls.follow) + "\n"
           "counter=" + std::to_string(controls.counter) + "\n"
           "short_random=" + std::to_string(controls.short_random) + "\n"
           "long_random=" + std::to_string(controls.long_random) + "\n"
           "density=" + std::to_string(controls.density) + "\n"
           "rhythm_follow=" + std::to_string(controls.rhythm_follow) + "\n"
           "syncopation=" + std::to_string(controls.syncopation) + "\n"
           "consonance=" + std::to_string(controls.consonance) + "\n"
           "reg=" + std::to_string(controls.reg) + "\n"
           "span=" + std::to_string(controls.span) + "\n"
           "gate=" + std::to_string(controls.gate) + "\n"
           "velocity_follow=" + std::to_string(controls.velocity_follow) + "\n"
           "pass_input=" + std::to_string(controls.pass_input ? 1 : 0) + "\n"
           "output_channel=" + std::to_string(controls.output_channel) + "\n"
           "action_learn=" + std::to_string(controls.action_learn) + "\n"
           "freeze=" + std::to_string(controls.freeze ? 1 : 0) + "\n";
}

std::optional<Controls> deserializeControls(const std::string& text)
{
    Controls controls = defaultControls();

    for (const std::string_view line : split(text, '\n'))
    {
        if (line.empty())
            continue;

        const std::size_t sep = line.find('=');
        if (sep == std::string_view::npos)
            return std::nullopt;

        const std::string_view key = line.substr(0, sep);
        const std::string_view value = line.substr(sep + 1);
        float floatValue = 0.0f;
        int intValue = 0;

        if (key == "version")
            continue;
        else if (key == "key" && parseInteger(value, intValue))
            controls.key = intValue;
        else if (key == "scale" && parseInteger(value, intValue))
            controls.scale = intValue;
        else if (key == "cycle_bars" && parseInteger(value, intValue))
            controls.cycle_bars = intValue;
        else if (key == "granularity" && parseInteger(value, intValue))
            controls.granularity = intValue;
        else if (key == "follow" && parseFloat(value, floatValue))
            controls.follow = floatValue;
        else if (key == "counter" && parseFloat(value, floatValue))
            controls.counter = floatValue;
        else if (key == "short_random" && parseFloat(value, floatValue))
            controls.short_random = floatValue;
        else if (key == "long_random" && parseFloat(value, floatValue))
            controls.long_random = floatValue;
        else if (key == "density" && parseFloat(value, floatValue))
            controls.density = floatValue;
        else if (key == "rhythm_follow" && parseFloat(value, floatValue))
            controls.rhythm_follow = floatValue;
        else if (key == "syncopation" && parseFloat(value, floatValue))
            controls.syncopation = floatValue;
        else if (key == "consonance" && parseFloat(value, floatValue))
            controls.consonance = floatValue;
        else if (key == "reg" && parseInteger(value, intValue))
            controls.reg = intValue;
        else if (key == "span" && parseFloat(value, floatValue))
            controls.span = floatValue;
        else if (key == "gate" && parseFloat(value, floatValue))
            controls.gate = floatValue;
        else if (key == "velocity_follow" && parseFloat(value, floatValue))
            controls.velocity_follow = floatValue;
        else if (key == "pass_input" && parseInteger(value, intValue))
            controls.pass_input = intValue != 0;
        else if (key == "output_channel" && parseInteger(value, intValue))
            controls.output_channel = intValue;
        else if (key == "action_learn" && parseInteger(value, intValue))
            controls.action_learn = intValue;
        else if (key == "freeze" && parseInteger(value, intValue))
            controls.freeze = intValue != 0;
        else
            return std::nullopt;
    }

    return clampControls(controls);
}

std::string serializePhraseState(const PhraseState& state)
{
    std::string text;
    text.reserve(2048);
    text += "version=" + std::to_string(state.version) + "\n";
    text += "segment_count=" + std::to_string(clampi(state.segmentCount, 0, kMaxSegments)) + "\n";
    text += "ready=" + std::to_string(state.ready ? 1 : 0) + "\n";

    for (int i = 0; i < clampi(state.segmentCount, 0, kMaxSegments); ++i)
    {
        const PhraseStep& step = state.steps[static_cast<std::size_t>(i)];
        const std::string prefix = "step" + std::to_string(i) + ".";
        text += prefix + "active=" + std::to_string(step.active ? 1 : 0) + "\n";
        text += prefix + "note=" + std::to_string(step.note) + "\n";
        text += prefix + "velocity=" + std::to_string(step.velocity) + "\n";
        text += prefix + "onset=" + std::to_string(step.onset) + "\n";
        text += prefix + "gate=" + std::to_string(step.gate) + "\n";
    }

    return text;
}

std::optional<PhraseState> deserializePhraseState(const std::string& text)
{
    PhraseState state;

    for (const std::string_view line : split(text, '\n'))
    {
        if (line.empty())
            continue;

        const std::size_t sep = line.find('=');
        if (sep == std::string_view::npos)
            return std::nullopt;

        const std::string_view key = line.substr(0, sep);
        const std::string_view value = line.substr(sep + 1);
        int intValue = 0;
        double doubleValue = 0.0;

        if (key == "version")
        {
            if (!parseInteger(value, state.version))
                return std::nullopt;
        }
        else if (key == "segment_count")
        {
            if (!parseInteger(value, intValue))
                return std::nullopt;
            state.segmentCount = clampi(intValue, 0, kMaxSegments);
        }
        else if (key == "ready")
        {
            if (!parseInteger(value, intValue))
                return std::nullopt;
            state.ready = intValue != 0;
        }
        else if (key.rfind("step", 0) == 0)
        {
            const std::size_t dot = key.find('.');
            if (dot == std::string_view::npos)
                return std::nullopt;

            int stepIndex = 0;
            if (!parseInteger(key.substr(4, dot - 4), stepIndex))
                return std::nullopt;
            if (stepIndex < 0 || stepIndex >= kMaxSegments)
                return std::nullopt;

            PhraseStep& step = state.steps[static_cast<std::size_t>(stepIndex)];
            const std::string_view field = key.substr(dot + 1);

            if (field == "active")
            {
                if (!parseInteger(value, intValue))
                    return std::nullopt;
                step.active = intValue != 0;
            }
            else if (field == "note")
            {
                if (!parseInteger(value, intValue))
                    return std::nullopt;
                step.note = static_cast<std::uint8_t>(clampi(intValue, 0, 127));
            }
            else if (field == "velocity")
            {
                if (!parseInteger(value, intValue))
                    return std::nullopt;
                step.velocity = static_cast<std::uint8_t>(clampi(intValue, 1, 127));
            }
            else if (field == "onset")
            {
                if (!parseDouble(value, doubleValue))
                    return std::nullopt;
                step.onset = clampd(doubleValue, 0.0, 0.94);
            }
            else if (field == "gate")
            {
                if (!parseDouble(value, doubleValue))
                    return std::nullopt;
                step.gate = clampd(doubleValue, 0.08, 1.0);
            }
            else
            {
                return std::nullopt;
            }
        }
        else
        {
            return std::nullopt;
        }
    }

    state.segmentCount = clampi(state.segmentCount, 0, kMaxSegments);
    state.ready = state.ready && state.segmentCount > 0;
    state.version = kPhraseStateVersion;
    return state;
}

std::string serializeVariationState(const VariationState& state)
{
    return "version=" + std::to_string(state.version) + "\n"
           "completed_cycles=" + std::to_string(state.completed_cycles) + "\n"
           "last_mutation_cycle=" + std::to_string(state.last_mutation_cycle) + "\n"
           "mutation_serial=" + std::to_string(state.mutation_serial) + "\n";
}

std::optional<VariationState> deserializeVariationState(const std::string& text)
{
    VariationState state = defaultVariationState();

    for (const std::string_view line : split(text, '\n'))
    {
        if (line.empty())
            continue;

        const std::size_t sep = line.find('=');
        if (sep == std::string_view::npos)
            return std::nullopt;

        const std::string_view key = line.substr(0, sep);
        const std::string_view value = line.substr(sep + 1);

        if (key == "version")
        {
            if (!parseInteger(value, state.version))
                return std::nullopt;
        }
        else if (key == "completed_cycles")
        {
            if (!parseInteger(value, state.completed_cycles))
                return std::nullopt;
        }
        else if (key == "last_mutation_cycle")
        {
            if (!parseInteger(value, state.last_mutation_cycle))
                return std::nullopt;
        }
        else if (key == "mutation_serial")
        {
            if (!parseInteger(value, state.mutation_serial))
                return std::nullopt;
        }
        else
        {
            return std::nullopt;
        }
    }

    state.version = kVariationStateVersion;
    return state;
}

}  // namespace downspout::counterpointer
