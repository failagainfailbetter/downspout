#ifndef DOWNSPOUT_AI_COORDINATOR_HPP_INCLUDED
#define DOWNSPOUT_AI_COORDINATOR_HPP_INCLUDED

#include "sidecar_core_types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace downspout::ai_coordinator {

inline constexpr int kMaxGuidePitchClasses = 16;

struct TuneState {
    int key = 0;
    std::string scale = "major";
    std::string genre = "jazz";
    int tempo = 120;
    int bars = 4;
    int beatsPerBar = 4;
    int channel = 1;
    int registerLow = 60;
    int registerHigh = 84;
    float density = 0.55f;
    float risk = 0.35f;
    std::uint32_t seed = 1;
    bool hasMidiContext = false;
    int guidePitchClassCount = 0;
    std::array<int, kMaxGuidePitchClasses> guidePitchClasses {};
};

[[nodiscard]] std::optional<TuneState> parseTuneStateJson(const std::string& text);
[[nodiscard]] std::string serializeTuneStateJson(const TuneState& state);
[[nodiscard]] std::optional<TuneState> loadTuneStateJson(const std::string& path);
[[nodiscard]] std::optional<TuneState> analyzeMidiFileToTuneState(const std::string& path);
[[nodiscard]] std::optional<downspout::sidecar::Phrase> parsePhraseResponseJson(const std::string& text);
[[nodiscard]] std::optional<downspout::sidecar::Phrase> loadPhraseResponseJson(const std::string& path);
[[nodiscard]] std::string buildSoloRequestJson(const TuneState& state);
[[nodiscard]] std::optional<downspout::sidecar::Phrase> requestOpenAiPhrase(const TuneState& state,
                                                                            std::string* rawResponse = nullptr,
                                                                            std::string* extractedText = nullptr);

[[nodiscard]] downspout::sidecar::Controls controlsFromTuneState(const TuneState& state);
[[nodiscard]] downspout::sidecar::Phrase generateSoloPhrase(const TuneState& state);

[[nodiscard]] bool writeMidiFile(const std::string& path,
                                 const downspout::sidecar::Phrase& phrase,
                                 int tempo,
                                 int channel,
                                 int beatsPerBar);

}  // namespace downspout::ai_coordinator

#endif
