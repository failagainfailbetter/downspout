#pragma once

#include "paunchlad_params.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace downspout::paunchlad {

struct MidiMessage {
    std::uint32_t frame = 0;
    std::uint8_t size = 0;
    std::array<std::uint8_t, 16> data {};
};

struct Status {
    float activity = 0.0f;
    float mode = 0.0f;
};

inline constexpr std::size_t kMaxProcessEvents = 256;

struct ProcessResult {
    std::array<MidiMessage, kMaxProcessEvents> events {};
    std::uint32_t eventCount = 0;
    Status status {};
};

enum class LaunchpadInputLayout {
    unknown,
    programmer,
    custom,
};

class Processor {
public:
    void init(double sampleRate);
    void activate();

    void setParameter(std::uint32_t index, float value);
    [[nodiscard]] float getParameter(std::uint32_t index) const noexcept;
    [[nodiscard]] const Status& getStatus() const noexcept;

    ProcessResult processBlock(const float* inLeft,
                               const float* inRight,
                               float* outLeft,
                               float* outRight,
                               std::uint32_t frameCount,
                               const MidiMessage* inputEvents,
                               std::uint32_t inputEventCount);

private:
    void resetToDefaults();
    void clearPerformance();
    bool handleMidi(const MidiMessage& message);
    bool handleGridPress(std::uint8_t note, std::uint8_t velocity);
    bool handleTopButton(std::uint8_t cc);
    bool handleSideButton(std::uint8_t cc);
    void triggerPad(std::size_t row, std::size_t col, float strength);
    void processAudio(const float* inLeft,
                      const float* inRight,
                      float* outLeft,
                      float* outRight,
                      std::uint32_t frameCount);
    void emitLedFeedback(ProcessResult& result, bool force);
    void appendMidi(ProcessResult& result,
                    std::uint32_t frame,
                    std::uint8_t status,
                    std::uint8_t data1,
                    std::uint8_t data2);
    void appendSysex(ProcessResult& result, const std::uint8_t* data, std::uint8_t size);
    [[nodiscard]] std::uint8_t colorForPad(std::size_t row, std::size_t col, bool active) const noexcept;
    [[nodiscard]] std::uint32_t framesForBeatDivision(float beats) const noexcept;
    [[nodiscard]] std::uint32_t random();
    [[nodiscard]] float noise();
    [[nodiscard]] float parameterDefault(std::uint32_t index) const noexcept;
    void decayCellFlashes(std::uint32_t frameCount);

    double sampleRate_ = 48000.0;
    std::array<float, kParameterCount> parameters_ {};
    std::array<float, kCellCount> cellFlash_ {};
    std::array<std::uint8_t, kCellCount> lastCellLeds_ {};
    std::array<std::uint8_t, 9> lastTopLeds_ {};
    std::array<std::uint8_t, 8> lastSideLeds_ {};
    std::vector<float> delayLeft_ {};
    std::vector<float> delayRight_ {};
    std::size_t delayWrite_ = 0;
    float delaySeconds_ = 0.34f;
    float delayToneLeft_ = 0.0f;
    float delayToneRight_ = 0.0f;
    float sirenPhase_ = 0.0f;
    float sirenLfo_ = 0.0f;
    float sirenEnv_ = 0.0f;
    float sirenSweep_ = 0.0f;
    int sirenMode_ = 0;
    float alarmPhase_ = 0.0f;
    float alarmEnv_ = 0.0f;
    float alarmRate_ = 3.5f;
    int alarmMode_ = 0;
    float snareEnv_ = 0.0f;
    float snareBodyPhase_ = 0.0f;
    float snareBodyHz_ = 180.0f;
    float springEnv_ = 0.0f;
    float springState_ = 0.0f;
    float throwSend_ = 0.0f;
    float throwFeedback_ = 0.0f;
    std::uint32_t throwFrames_ = 0;
    std::uint32_t dropFrames_ = 0;
    std::uint32_t chopFrames_ = 0;
    std::uint32_t freezeFrames_ = 0;
    std::uint32_t ledRefreshSamples_ = 0;
    std::uint32_t randomState_ = 0x61c88647u;
    bool ledInitialized_ = false;
    LaunchpadInputLayout inputLayout_ = LaunchpadInputLayout::unknown;
    Status status_ {};
};

} // namespace downspout::paunchlad
