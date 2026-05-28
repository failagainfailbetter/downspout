#include "paunchlad_processor.hpp"

#include <algorithm>
#include <cmath>

namespace downspout::paunchlad {
namespace {

template <typename T>
[[nodiscard]] T clampValue(const T value, const T minimum, const T maximum) noexcept
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] int findIndex(const std::uint8_t value, const std::uint8_t* data, const std::size_t count) noexcept
{
    for (std::size_t i = 0; i < count; ++i)
    {
        if (data[i] == value)
            return static_cast<int>(i);
    }
    return -1;
}

constexpr float kPi = 3.14159265358979323846f;
constexpr std::array<std::uint8_t, 9> kProgrammerModeSysex = {{
    0xf0, 0x00, 0x20, 0x29, 0x02, 0x0d, 0x0e, 0x01, 0xf7,
}};

} // namespace

void Processor::init(const double sampleRate)
{
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
    const std::size_t delaySize = static_cast<std::size_t>(std::ceil(sampleRate_ * 4.0));
    delayLeft_.assign(std::max<std::size_t>(delaySize, 8u), 0.0f);
    delayRight_.assign(std::max<std::size_t>(delaySize, 8u), 0.0f);
    activate();
}

void Processor::activate()
{
    resetToDefaults();
}

void Processor::resetToDefaults()
{
    for (std::uint32_t i = 0; i < kParameterCount; ++i)
        parameters_[i] = parameterDefault(i);
    clearPerformance();
    ledInitialized_ = false;
    ledRefreshSamples_ = 0;
}

void Processor::clearPerformance()
{
    cellFlash_.fill(0.0f);
    lastCellLeds_.fill(255u);
    lastTopLeds_.fill(255u);
    lastSideLeds_.fill(255u);
    std::fill(delayLeft_.begin(), delayLeft_.end(), 0.0f);
    std::fill(delayRight_.begin(), delayRight_.end(), 0.0f);
    delayWrite_ = 0;
    delayToneLeft_ = 0.0f;
    delayToneRight_ = 0.0f;
    sirenPhase_ = 0.0f;
    sirenLfo_ = 0.0f;
    sirenEnv_ = 0.0f;
    sirenSweep_ = 0.0f;
    springEnv_ = 0.0f;
    springState_ = 0.0f;
    throwSend_ = 0.0f;
    throwFeedback_ = 0.0f;
    throwFrames_ = 0;
    dropFrames_ = 0;
    chopFrames_ = 0;
    freezeFrames_ = 0;
    status_ = {};
}

float Processor::parameterDefault(const std::uint32_t index) const noexcept
{
    if (index < kCellCount)
        return 0.0f;

    switch (index)
    {
    case kParamDry: return kControlParamSpecs[0].defaultValue;
    case kParamWet: return kControlParamSpecs[1].defaultValue;
    case kParamFeedback: return kControlParamSpecs[2].defaultValue;
    case kParamTone: return kControlParamSpecs[3].defaultValue;
    case kParamSirenLevel: return kControlParamSpecs[4].defaultValue;
    case kParamSpring: return kControlParamSpecs[5].defaultValue;
    case kParamOutput: return kControlParamSpecs[6].defaultValue;
    case kParamLedFeedback: return kControlParamSpecs[7].defaultValue;
    default: return 0.0f;
    }
}

void Processor::setParameter(const std::uint32_t index, float value)
{
    if (index >= kParameterCount || index >= kParamStatusCellStart)
        return;

    if (index < kCellCount)
    {
        if (value > 0.5f)
            triggerPad(index / kGridWidth, index % kGridWidth, 1.0f);
        parameters_[index] = 0.0f;
        return;
    }

    if (index == kParamPanic)
    {
        if (value > 0.5f)
            clearPerformance();
        parameters_[index] = 0.0f;
        return;
    }

    if (index == kParamLedFeedback)
        value = value >= 0.5f ? 1.0f : 0.0f;
    else
        value = clampValue(value, 0.0f, 1.0f);

    parameters_[index] = value;
}

float Processor::getParameter(const std::uint32_t index) const noexcept
{
    if (index == kParamStatusActivity)
        return status_.activity;
    if (index == kParamStatusMode)
        return status_.mode;
    if (index >= kParamStatusCellStart && index < kParamStatusCellStart + kCellCount)
        return cellFlash_[index - kParamStatusCellStart] > 0.02f ? 1.0f : 0.0f;
    return index < kParameterCount ? parameters_[index] : 0.0f;
}

const Status& Processor::getStatus() const noexcept
{
    return status_;
}

