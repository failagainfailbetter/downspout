#include "ai_coordinator.hpp"

#include "sidecar_protocol.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

namespace downspout::ai_coordinator {
namespace {

[[nodiscard]] std::uint32_t nextRandom(std::uint32_t& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

[[nodiscard]] float randomUnit(std::uint32_t& seed)
{
    return static_cast<float>((nextRandom(seed) >> 8u) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] std::string lowercase(std::string text)
{
    for (char& ch : text)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

[[nodiscard]] std::array<int, 8> scaleIntervals(const std::string& scale, int& count)
{
    const std::string name = lowercase(scale);
    if (name == "minor" || name == "natural_minor" || name == "nat minor") {
        count = 7;
        return {0, 2, 3, 5, 7, 8, 10, 0};
    }
    if (name == "dorian") {
        count = 7;
        return {0, 2, 3, 5, 7, 9, 10, 0};
    }
    if (name == "mixolydian") {
        count = 7;
        return {0, 2, 4, 5, 7, 9, 10, 0};
    }
    if (name == "phrygian") {
        count = 7;
        return {0, 1, 3, 5, 7, 8, 10, 0};
    }
    if (name == "blues") {
        count = 6;
        return {0, 3, 5, 6, 7, 10, 0, 0};
    }
    if (name == "chromatic") {
        count = 8;
        return {0, 1, 2, 3, 4, 5, 6, 7};
    }
    count = 7;
    return {0, 2, 4, 5, 7, 9, 11, 0};
}

[[nodiscard]] int nearestInRegister(const int pitchClass, const int low, const int high, const int target)
{
    int best = low;
    int bestDistance = 1024;
    for (int note = low; note <= high; ++note) {
        if (note % 12 != pitchClass)
            continue;
        const int distance = std::abs(note - target);
        if (distance < bestDistance) {
            best = note;
            bestDistance = distance;
        }
    }
    return best;
}

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

downspout::sidecar::Controls controlsFromTuneState(const TuneState& state)
{
    downspout::sidecar::Controls controls {};
    controls.channel = state.channel;
    controls.bars = state.bars;
    controls.reg = downspout::sidecar::RegisterId::custom;
    controls.registerLow = state.registerLow;
    controls.registerHigh = state.registerHigh;
    controls.density = state.density;
    controls.risk = state.risk;
    return downspout::sidecar::clampControls(controls);
}

downspout::sidecar::Phrase generateSoloPhrase(const TuneState& state)
{
    const downspout::sidecar::Controls controls = controlsFromTuneState(state);
    downspout::sidecar::Phrase phrase {};
    phrase.version = downspout::sidecar::kPhraseStateVersion;
    phrase.bars = controls.bars;
    phrase.beatsPerBar = clampi(state.beatsPerBar, 1, 12);

    int intervalCount = 0;
    const std::array<int, 8> intervals = scaleIntervals(state.scale, intervalCount);
    const std::array<int, 16> melodicShape = {0, 2, 4, 5, 4, 2, 1, 3, 5, 6, 4, 2, 0, 1, 2, 4};
    const int totalBeats = phrase.bars * phrase.beatsPerBar;
    const float grid = controls.density > 0.70f ? 0.5f : 1.0f;
    const int maxEvents = std::min<int>(downspout::sidecar::kMaxPhraseEvents,
                                        static_cast<int>(std::ceil(static_cast<float>(totalBeats) / grid)));
    const int targetEvents = clampi(static_cast<int>(std::round(static_cast<float>(maxEvents) * (0.20f + controls.density * 0.80f))),
                                    1,
                                    maxEvents);
    const int center = (controls.registerLow + controls.registerHigh) / 2;
    std::uint32_t seed = state.seed == 0 ? 1u : state.seed;

    float beat = 0.0f;
    int emitted = 0;
    while (emitted < targetEvents && beat < static_cast<float>(totalBeats) - 0.001f) {
        const bool rest = emitted > 0 && randomUnit(seed) > (0.55f + controls.density * 0.40f);
        if (rest) {
            beat += grid;
            continue;
        }

        const int shapeIndex = (emitted + static_cast<int>(std::floor(beat))) % static_cast<int>(melodicShape.size());
        int scaleDegree = melodicShape[static_cast<std::size_t>(shapeIndex)] % intervalCount;
        if (randomUnit(seed) < controls.risk * 0.45f)
            scaleDegree = (scaleDegree + 2 + static_cast<int>(nextRandom(seed) % 3u)) % intervalCount;

        const int octaveLift = (emitted % 7 == 5 && randomUnit(seed) < controls.risk) ? 12 : 0;
        int pitchClass = (state.key + intervals[static_cast<std::size_t>(scaleDegree)] + 120) % 12;
        if (state.guidePitchClassCount > 0 && (emitted % 4 == 0 || randomUnit(seed) < controls.risk * 0.55f)) {
            const int guideIndex = static_cast<int>(nextRandom(seed) % static_cast<std::uint32_t>(state.guidePitchClassCount));
            pitchClass = ((state.guidePitchClasses[static_cast<std::size_t>(guideIndex)] % 12) + 12) % 12;
        }
        int note = nearestInRegister(pitchClass, controls.registerLow, controls.registerHigh, center + octaveLift);
        if (randomUnit(seed) < controls.risk * 0.25f)
            note = nearestInRegister((pitchClass + 1) % 12, controls.registerLow, controls.registerHigh, note);

        downspout::sidecar::PhraseEvent& event = phrase.events[static_cast<std::size_t>(phrase.eventCount++)];
        event.beat = beat;
        event.duration = grid <= 0.5f ? 0.45f : 0.80f;
        event.note = note;
        event.velocity = clampi(78 + static_cast<int>(nextRandom(seed) % 24u), 1, 127);

        ++emitted;
        beat += grid;
    }

    if (!downspout::sidecar::validatePhrase(phrase, controls).valid)
        return downspout::sidecar::makeFallbackPhrase(controls, seed);
    return phrase;
}

}  // namespace downspout::ai_coordinator
