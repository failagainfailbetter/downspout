#include "DistrhoPlugin.hpp"

#include "sidecar_engine.hpp"
#include "sidecar_protocol.hpp"
#include "sidecar_serialization.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>

START_NAMESPACE_DISTRHO

namespace {

enum ParameterIndex : uint32_t {
    kParamChannel = 0,
    kParamBars,
    kParamRegister,
    kParamRegisterLow,
    kParamRegisterHigh,
    kParamDensity,
    kParamRisk,
    kParamHumanize,
    kParamMute,
    kParamGenerate,
    kParamAccept,
    kParamRetry,
    kParamStatusReady,
    kParameterCount
};

enum StateIndex : uint32_t {
    kStatePhrase = 0,
    kStateCount
};

constexpr const char* kStateKeyPhrase = "phrase";

using downspout::sidecar::BlockResult;
using downspout::sidecar::Controls;
using downspout::sidecar::EngineState;
using downspout::sidecar::MidiEventType;
using downspout::sidecar::Phrase;
using downspout::sidecar::RegisterId;
using downspout::sidecar::ScheduledMidiEvent;
using downspout::sidecar::TransportSnapshot;

ParameterEnumerationValue kRegisterEnumValues[] = {
    {0.0f, "Low"},
    {1.0f, "Mid"},
    {2.0f, "High"},
    {3.0f, "Custom"},
};

TransportSnapshot toCoreTransport(const TimePosition& timePos)
{
    TransportSnapshot transport {};
    transport.valid = timePos.bbt.valid;
    transport.playing = timePos.playing;
    if (timePos.bbt.valid)
    {
        transport.bar = static_cast<double>(timePos.bbt.bar - 1);
        transport.barBeat = static_cast<double>(timePos.bbt.beat - 1) +
                            (timePos.bbt.ticksPerBeat > 0.0
                                 ? timePos.bbt.tick / timePos.bbt.ticksPerBeat
                                 : 0.0);
        transport.beatsPerBar = timePos.bbt.beatsPerBar;
        transport.bpm = timePos.bbt.beatsPerMinute;
    }
    return transport;
}

MidiEvent toDpfMidiEvent(const ScheduledMidiEvent& event)
{
    MidiEvent midi {};
    midi.frame = event.frame;
    midi.size = 3;
    midi.data[0] = static_cast<std::uint8_t>((event.type == MidiEventType::noteOn ? 0x90u : 0x80u) | (event.channel & 0x0fu));
    midi.data[1] = event.data1;
    midi.data[2] = event.data2;
    midi.dataExt = nullptr;
    return midi;
}

void setRanges(Parameter& parameter, const float minValue, const float maxValue, const float defaultValue)
{
    parameter.ranges.min = minValue;
    parameter.ranges.max = maxValue;
    parameter.ranges.def = defaultValue;
}

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] float clampf(const float value, const float minimum, const float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] const std::uint8_t* midiData(const MidiEvent& event)
{
    return event.dataExt != nullptr ? event.dataExt : event.data;
}

}  // namespace