ProcessResult Processor::processBlock(const float* inLeft,
                                      const float* inRight,
                                      float* outLeft,
                                      float* outRight,
                                      const std::uint32_t frameCount,
                                      const MidiMessage* inputEvents,
                                      const std::uint32_t inputEventCount)
{
    ProcessResult result {};
    bool forceLeds = false;

    for (std::uint32_t i = 0; i < inputEventCount; ++i)
    {
        if (handleMidi(inputEvents[i]))
            forceLeds = true;
    }

    processAudio(inLeft, inRight, outLeft, outRight, frameCount);
    decayCellFlashes(frameCount);

    const bool periodicRefresh = ledRefreshSamples_ >= static_cast<std::uint32_t>(sampleRate_ * 0.75);
    emitLedFeedback(result, forceLeds || periodicRefresh || !ledInitialized_);
    ledRefreshSamples_ = periodicRefresh ? 0u : ledRefreshSamples_ + frameCount;
    result.status = status_;
    return result;
}

bool Processor::handleMidi(const MidiMessage& message)
{
    if (message.size < 3)
        return false;

    const std::uint8_t status = message.data[0] & 0xf0u;
    const std::uint8_t data1 = message.data[1];
    const std::uint8_t data2 = message.data[2];

    if (status == 0x90u && data2 > 0u)
        return handleGridPress(data1, data2);
    if (status == 0xb0u && data2 > 0u)
        return handleTopButton(data1) || handleSideButton(data1);
    return false;
}

bool Processor::handleGridPress(const std::uint8_t note, const std::uint8_t velocity)
{
    std::size_t row = 0;
    std::size_t col = 0;
    if (!noteToGrid(note, row, col))
        return false;

    triggerPad(row, col, static_cast<float>(velocity) / 127.0f);
    return true;
}

bool Processor::handleTopButton(const std::uint8_t cc)
{
    const int index = findIndex(cc, kTopButtonCCs.data(), kTopButtonCCs.size());
    if (index < 0)
        return false;

    switch (index)
    {
    case 0: parameters_[kParamWet] = clampValue(parameters_[kParamWet] - 0.08f, 0.0f, 1.0f); break;
    case 1: parameters_[kParamWet] = clampValue(parameters_[kParamWet] + 0.08f, 0.0f, 1.0f); break;
    case 2: parameters_[kParamFeedback] = clampValue(parameters_[kParamFeedback] - 0.08f, 0.0f, 1.0f); break;
    case 3: parameters_[kParamFeedback] = clampValue(parameters_[kParamFeedback] + 0.08f, 0.0f, 1.0f); break;
    case 4: parameters_[kParamTone] = clampValue(parameters_[kParamTone] - 0.10f, 0.0f, 1.0f); break;
    case 5: parameters_[kParamTone] = clampValue(parameters_[kParamTone] + 0.10f, 0.0f, 1.0f); break;
    default: clearPerformance(); break;
    }

    status_.activity = 1.0f;
    return true;
}

bool Processor::handleSideButton(const std::uint8_t cc)
{
    const int index = findIndex(cc, kSideButtonCCs.data(), kSideButtonCCs.size());
    if (index < 0)
        return false;

    parameters_[kParamSirenLevel] = static_cast<float>(index + 1) / 8.0f;
    status_.activity = 1.0f;
    return true;
}

void Processor::triggerPad(const std::size_t row, const std::size_t col, const float strength)
{
    const std::size_t index = cellIndex(row, col);
    cellFlash_[index] = 1.0f;
    status_.activity = 1.0f;
    status_.mode = static_cast<float>(row);

    if (row <= 1)
    {
        static constexpr std::array<float, 8> kBeatLengths = {{
            0.25f, 0.50f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f,
        }};
        throwFrames_ = framesForBeatDivision(kBeatLengths[col]);
        throwSend_ = clampValue(0.45f + strength * 0.55f, 0.0f, 1.0f);
        throwFeedback_ = clampValue(0.18f + static_cast<float>(col) * 0.08f, 0.0f, 0.86f);
        return;
    }

    if (row <= 3)
    {
        sirenMode_ = static_cast<int>((row - 2u) * 8u + col);
        sirenEnv_ = 1.0f;
        sirenSweep_ = static_cast<float>(col) / 7.0f;
        return;
    }

    if (row == 4)
    {
        springEnv_ = clampValue(0.45f + strength * 0.55f, 0.0f, 1.0f);
        parameters_[kParamTone] = clampValue(static_cast<float>(col) / 7.0f, 0.0f, 1.0f);
        return;
    }

    if (row == 5)
    {
        dropFrames_ = framesForBeatDivision(0.25f + static_cast<float>(col) * 0.25f);
        return;
    }

    if (row == 6)
    {
        chopFrames_ = framesForBeatDivision(0.5f + static_cast<float>(col) * 0.25f);
        return;
    }

    if (col < 4)
    {
        freezeFrames_ = framesForBeatDivision(1.0f + static_cast<float>(col));
    }
    else
    {
        clearPerformance();
        cellFlash_[index] = 1.0f;
    }
}

