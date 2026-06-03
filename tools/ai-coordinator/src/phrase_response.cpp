#include "ai_coordinator.hpp"

#include "sidecar_protocol.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>

namespace downspout::ai_coordinator {
namespace {

[[nodiscard]] std::optional<std::string_view> valueForKey(const std::string_view text, const std::string_view key)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = text.find(needle);
    if (keyPos == std::string_view::npos)
        return std::nullopt;
    const std::size_t colon = text.find(':', keyPos + needle.size());
    if (colon == std::string_view::npos)
        return std::nullopt;
    std::size_t start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
        ++start;
    if (start >= text.size())
        return std::nullopt;
    std::size_t end = start;
    if (text[start] == '"') {
        end = text.find('"', start + 1);
        if (end == std::string_view::npos)
            return std::nullopt;
        return text.substr(start + 1, end - start - 1);
    }
    while (end < text.size() && text[end] != ',' && text[end] != '}' && text[end] != ']')
        ++end;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return text.substr(start, end - start);
}

template <typename T>
bool parseInteger(const std::optional<std::string_view> value, T& target)
{
    if (!value.has_value())
        return false;
    T parsed {};
    const auto result = std::from_chars(value->data(), value->data() + value->size(), parsed);
    if (result.ec != std::errc() || result.ptr != value->data() + value->size())
        return false;
    target = parsed;
    return true;
}

bool parseFloat(const std::optional<std::string_view> value, float& target)
{
    if (!value.has_value())
        return false;
    std::string local(*value);
    char* end = nullptr;
    const float parsed = std::strtof(local.c_str(), &end);
    if (end == nullptr || *end != '\0')
        return false;
    target = parsed;
    return true;
}

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

std::optional<downspout::sidecar::Phrase> parsePhraseResponseJson(const std::string& text)
{
    downspout::sidecar::Phrase phrase {};
    parseInteger(valueForKey(text, "version"), phrase.version);
    parseInteger(valueForKey(text, "bars"), phrase.bars);
    parseInteger(valueForKey(text, "beats_per_bar"), phrase.beatsPerBar);
    parseInteger(valueForKey(text, "beatsPerBar"), phrase.beatsPerBar);
    phrase.version = downspout::sidecar::kPhraseStateVersion;
    phrase.bars = clampi(phrase.bars, 1, 8);
    phrase.beatsPerBar = clampi(phrase.beatsPerBar, 1, 12);

    const std::size_t eventsKey = text.find("\"events\"");
    if (eventsKey == std::string::npos)
        return std::nullopt;
    const std::size_t arrayOpen = text.find('[', eventsKey);
    const std::size_t arrayClose = arrayOpen == std::string::npos ? std::string::npos : text.find(']', arrayOpen);
    if (arrayOpen == std::string::npos || arrayClose == std::string::npos)
        return std::nullopt;

    std::size_t cursor = arrayOpen + 1;
    while (cursor < arrayClose && phrase.eventCount < downspout::sidecar::kMaxPhraseEvents) {
        const std::size_t objectOpen = text.find('{', cursor);
        if (objectOpen == std::string::npos || objectOpen >= arrayClose)
            break;
        const std::size_t objectClose = text.find('}', objectOpen);
        if (objectClose == std::string::npos || objectClose > arrayClose)
            return std::nullopt;

        const std::string_view object = std::string_view(text).substr(objectOpen, objectClose - objectOpen + 1);
        downspout::sidecar::PhraseEvent event {};
        if (!parseFloat(valueForKey(object, "beat"), event.beat) ||
            !parseFloat(valueForKey(object, "duration"), event.duration) ||
            !parseInteger(valueForKey(object, "note"), event.note) ||
            !parseInteger(valueForKey(object, "velocity"), event.velocity)) {
            return std::nullopt;
        }
        phrase.events[static_cast<std::size_t>(phrase.eventCount++)] = event;
        cursor = objectClose + 1;
    }

    downspout::sidecar::Controls validationControls {};
    validationControls.reg = downspout::sidecar::RegisterId::custom;
    validationControls.registerLow = 0;
    validationControls.registerHigh = 127;
    validationControls.bars = phrase.bars;
    if (!downspout::sidecar::validatePhrase(phrase, validationControls).valid)
        return std::nullopt;
    return phrase;
}

std::optional<downspout::sidecar::Phrase> loadPhraseResponseJson(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
        return std::nullopt;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parsePhraseResponseJson(buffer.str());
}

}  // namespace downspout::ai_coordinator
