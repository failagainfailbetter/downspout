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
    delaySeconds_ = 0.34f;
    delayToneLeft_ = 0.0f;
    delayToneRight_ = 0.0f;
    sirenPhase_ = 0.0f;
    sirenLfo_ = 0.0f;
    sirenEnv_ = 0.0f;
    sirenSweep_ = 0.0f;
    alarmPhase_ = 0.0f;
    alarmEnv_ = 0.0f;
    alarmRate_ = 3.5f;
    alarmMode_ = 0;
    snareEnv_ = 0.0f;
    snareBodyPhase_ = 0.0f;
    snareBodyHz_ = 180.0f;
    springEnv_ = 0.0f;
    springState_ = 0.0f;
    throwSend_ = 0.0f;
    throwFeedback_ = 0.0f;
    throwFrames_ = 0;
    dropFrames_ = 0;
    chopFrames_ = 0;
    freezeFrames_ = 0;
    inputLayout_ = LaunchpadInputLayout::unknown;
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

    bool mapped = false;
    if (parameters_[kParamLedFeedback] >= 0.5f)
    {
        mapped = noteToGrid(note, row, col);
        if (mapped)
            inputLayout_ = LaunchpadInputLayout::programmer;
    }
    else
    {
        if (inputLayout_ == LaunchpadInputLayout::programmer)
            mapped = noteToGrid(note, row, col);
        else if (inputLayout_ == LaunchpadInputLayout::custom)
            mapped = noteToLinearChromaticGrid(note, row, col) || noteToDefaultCustomGrid(note, row, col);

        if (!mapped && noteToLinearChromaticGrid(note, row, col))
        {
            inputLayout_ = LaunchpadInputLayout::custom;
            mapped = true;
        }
        if (!mapped && noteToDefaultCustomGrid(note, row, col))
        {
            inputLayout_ = LaunchpadInputLayout::custom;
            mapped = true;
        }
        if (!mapped && noteToGrid(note, row, col))
        {
            inputLayout_ = LaunchpadInputLayout::programmer;
            mapped = true;
        }
    }

    if (!mapped)
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
    case 0: triggerPad(0, 0, 1.0f); break;
    case 1: triggerPad(4, 2, 1.0f); break;
    case 2: triggerPad(6, 6, 1.0f); break;
    case 3: triggerPad(5, 4, 1.0f); break;
    case 4: parameters_[kParamWet] = clampValue(parameters_[kParamWet] + 0.08f, 0.0f, 1.0f); break;
    case 5: parameters_[kParamFeedback] = clampValue(parameters_[kParamFeedback] + 0.08f, 0.0f, 1.0f); break;
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
    triggerPad(6, static_cast<std::size_t>(index), 0.85f);
    status_.activity = 1.0f;
    return true;
}

