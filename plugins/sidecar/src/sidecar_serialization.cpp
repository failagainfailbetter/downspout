#include "sidecar_serialization.hpp"

#include "sidecar_protocol.hpp"

#include <charconv>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>

namespace downspout::sidecar {
namespace {

template <typename T>
bool parseInteger(const std::string_view text, T& value)
{
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc() && result.ptr == end;
}

bool parseFloat(const std::string_view text, float& value)
{
    std::string local(text);
    char* parseEnd = nullptr;
    value = std::strtof(local.c_str(), &parseEnd);
    return parseEnd && *parseEnd == '\0';
}

}  // namespace

std::string serializePhrase(const Phrase& phrase)
{
    std::ostringstream out;
    out << "version=" << phrase.version << '\n';
    out << "bars=" << phrase.bars << '\n';
    out << "beatsPerBar=" << phrase.beatsPerBar << '\n';
    out << "eventCount=" << phrase.eventCount << '\n';
    for (int i = 0; i < phrase.eventCount; ++i)
    {
        const PhraseEvent& event = phrase.events[i];
        out << "event=" << event.beat << ','
            << event.duration << ','
            << event.note << ','
            << event.velocity << '\n';
    }
    return out.str();
}

std::optional<Phrase> deserializePhrase(const std::string& text)
{
    Phrase phrase {};
    int eventIndex = 0;
    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t end = text.find('\n', start);
        const std::string_view line = end == std::string::npos
            ? std::string_view(text).substr(start)
            : std::string_view(text).substr(start, end - start);
        start = end == std::string::npos ? text.size() + 1 : end + 1;
        if (line.empty())
            continue;

        const std::size_t sep = line.find('=');
        if (sep == std::string_view::npos)
            return std::nullopt;
        const std::string_view key = line.substr(0, sep);
        const std::string_view value = line.substr(sep + 1);

        int intValue = 0;
        if (key == "version")
        {
            if (!parseInteger(value, phrase.version))
                return std::nullopt;
        }
        else if (key == "bars")
        {
            if (!parseInteger(value, phrase.bars))
                return std::nullopt;
        }
        else if (key == "beatsPerBar")
        {
            if (!parseInteger(value, phrase.beatsPerBar))
                return std::nullopt;
        }
        else if (key == "eventCount")
        {
            if (!parseInteger(value, phrase.eventCount))
                return std::nullopt;
        }
        else if (key == "event")
        {
            if (eventIndex >= kMaxPhraseEvents)
                return std::nullopt;
            PhraseEvent event {};
            std::size_t fieldStart = 0;
            for (int field = 0; field < 4; ++field)
            {
                const std::size_t fieldEnd = value.find(',', fieldStart);
                const std::string_view fieldText = fieldEnd == std::string_view::npos
                    ? value.substr(fieldStart)
                    : value.substr(fieldStart, fieldEnd - fieldStart);
                if ((field == 0 && !parseFloat(fieldText, event.beat)) ||
                    (field == 1 && !parseFloat(fieldText, event.duration)) ||
                    (field == 2 && !parseInteger(fieldText, event.note)) ||
                    (field == 3 && !parseInteger(fieldText, event.velocity)))
                {
                    return std::nullopt;
                }
                fieldStart = fieldEnd == std::string_view::npos ? value.size() : fieldEnd + 1;
            }
            phrase.events[eventIndex++] = event;
        }
        else
        {
            return std::nullopt;
        }
    }

    if (phrase.eventCount != eventIndex)
        return std::nullopt;
    Controls validationControls {};
    validationControls.reg = RegisterId::custom;
    validationControls.registerLow = 0;
    validationControls.registerHigh = 127;
    if (!validatePhrase(phrase, validationControls).valid)
        return std::nullopt;
    return phrase;
}

}  // namespace downspout::sidecar
