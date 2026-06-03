#include "ai_coordinator.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>

namespace downspout::ai_coordinator {
namespace {

[[nodiscard]] std::string lowercase(std::string text)
{
    for (char& ch : text)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

[[nodiscard]] int keyNameToSemitone(const std::string& text)
{
    const std::string key = lowercase(text);
    if (key == "c") return 0;
    if (key == "c#" || key == "db") return 1;
    if (key == "d") return 2;
    if (key == "d#" || key == "eb") return 3;
    if (key == "e" || key == "fb") return 4;
    if (key == "f" || key == "e#") return 5;
    if (key == "f#" || key == "gb") return 6;
    if (key == "g") return 7;
    if (key == "g#" || key == "ab") return 8;
    if (key == "a") return 9;
    if (key == "a#" || key == "bb") return 10;
    if (key == "b" || key == "cb") return 11;
    return 0;
}

[[nodiscard]] std::optional<std::string_view> valueForKey(const std::string& text, const std::string_view key)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = text.find(needle);
    if (keyPos == std::string::npos)
        return std::nullopt;

    const std::size_t colon = text.find(':', keyPos + needle.size());
    if (colon == std::string::npos)
        return std::nullopt;

    std::size_t start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
        ++start;
    if (start >= text.size())
        return std::nullopt;

    if (text[start] == '"') {
        const std::size_t end = text.find('"', start + 1);
        if (end == std::string::npos)
            return std::nullopt;
        return std::string_view(text).substr(start + 1, end - start - 1);
    }

    std::size_t end = start;
    while (end < text.size() && text[end] != ',' && text[end] != '}' && text[end] != '\n' && text[end] != '\r')
        ++end;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return std::string_view(text).substr(start, end - start);
}

template <typename T>
void readNumber(const std::string& text, const std::string_view key, T& target)
{
    const auto value = valueForKey(text, key);
    if (!value.has_value())
        return;

    T parsed {};
    const char* begin = value->data();
    const char* end = value->data() + value->size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec == std::errc() && result.ptr == end)
        target = parsed;
}

void readFloat(const std::string& text, const std::string_view key, float& target)
{
    const auto value = valueForKey(text, key);
    if (!value.has_value())
        return;

    std::string local(*value);
    char* parseEnd = nullptr;
    const float parsed = std::strtof(local.c_str(), &parseEnd);
    if (parseEnd != nullptr && *parseEnd == '\0')
        target = parsed;
}

void readString(const std::string& text, const std::string_view key, std::string& target)
{
    const auto value = valueForKey(text, key);
    if (value.has_value())
        target = std::string(*value);
}

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] float clampf(const float value, const float minimum, const float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

std::optional<TuneState> parseTuneStateJson(const std::string& text)
{
    if (text.find('{') == std::string::npos || text.find('}') == std::string::npos)
        return std::nullopt;

    TuneState state {};
    if (const auto keyText = valueForKey(text, "key"); keyText.has_value()) {
        const bool quoted = keyText->data() > text.data() && *(keyText->data() - 1) == '"';
        if (quoted) {
            state.key = keyNameToSemitone(std::string(*keyText));
        } else {
            int parsed = state.key;
            const char* begin = keyText->data();
            const char* end = keyText->data() + keyText->size();
            const auto result = std::from_chars(begin, end, parsed);
            if (result.ec == std::errc() && result.ptr == end)
                state.key = parsed;
        }
    }

    readString(text, "scale", state.scale);
    readString(text, "genre", state.genre);
    readNumber(text, "tempo", state.tempo);
    readNumber(text, "bars", state.bars);
    readNumber(text, "beats_per_bar", state.beatsPerBar);
    readNumber(text, "beatsPerBar", state.beatsPerBar);
    readNumber(text, "channel", state.channel);
    readNumber(text, "register_low", state.registerLow);
    readNumber(text, "registerLow", state.registerLow);
    readNumber(text, "register_high", state.registerHigh);
    readNumber(text, "registerHigh", state.registerHigh);
    readFloat(text, "density", state.density);
    readFloat(text, "risk", state.risk);
    readNumber(text, "seed", state.seed);
    if (const std::size_t guides = text.find("\"guide_pitch_classes\""); guides != std::string::npos) {
        const std::size_t open = text.find('[', guides);
        const std::size_t close = open == std::string::npos ? std::string::npos : text.find(']', open);
        if (open != std::string::npos && close != std::string::npos) {
            std::size_t cursor = open + 1;
            while (cursor < close && state.guidePitchClassCount < kMaxGuidePitchClasses) {
                while (cursor < close && (std::isspace(static_cast<unsigned char>(text[cursor])) || text[cursor] == ','))
                    ++cursor;
                std::size_t end = cursor;
                while (end < close && text[end] != ',')
                    ++end;
                int pc = 0;
                const auto result = std::from_chars(text.data() + cursor, text.data() + end, pc);
                if (result.ec == std::errc()) {
                    state.guidePitchClasses[static_cast<std::size_t>(state.guidePitchClassCount++)] = ((pc % 12) + 12) % 12;
                }
                cursor = end + 1;
            }
        }
    }

    state.key = clampi(state.key, 0, 11);
    state.tempo = clampi(state.tempo, 30, 300);
    state.bars = clampi(state.bars, 1, 8);
    state.beatsPerBar = clampi(state.beatsPerBar, 1, 12);
    state.channel = clampi(state.channel, 1, 16);
    state.registerLow = clampi(state.registerLow, 0, 127);
    state.registerHigh = clampi(state.registerHigh, 0, 127);
    if (state.registerHigh < state.registerLow)
        std::swap(state.registerHigh, state.registerLow);
    state.density = clampf(state.density, 0.0f, 1.0f);
    state.risk = clampf(state.risk, 0.0f, 1.0f);
    if (state.scale.empty())
        state.scale = "major";
    if (state.genre.empty())
        state.genre = "jazz";
    state.hasMidiContext = state.guidePitchClassCount > 0 || state.hasMidiContext;

    return state;
}

std::string serializeTuneStateJson(const TuneState& rawState)
{
    TuneState state = rawState;
    state.key = clampi(state.key, 0, 11);
    state.tempo = clampi(state.tempo, 30, 300);
    state.bars = clampi(state.bars, 1, 8);
    state.beatsPerBar = clampi(state.beatsPerBar, 1, 12);
    state.channel = clampi(state.channel, 1, 16);
    state.registerLow = clampi(state.registerLow, 0, 127);
    state.registerHigh = clampi(state.registerHigh, 0, 127);
    if (state.registerHigh < state.registerLow)
        std::swap(state.registerHigh, state.registerLow);
    state.density = clampf(state.density, 0.0f, 1.0f);
    state.risk = clampf(state.risk, 0.0f, 1.0f);
    state.guidePitchClassCount = clampi(state.guidePitchClassCount, 0, kMaxGuidePitchClasses);

    std::ostringstream out;
    out << "{\n";
    out << "  \"key\": " << state.key << ",\n";
    out << "  \"scale\": \"" << state.scale << "\",\n";
    out << "  \"genre\": \"" << state.genre << "\",\n";
    out << "  \"tempo\": " << state.tempo << ",\n";
    out << "  \"bars\": " << state.bars << ",\n";
    out << "  \"beats_per_bar\": " << state.beatsPerBar << ",\n";
    out << "  \"channel\": " << state.channel << ",\n";
    out << "  \"register_low\": " << state.registerLow << ",\n";
    out << "  \"register_high\": " << state.registerHigh << ",\n";
    out << "  \"density\": " << state.density << ",\n";
    out << "  \"risk\": " << state.risk << ",\n";
    out << "  \"seed\": " << state.seed << ",\n";
    out << "  \"midi_context\": " << (state.hasMidiContext ? "true" : "false") << ",\n";
    out << "  \"guide_pitch_classes\": [";
    for (int i = 0; i < state.guidePitchClassCount; ++i) {
        if (i > 0)
            out << ", ";
        out << state.guidePitchClasses[static_cast<std::size_t>(i)];
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}

std::optional<TuneState> loadTuneStateJson(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
        return std::nullopt;

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseTuneStateJson(buffer.str());
}

}  // namespace downspout::ai_coordinator
