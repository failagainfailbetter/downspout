#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace downspout::paunchlad {

inline constexpr std::size_t kGridWidth = 8;
inline constexpr std::size_t kGridHeight = 8;
inline constexpr std::size_t kCellCount = kGridWidth * kGridHeight;

struct ParamSpec {
    const char* symbol;
    const char* name;
    float minimum;
    float maximum;
    float defaultValue;
    bool integer = false;
    bool boolean = false;
    bool trigger = false;
    bool output = false;
};

inline constexpr std::uint32_t kParamCellStart = 0;
inline constexpr std::uint32_t kParamDry = 64;
inline constexpr std::uint32_t kParamWet = 65;
inline constexpr std::uint32_t kParamFeedback = 66;
inline constexpr std::uint32_t kParamTone = 67;
inline constexpr std::uint32_t kParamSirenLevel = 68;
inline constexpr std::uint32_t kParamSpring = 69;
inline constexpr std::uint32_t kParamOutput = 70;
inline constexpr std::uint32_t kParamLedFeedback = 71;
inline constexpr std::uint32_t kParamPadMap = 72;
inline constexpr std::uint32_t kParamPanic = 73;
inline constexpr std::uint32_t kParamStatusActivity = 74;
inline constexpr std::uint32_t kParamStatusMode = 75;
inline constexpr std::uint32_t kParamStatusCellStart = 76;
inline constexpr std::uint32_t kParameterCount = kParamStatusCellStart + static_cast<std::uint32_t>(kCellCount);

inline constexpr std::array<ParamSpec, 8> kControlParamSpecs = {{
    {"dry", "Dry", 0.0f, 1.0f, 0.90f, false, false, false, false},
    {"wet", "Echo Wet", 0.0f, 1.0f, 0.42f, false, false, false, false},
    {"feedback", "Feedback", 0.0f, 1.0f, 0.34f, false, false, false, false},
    {"tone", "Tape Tone", 0.0f, 1.0f, 0.42f, false, false, false, false},
    {"siren_level", "Siren", 0.0f, 1.0f, 0.50f, false, false, false, false},
    {"spring", "Spring", 0.0f, 1.0f, 0.35f, false, false, false, false},
    {"output", "Output", 0.0f, 1.0f, 0.78f, false, false, false, false},
    {"led_feedback", "LED Feedback", 0.0f, 1.0f, 1.0f, true, true, false, false},
}};

inline constexpr std::array<std::uint8_t, 9> kTopButtonCCs = {{
    91, 92, 93, 94, 95, 96, 97, 98, 99,
}};

inline constexpr std::array<std::uint8_t, 8> kSideButtonCCs = {{
    19, 29, 39, 49, 59, 69, 79, 89,
}};

inline constexpr std::uint8_t kLedOff = 0;
inline constexpr std::uint8_t kLedDim = 1;
inline constexpr std::uint8_t kLedWhite = 3;
inline constexpr std::uint8_t kLedRed = 5;
inline constexpr std::uint8_t kLedOrange = 9;
inline constexpr std::uint8_t kLedYellow = 14;
inline constexpr std::uint8_t kLedGreen = 21;
inline constexpr std::uint8_t kLedCyan = 37;
inline constexpr std::uint8_t kLedBlue = 45;
inline constexpr std::uint8_t kLedPurple = 53;
inline constexpr std::uint8_t kLedPink = 57;

[[nodiscard]] constexpr std::uint8_t gridToNote(const std::size_t row, const std::size_t col) noexcept
{
    return row < kGridHeight && col < kGridWidth
        ? static_cast<std::uint8_t>((row + 1u) * 10u + (col + 1u))
        : 0u;
}

[[nodiscard]] constexpr bool noteToGrid(const std::uint8_t note, std::size_t& row, std::size_t& col) noexcept
{
    if (note < 11u || note > 88u)
        return false;

    const std::uint8_t tens = static_cast<std::uint8_t>(note / 10u);
    const std::uint8_t ones = static_cast<std::uint8_t>(note % 10u);
    if (tens < 1u || tens > 8u || ones < 1u || ones > 8u)
        return false;

    row = static_cast<std::size_t>(tens - 1u);
    col = static_cast<std::size_t>(ones - 1u);
    return true;
}

[[nodiscard]] constexpr bool noteToDefaultCustomGrid(const std::uint8_t note, std::size_t& row, std::size_t& col) noexcept
{
    if (note >= 36u && note <= 67u)
    {
        const std::uint8_t offset = static_cast<std::uint8_t>(note - 36u);
        row = static_cast<std::size_t>(offset / 4u);
        col = static_cast<std::size_t>(offset % 4u);
        return row < kGridHeight;
    }

    if (note >= 68u && note <= 99u)
    {
        const std::uint8_t offset = static_cast<std::uint8_t>(note - 68u);
        row = static_cast<std::size_t>(offset / 4u);
        col = 4u + static_cast<std::size_t>(offset % 4u);
        return row < kGridHeight;
    }

    return false;
}

[[nodiscard]] constexpr bool noteToLinearChromaticGrid(const std::uint8_t note, std::size_t& row, std::size_t& col) noexcept
{
    if (note < 36u || note > 99u)
        return false;

    const std::uint8_t offset = static_cast<std::uint8_t>(note - 36u);
    row = static_cast<std::size_t>(offset / 8u);
    col = static_cast<std::size_t>(offset % 8u);
    return row < kGridHeight;
}

constexpr void applyPadMap(const std::uint32_t map, std::size_t& row, std::size_t& col) noexcept
{
    const std::size_t r = row;
    const std::size_t c = col;
    switch (map % 4u)
    {
    case 1u:
        row = c;
        col = kGridWidth - 1u - r;
        break;
    case 2u:
        row = kGridHeight - 1u - r;
        col = kGridWidth - 1u - c;
        break;
    case 3u:
        row = kGridHeight - 1u - c;
        col = r;
        break;
    default:
        break;
    }
}

constexpr void unapplyPadMap(const std::uint32_t map, std::size_t& row, std::size_t& col) noexcept
{
    const std::size_t r = row;
    const std::size_t c = col;
    switch (map % 4u)
    {
    case 1u:
        row = kGridHeight - 1u - c;
        col = r;
        break;
    case 2u:
        row = kGridHeight - 1u - r;
        col = kGridWidth - 1u - c;
        break;
    case 3u:
        row = c;
        col = kGridWidth - 1u - r;
        break;
    default:
        break;
    }
}

[[nodiscard]] constexpr std::size_t cellIndex(const std::size_t row, const std::size_t col) noexcept
{
    return row * kGridWidth + col;
}

} // namespace downspout::paunchlad