void Processor::processAudio(const float* inLeft,
                             const float* inRight,
                             float* outLeft,
                             float* outRight,
                             const std::uint32_t frameCount)
{
    if (outLeft == nullptr && outRight == nullptr)
        return;

    const float outputGain = 0.15f + parameters_[kParamOutput] * 1.35f;
    const float tone = parameters_[kParamTone];
    const float damp = 0.02f + tone * 0.42f;
    const float baseFeedback = std::min(0.92f, parameters_[kParamFeedback] * 0.78f);
    const std::size_t delaySize = delayLeft_.empty() ? 1u : delayLeft_.size();
    const std::size_t delaySamples = clampValue(static_cast<std::size_t>(sampleRate_ * (0.14 + parameters_[kParamWet] * 0.72)),
                                                static_cast<std::size_t>(1u),
                                                delaySize - 1u);

    for (std::uint32_t frame = 0; frame < frameCount; ++frame)
    {
        const float inputL = inLeft != nullptr ? inLeft[frame] : 0.0f;
        const float inputR = inRight != nullptr ? inRight[frame] : inputL;

        const std::size_t read = (delayWrite_ + delaySize - delaySamples) % delaySize;
        float delayL = delayLeft_[read];
        float delayR = delayRight_[(read + delaySamples / 5u) % delaySize];

        delayToneLeft_ += (delayL - delayToneLeft_) * damp;
        delayToneRight_ += (delayR - delayToneRight_) * damp;
        delayL = delayToneLeft_;
        delayR = delayToneRight_;

        const float throwAmount = throwFrames_ > 0 ? throwSend_ : 0.0f;
        const float freeze = freezeFrames_ > 0 ? 1.0f : 0.0f;
        const float feedback = std::min(0.985f, baseFeedback + throwFeedback_ + freeze * 0.30f);
        const float send = parameters_[kParamWet] * 0.20f + throwAmount;

        delayLeft_[delayWrite_] = inputL * send + delayR * feedback;
        delayRight_[delayWrite_] = inputR * send + delayL * feedback;
        delayWrite_ = (delayWrite_ + 1u) % delaySize;

        float siren = 0.0f;
        if (sirenEnv_ > 0.0001f)
        {
            sirenLfo_ += static_cast<float>((0.9 + sirenSweep_ * 5.8) / sampleRate_);
            if (sirenLfo_ >= 1.0f)
                sirenLfo_ -= 1.0f;
            const float wobble = 0.5f + 0.5f * std::sin(sirenLfo_ * 2.0f * kPi);
            const float base = (sirenMode_ < 8 ? 360.0f : 540.0f) + static_cast<float>(sirenMode_ % 8) * 45.0f;
            const float hz = base + wobble * (180.0f + sirenSweep_ * 900.0f);
            sirenPhase_ += hz / static_cast<float>(sampleRate_);
            if (sirenPhase_ >= 1.0f)
                sirenPhase_ -= 1.0f;
            const float sine = std::sin(sirenPhase_ * 2.0f * kPi);
            const float square = sirenPhase_ < 0.5f ? 1.0f : -1.0f;
            siren = (sirenMode_ % 3 == 0 ? sine : (sine * 0.62f + square * 0.38f)) *
                    sirenEnv_ * parameters_[kParamSirenLevel] * 0.38f;
            sirenEnv_ *= 0.9995f;
        }

        float spring = 0.0f;
        if (springEnv_ > 0.0001f)
        {
            springState_ = springState_ * 0.78f + noise() * springEnv_;
            spring = springState_ * parameters_[kParamSpring] * 0.18f;
            springEnv_ *= 0.9965f;
        }

        float dryGain = parameters_[kParamDry];
        if (dropFrames_ > 0)
            dryGain *= 0.08f;
        if (chopFrames_ > 0 && ((chopFrames_ / 900u) % 2u) == 0u)
            dryGain *= 0.20f;

        const float wetGain = parameters_[kParamWet] * (0.25f + throwAmount * 1.3f);
        const float outL = (inputL * dryGain + delayL * wetGain + siren + spring) * outputGain;
        const float outR = (inputR * dryGain + delayR * wetGain + siren * 0.92f - spring) * outputGain;

        if (outLeft != nullptr)
            outLeft[frame] = std::tanh(outL);
        if (outRight != nullptr)
            outRight[frame] = std::tanh(outR);

        if (throwFrames_ > 0)
            --throwFrames_;
        if (dropFrames_ > 0)
            --dropFrames_;
        if (chopFrames_ > 0)
            --chopFrames_;
        if (freezeFrames_ > 0)
            --freezeFrames_;
    }
}