class SidecarPlugin : public Plugin
{
public:
    SidecarPlugin()
        : Plugin(kParameterCount, 0, kStateCount)
    {
        controls_ = downspout::sidecar::clampControls(controls_);
        downspout::sidecar::activate(engine_, controls_);
        Phrase phrase = downspout::sidecar::makeFallbackPhrase(controls_, 1u);
        downspout::sidecar::setPhrase(engine_, phrase);
        acceptedPhrase_ = phrase;
        hasAcceptedPhrase_ = true;
    }

protected:
    const char* getLabel() const override { return "Sidecar"; }
    const char* getDescription() const override { return "AI-ready MIDI phrase sidecar and validated solo phrase player."; }
    const char* getMaker() const override { return "danja"; }
    const char* getHomePage() const override { return "https://danja.github.io/downspout/"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override { return d_cconst('S', 'd', 'C', 'r'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kParamChannel:
            parameter.name = "Channel";
            parameter.symbol = "channel";
            parameter.hints |= kParameterIsInteger;
            setRanges(parameter, 1.0f, 16.0f, 1.0f);
            break;
        case kParamBars:
            parameter.name = "Bars";
            parameter.symbol = "bars";
            parameter.hints |= kParameterIsInteger;
            setRanges(parameter, 1.0f, 8.0f, 4.0f);
            break;
        case kParamRegister:
            parameter.name = "Register";
            parameter.symbol = "register";
            parameter.hints |= kParameterIsInteger;
            setRanges(parameter, 0.0f, 3.0f, 1.0f);
            parameter.enumValues.count = static_cast<uint8_t>(std::size(kRegisterEnumValues));
            parameter.enumValues.restrictedMode = true;
            parameter.enumValues.values = kRegisterEnumValues;
            parameter.enumValues.deleteLater = false;
            break;
        case kParamRegisterLow:
            parameter.name = "Register Low";
            parameter.symbol = "register_low";
            parameter.hints |= kParameterIsInteger;
            setRanges(parameter, 0.0f, 127.0f, 55.0f);
            break;
        case kParamRegisterHigh:
            parameter.name = "Register High";
            parameter.symbol = "register_high";
            parameter.hints |= kParameterIsInteger;
            setRanges(parameter, 0.0f, 127.0f, 82.0f);
            break;
        case kParamDensity:
            parameter.name = "Density";
            parameter.symbol = "density";
            setRanges(parameter, 0.0f, 1.0f, 0.5f);
            break;
        case kParamRisk:
            parameter.name = "Risk";
            parameter.symbol = "risk";
            setRanges(parameter, 0.0f, 1.0f, 0.35f);
            break;
        case kParamHumanize:
            parameter.name = "Humanize";
            parameter.symbol = "humanize";
            setRanges(parameter, 0.0f, 1.0f, 0.0f);
            break;
        case kParamMute:
            parameter.name = "Mute";
            parameter.symbol = "mute";
            parameter.hints |= kParameterIsBoolean | kParameterIsInteger;
            setRanges(parameter, 0.0f, 1.0f, 0.0f);
            break;
        case kParamGenerate:
        case kParamAccept:
        case kParamRetry:
            parameter.name = index == kParamGenerate ? "Generate" : (index == kParamAccept ? "Accept" : "Retry");
            parameter.symbol = index == kParamGenerate ? "generate" : (index == kParamAccept ? "accept" : "retry");
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            setRanges(parameter, 0.0f, 1048576.0f, 0.0f);
            break;
        case kParamStatusReady:
            parameter.name = "Phrase Ready";
            parameter.symbol = "status_ready";
            parameter.hints = kParameterIsOutput | kParameterIsBoolean | kParameterIsInteger;
            setRanges(parameter, 0.0f, 1.0f, 1.0f);
            break;
        }
    }

    void initState(uint32_t index, State& state) override
    {
        if (index == kStatePhrase)
        {
            state.key = kStateKeyPhrase;
            state.label = "Phrase";
            state.hints = kStateIsOnlyForDSP;
            state.defaultValue = "";
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParamChannel: return static_cast<float>(controls_.channel);
        case kParamBars: return static_cast<float>(controls_.bars);
        case kParamRegister: return static_cast<float>(static_cast<int>(controls_.reg));
        case kParamRegisterLow: return static_cast<float>(controls_.registerLow);
        case kParamRegisterHigh: return static_cast<float>(controls_.registerHigh);
        case kParamDensity: return controls_.density;
        case kParamRisk: return controls_.risk;
        case kParamHumanize: return controls_.humanize;
        case kParamMute: return controls_.mute ? 1.0f : 0.0f;
        case kParamGenerate: return generateCounter_;
        case kParamAccept: return acceptCounter_;
        case kParamRetry: return retryCounter_;
        case kParamStatusReady: return engine_.phraseReady ? 1.0f : 0.0f;
        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamChannel: controls_.channel = static_cast<int>(std::lround(value)); break;
        case kParamBars: controls_.bars = static_cast<int>(std::lround(value)); break;
        case kParamRegister: controls_.reg = static_cast<RegisterId>(static_cast<int>(std::lround(value))); break;
        case kParamRegisterLow: controls_.registerLow = static_cast<int>(std::lround(value)); break;
        case kParamRegisterHigh: controls_.registerHigh = static_cast<int>(std::lround(value)); break;
        case kParamDensity: controls_.density = value; break;
        case kParamRisk: controls_.risk = value; break;
        case kParamHumanize: controls_.humanize = value; break;
        case kParamMute: controls_.mute = value >= 0.5f; break;
        case kParamGenerate:
            if (value > generateCounter_)
            {
                generateCounter_ = value;
                setGeneratedPhrase(static_cast<std::uint32_t>(std::lround(value)) + 1u);
            }
            break;
        case kParamAccept:
            acceptCounter_ = std::max(acceptCounter_, value);
            if (engine_.phraseReady)
            {
                acceptedPhrase_ = engine_.phrase;
                hasAcceptedPhrase_ = true;
            }
            break;
        case kParamRetry:
            if (value > retryCounter_)
            {
                retryCounter_ = value;
                setGeneratedPhrase(static_cast<std::uint32_t>(std::lround(value)) + 1009u);
            }
            break;
        default:
            break;
        }
        controls_ = downspout::sidecar::clampControls(controls_);
    }

    String getState(const char* key) const override
    {
        if (std::strcmp(key, kStateKeyPhrase) == 0)
        {
            if (hasAcceptedPhrase_)
                return String(downspout::sidecar::serializePhrase(acceptedPhrase_).c_str());
            if (engine_.phraseReady)
                return String(downspout::sidecar::serializePhrase(engine_.phrase).c_str());
        }
        return String();
    }

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, kStateKeyPhrase) != 0 || value == nullptr)
            return;
        const auto phrase = downspout::sidecar::deserializePhrase(value);
        if (phrase.has_value())
        {
            downspout::sidecar::setPhrase(engine_, *phrase);
            acceptedPhrase_ = *phrase;
            hasAcceptedPhrase_ = true;
        }
    }

    void activate() override
    {
        downspout::sidecar::activate(engine_, controls_);
    }

    void run(const float**, float**, uint32_t frames, const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        consumeInputMidi(midiEvents, midiEventCount);

        const BlockResult result = downspout::sidecar::processBlock(engine_,
                                                                    controls_,
                                                                    toCoreTransport(getTimePosition()),
                                                                    frames,
                                                                    getSampleRate());
        for (int i = 0; i < result.eventCount; ++i)
            writeMidiEvent(toDpfMidiEvent(result.events[i]));
    }