void Processor::triggerPad(const std::size_t row, const std::size_t col, const float strength)
{
    const std::size_t index = cellIndex(row, col);
    cellFlash_[index] = 1.0f;
    status_.activity = 1.0f;
    status_.mode = static_cast<float>(row);

    const std::size_t visualRow = kGridHeight - 1u - row;

    if (visualRow == 7)
    {
        static constexpr std::array<float, 8> kBeatLengths = {{
            0.25f, 0.50f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f,
        }};
        throwFrames_ = framesForBeatDivision(kBeatLengths[col]);
        delaySeconds_ = 0.10f + static_cast<float>(col) * 0.12f;
        throwSend_ = clampValue(0.45f + strength * 0.55f, 0.0f, 1.0f);
        throwFeedback_ = clampValue(0.18f + static_cast<float>(col) * 0.08f, 0.0f, 0.86f);
        snareEnv_ = 0.35f + strength * 0.25f;
        snareBodyHz_ = 135.0f + static_cast<float>(col) * 16.0f;
        return;
    }

    if (visualRow == 6)
    {
        chopFrames_ = framesForBeatDivision(0.25f + static_cast<float>(col) * 0.18f);
        throwFrames_ = framesForBeatDivision(0.5f + static_cast<float>(col) * 0.25f);
        delaySeconds_ = 0.12f + static_cast<float>(col) * 0.06f;
        throwSend_ = clampValue(0.35f + strength * 0.45f, 0.0f, 1.0f);
        throwFeedback_ = clampValue(0.20f + static_cast<float>(col) * 0.07f, 0.0f, 0.82f);
        return;
    }

    if (visualRow == 5)
    {
        dropFrames_ = framesForBeatDivision(0.25f + static_cast<float>(col) * 0.25f);
        throwFrames_ = framesForBeatDivision(0.5f + static_cast<float>(col) * 0.20f);
        delaySeconds_ = 0.16f + static_cast<float>(col) * 0.06f;
        throwSend_ = clampValue(0.30f + strength * 0.50f, 0.0f, 1.0f);
        throwFeedback_ = clampValue(0.15f + static_cast<float>(col) * 0.06f, 0.0f, 0.75f);
        return;
    }

    if (visualRow == 4)
    {
        springEnv_ = clampValue(0.45f + strength * 0.55f, 0.0f, 1.0f);
        parameters_[kParamTone] = clampValue(static_cast<float>(col) / 7.0f, 0.0f, 1.0f);
        throwFrames_ = framesForBeatDivision(0.5f + static_cast<float>(col) * 0.18f);
        delaySeconds_ = 0.11f + static_cast<float>(col) * 0.05f;
        throwSend_ = 0.35f + strength * 0.35f;
        throwFeedback_ = 0.24f + static_cast<float>(col) * 0.05f;
        return;
    }

    if (visualRow == 3)
    {
        static constexpr std::array<float, 8> kSnareEchoLengths = {{
            0.25f, 0.375f, 0.50f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f,
        }};
        snareEnv_ = clampValue(0.75f + strength * 0.45f, 0.0f, 1.2f);
        snareBodyHz_ = 150.0f + static_cast<float>(col) * 22.0f;
        throwFrames_ = framesForBeatDivision(kSnareEchoLengths[col]);
        delaySeconds_ = 0.10f + static_cast<float>(col) * 0.075f;
        throwSend_ = clampValue(0.70f + strength * 0.35f, 0.0f, 1.0f);
        throwFeedback_ = clampValue(0.42f + static_cast<float>(col) * 0.06f, 0.0f, 0.90f);
        return;
    }

    if (visualRow == 2)
    {
        sirenMode_ = static_cast<int>((row - 2u) * 8u + col);
        sirenEnv_ = 1.0f;
        sirenSweep_ = static_cast<float>(col) / 7.0f;
        throwFrames_ = framesForBeatDivision(1.0f + static_cast<float>(col) * 0.25f);
        delaySeconds_ = 0.18f + static_cast<float>(col) * 0.08f;
        throwSend_ = 0.25f + strength * 0.30f;
        throwFeedback_ = 0.28f + static_cast<float>(col) * 0.05f;
        return;
    }

    if (visualRow == 1)
    {
        alarmMode_ = static_cast<int>(col);
        alarmEnv_ = 1.0f;
        alarmRate_ = 2.6f + static_cast<float>(col) * 0.9f;
        throwFrames_ = framesForBeatDivision(1.0f + static_cast<float>(col) * 0.30f);
        delaySeconds_ = 0.16f + static_cast<float>(col) * 0.07f;
        throwSend_ = 0.30f + strength * 0.35f;
        throwFeedback_ = 0.34f + static_cast<float>(col) * 0.055f;
        return;
    }

    if (col < 3)
    {
        freezeFrames_ = framesForBeatDivision(1.0f + static_cast<float>(col) * 1.5f);
    }
    else if (col < 6)
    {
        alarmEnv_ = 1.0f;
        alarmMode_ = static_cast<int>(col);
        springEnv_ = 1.0f;
        dropFrames_ = framesForBeatDivision(0.5f);
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
    const float gestureDelay = clampValue(delaySeconds_ + parameters_[kParamWet] * 0.10f, 0.06f, 1.25f);
    const std::size_t delaySamples = clampValue(static_cast<std::size_t>(sampleRate_ * gestureDelay),
                                                static_cast<std::size_t>(1u),
                                                delaySize - 1u);

    for (std::uint32_t frame = 0; frame < frameCount; ++frame)
    {
        const float inputL = inLeft != nullptr ? inLeft[frame] : 0.0f;
        const float inputR = inRight != nullptr ? inRight[frame] : inputL;

        float snare = 0.0f;
        if (snareEnv_ > 0.0001f)
        {
            snareBodyPhase_ += snareBodyHz_ / static_cast<float>(sampleRate_);
            if (snareBodyPhase_ >= 1.0f)
                snareBodyPhase_ -= 1.0f;
            const float body = std::sin(snareBodyPhase_ * 2.0f * kPi) * snareEnv_;
            const float crack = noise() * snareEnv_ * snareEnv_;
            snare = body * 0.32f + crack * 0.42f;
            snareEnv_ *= 0.986f;
        }

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

        delayLeft_[delayWrite_] = (inputL + snare * 0.80f) * send + delayR * feedback;
        delayRight_[delayWrite_] = (inputR + snare * 0.65f) * send + delayL * feedback;
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

        float alarm = 0.0f;
        if (alarmEnv_ > 0.0001f)
        {
            alarmPhase_ += alarmRate_ / static_cast<float>(sampleRate_);
            if (alarmPhase_ >= 1.0f)
                alarmPhase_ -= 1.0f;
            const float gate = alarmPhase_ < 0.50f ? 1.0f : 0.20f;
            const float toneHz = (alarmPhase_ < 0.50f ? 680.0f : 920.0f) +
                                 static_cast<float>(alarmMode_) * 38.0f;
            sirenPhase_ += toneHz / static_cast<float>(sampleRate_);
            if (sirenPhase_ >= 1.0f)
                sirenPhase_ -= 1.0f;
            const float square = sirenPhase_ < 0.5f ? 1.0f : -1.0f;
            const float sine = std::sin(sirenPhase_ * 2.0f * kPi);
            alarm = (square * 0.55f + sine * 0.45f) * gate * alarmEnv_ *
                    parameters_[kParamSirenLevel] * 0.46f;
            alarmEnv_ *= 0.9992f;
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
        const float outL = (inputL * dryGain + delayL * wetGain + snare * 0.42f + siren + alarm + spring) * outputGain;
        const float outR = (inputR * dryGain + delayR * wetGain + snare * 0.36f + siren * 0.92f + alarm * 0.86f - spring) * outputGain;

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

    const std::size_t visualRow = kGridHeight - 1u - row;
    if (visualRow == 7 || visualRow == 3)
        return kLedBlue;
    if (visualRow == 2 || visualRow == 1)
        return kLedPurple;
    if (visualRow == 4)
        return kLedGreen;
    if (visualRow == 5)
        return kLedOrange;
    if (visualRow == 6)
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