void Processor::emitLedFeedback(ProcessResult& result, const bool force)
{
    if (parameters_[kParamLedFeedback] < 0.5f)
        return;

    if (!ledInitialized_)
        appendSysex(result, kProgrammerModeSysex.data(), static_cast<std::uint8_t>(kProgrammerModeSysex.size()));

    for (std::size_t row = 0; row < kGridHeight; ++row)
    {
        for (std::size_t col = 0; col < kGridWidth; ++col)
        {
            const std::size_t index = cellIndex(row, col);
            const bool active = cellFlash_[index] > 0.02f;
            const std::uint8_t color = colorForPad(row, col, active);
            if (force || color != lastCellLeds_[index])
            {
                appendMidi(result, 0, 0x90u, gridToNote(row, col), color);
                lastCellLeds_[index] = color;
            }
        }
    }

    const std::array<std::uint8_t, 9> topColors = {{
        kLedBlue, kLedBlue, kLedOrange, kLedOrange, kLedGreen, kLedGreen, kLedRed, kLedRed, kLedWhite,
    }};
    for (std::size_t i = 0; i < topColors.size(); ++i)
    {
        if (force || topColors[i] != lastTopLeds_[i])
        {
            appendMidi(result, 0, 0xb0u, kTopButtonCCs[i], topColors[i]);
            lastTopLeds_[i] = topColors[i];
        }
    }

    const int sirenIndex = clampValue(static_cast<int>(std::ceil(parameters_[kParamSirenLevel] * 8.0f)) - 1, 0, 7);
    for (std::size_t i = 0; i < kSideButtonCCs.size(); ++i)
    {
        const std::uint8_t color = static_cast<int>(i) <= sirenIndex ? kLedPurple : kLedOff;
        if (force || color != lastSideLeds_[i])
        {
            appendMidi(result, 0, 0xb0u, kSideButtonCCs[i], color);
            lastSideLeds_[i] = color;
        }
    }

    ledInitialized_ = true;
}

void Processor::appendMidi(ProcessResult& result,
                           const std::uint32_t frame,
                           const std::uint8_t status,
                           const std::uint8_t data1,
                           const std::uint8_t data2)
{
    if (result.eventCount >= result.events.size())
        return;

    MidiMessage& event = result.events[result.eventCount++];
    event.frame = frame;
    event.size = 3;
    event.data[0] = status;
    event.data[1] = data1;
    event.data[2] = data2;
}

void Processor::appendSysex(ProcessResult& result, const std::uint8_t* data, const std::uint8_t size)
{
    if (result.eventCount >= result.events.size() || data == nullptr || size > result.events[0].data.size())
        return;

    MidiMessage& event = result.events[result.eventCount++];
    event.frame = 0;
    event.size = size;
    for (std::uint8_t i = 0; i < size; ++i)
        event.data[i] = data[i];
}

std::uint8_t Processor::colorForPad(const std::size_t row, const std::size_t, const bool active) const noexcept
{
    if (!active)
    {
        switch (row)
        {
        case 0:
        case 1: return kLedDim;
        default: return kLedOff;
        }
    }

    if (row <= 1)
        return kLedBlue;
    if (row <= 3)
        return kLedPurple;
    if (row == 4)
        return kLedGreen;
    if (row == 5)
        return kLedOrange;
    if (row == 6)
        return kLedYellow;
    return kLedRed;
}

std::uint32_t Processor::framesForBeatDivision(const float beats) const noexcept
{
    return static_cast<std::uint32_t>(std::max(1.0, sampleRate_ * 60.0 / 120.0 * beats));
}

void Processor::decayCellFlashes(const std::uint32_t frameCount)
{
    const float amount = static_cast<float>(frameCount / std::max(1.0, sampleRate_ * 0.28));
    for (float& flash : cellFlash_)
        flash = std::max(0.0f, flash - amount);
    status_.activity = std::max(0.0f, status_.activity - amount * 2.0f);
}

std::uint32_t Processor::random()
{
    randomState_ ^= randomState_ << 13u;
    randomState_ ^= randomState_ >> 17u;
    randomState_ ^= randomState_ << 5u;
    return randomState_;
}

float Processor::noise()
{
    return static_cast<float>(static_cast<int>(random() & 0xffffu) - 32768) / 32768.0f;
}

} // namespace downspout::paunchlad