private:
    [[nodiscard]] Controls controlsForGeneration() const
    {
        Controls generationControls = downspout::sidecar::clampControls(controls_);
        if (capturedNoteCount_ < 3)
            return generationControls;

        generationControls.reg = RegisterId::custom;
        generationControls.registerLow = clampi(capturedMinNote_ + 7, 0, 127);
        generationControls.registerHigh = clampi(capturedMaxNote_ + 19, 0, 127);
        if (generationControls.registerHigh < generationControls.registerLow)
            std::swap(generationControls.registerHigh, generationControls.registerLow);

        const float densityHint = clampf(0.30f + static_cast<float>(capturedNoteCount_) / 80.0f, 0.25f, 0.95f);
        generationControls.density = clampf((generationControls.density * 0.65f) + (densityHint * 0.35f), 0.0f, 1.0f);

        const float rangeHint = clampf(static_cast<float>(capturedMaxNote_ - capturedMinNote_) / 72.0f, 0.0f, 1.0f);
        generationControls.risk = clampf((generationControls.risk * 0.75f) + (rangeHint * 0.25f), 0.0f, 1.0f);
        return generationControls;
    }

    void setGeneratedPhrase(const std::uint32_t seed)
    {
        controls_ = downspout::sidecar::clampControls(controls_);
        const Controls generationControls = controlsForGeneration();
        engine_.controls = generationControls;
        const std::uint32_t guidedSeed = seed ^ capturedSeed_;
        Phrase phrase = downspout::sidecar::makeFallbackPhrase(generationControls, guidedSeed == 0u ? seed : guidedSeed);
        downspout::sidecar::setPhrase(engine_, phrase);
    }

    void consumeInputMidi(const MidiEvent* midiEvents, const uint32_t midiEventCount)
    {
        if (midiEvents == nullptr)
            return;

        for (uint32_t i = 0; i < midiEventCount; ++i)
        {
            const MidiEvent& event = midiEvents[i];
            if (event.size < 3)
                continue;
            const std::uint8_t* data = midiData(event);
            const std::uint8_t status = data[0] & 0xf0u;
            if (status != 0x90u || data[2] == 0u)
                continue;

            const int note = data[1];
            capturedMinNote_ = std::min(capturedMinNote_, note);
            capturedMaxNote_ = std::max(capturedMaxNote_, note);
            ++capturedPitchClasses_[static_cast<std::size_t>(note % 12)];
            capturedNoteCount_ = std::min(capturedNoteCount_ + 1, 4096);
            capturedSeed_ ^= static_cast<std::uint32_t>((note << 16) ^ (data[2] << 8) ^ (event.frame & 0xffu));
            capturedSeed_ = capturedSeed_ * 1664525u + 1013904223u;
            if (capturedSeed_ == 0u)
                capturedSeed_ = 1u;
        }
    }

    Controls controls_ {};
    EngineState engine_ {};
    Phrase acceptedPhrase_ {};
    bool hasAcceptedPhrase_ = false;
    int capturedNoteCount_ = 0;
    int capturedMinNote_ = 127;
    int capturedMaxNote_ = 0;
    std::uint32_t capturedSeed_ = 1u;
    std::array<int, 12> capturedPitchClasses_ {};
    float generateCounter_ = 0.0f;
    float acceptCounter_ = 0.0f;
    float retryCounter_ = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidecarPlugin)
};

Plugin* createPlugin()
{
    return new SidecarPlugin();
}

END_NAMESPACE_DISTRHO
