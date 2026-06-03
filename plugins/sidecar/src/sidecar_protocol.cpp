#include "sidecar_protocol.hpp"

#include <algorithm>
#include <cmath>

namespace downspout::sidecar {
namespace {

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] float clampf(const float value, const float minimum, const float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] std::uint32_t nextRandom(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

[[nodiscard]] float randomUnit(std::uint32_t& state)
{
    return static_cast<float>((nextRandom(state) >> 8u) & 0xffffu) / 65535.0f;
}

void resolveRegister(const Controls& controls, int& low, int& high)
{
    switch (controls.reg)
    {
    case RegisterId::low:
        low = 48;
        high = 67;
        break;
    case RegisterId::mid:
        low = 55;
        high = 82;
        break;
    case RegisterId::high:
        low = 67;
        high = 94;
        break;
    case RegisterId::custom:
    case RegisterId::count:
        low = controls.registerLow;
        high = controls.registerHigh;
        break;
    }
    low = clampi(low, 0, 127);
    high = clampi(high, low, 127);
}

}  // namespace

Controls clampControls(const Controls& raw)
{
    Controls controls = raw;
    controls.channel = clampi(controls.channel, 1, 16);
    controls.bars = clampi(controls.bars, 1, 8);
    controls.reg = static_cast<RegisterId>(clampi(static_cast<int>(controls.reg), 0, static_cast<int>(RegisterId::count) - 1));
    controls.registerLow = clampi(controls.registerLow, 0, 127);
    controls.registerHigh = clampi(controls.registerHigh, controls.registerLow, 127);
    controls.density = clampf(controls.density, 0.0f, 1.0f);
    controls.risk = clampf(controls.risk, 0.0f, 1.0f);
    controls.humanize = clampf(controls.humanize, 0.0f, 1.0f);
    return controls;
}

ValidationResult validatePhrase(const Phrase& phrase, const Controls& rawControls)
{
    const Controls controls = clampControls(rawControls);
    ValidationResult result {};
    if (phrase.eventCount < 0 || phrase.eventCount > kMaxPhraseEvents || phrase.bars < 1 || phrase.beatsPerBar < 1)
    {
        result.valid = false;
        result.firstInvalidEvent = 0;
        return result;
    }

    int low = 0;
    int high = 0;
    resolveRegister(controls, low, high);
    const float maxBeat = static_cast<float>(phrase.bars * phrase.beatsPerBar);
    float previousEnd = -1.0f;

    for (int i = 0; i < phrase.eventCount; ++i)
    {
        const PhraseEvent& event = phrase.events[i];
        const float end = event.beat + event.duration;
        if (!std::isfinite(event.beat) || !std::isfinite(event.duration) ||
            event.beat < 0.0f || event.duration <= 0.0f || end > maxBeat + 0.0001f ||
            event.note < low || event.note > high ||
            event.velocity < 1 || event.velocity > 127 ||
            event.beat < previousEnd - 0.0001f)
        {
            result.valid = false;
            result.firstInvalidEvent = i;
            return result;
        }
        previousEnd = end;
    }

    return result;
}

Phrase makeFallbackPhrase(const Controls& rawControls, std::uint32_t seed)
{
    const Controls controls = clampControls(rawControls);
    Phrase phrase {};
    phrase.bars = controls.bars;
    phrase.beatsPerBar = 4;

    int low = 0;
    int high = 0;
    resolveRegister(controls, low, high);

    const int totalBeats = phrase.bars * phrase.beatsPerBar;
    const int targetEvents = clampi(static_cast<int>(std::lround(static_cast<float>(totalBeats) * (0.35f + controls.density * 0.65f))),
                                    1,
                                    std::min(kMaxPhraseEvents, totalBeats * 2));
    const int baseNote = clampi((low + high) / 2, low, high);
    const int scale[] = {0, 2, 3, 5, 7, 9, 10, 12};

    float beat = 0.0f;
    while (phrase.eventCount < targetEvents && beat < static_cast<float>(totalBeats) - 0.25f)
    {
        const float gap = randomUnit(seed) < controls.density ? 0.5f : 1.0f;
        const float duration = randomUnit(seed) < 0.25f + controls.risk * 0.35f ? 0.25f : 0.5f;
        const int degree = static_cast<int>(nextRandom(seed) % 8u);
        const int outside = randomUnit(seed) < controls.risk * 0.18f ? (randomUnit(seed) < 0.5f ? -1 : 1) : 0;

        PhraseEvent& event = phrase.events[phrase.eventCount++];
        event.beat = beat;
        event.duration = std::min(duration, static_cast<float>(totalBeats) - beat);
        event.note = clampi(baseNote + scale[degree] + outside, low, high);
        event.velocity = clampi(72 + static_cast<int>(std::lround(randomUnit(seed) * 32.0f)), 1, 127);

        beat += gap;
    }

    return phrase;
}

}  // namespace downspout::sidecar
