#include "floozy_engine.hpp"

#include "flues/disyn/modules/OscillatorModule.hpp"
#include "flues/pm/Random.hpp"
#include "flues/pm/modules/DelayLinesModule.hpp"
#include "flues/pm/modules/EnvelopeModule.hpp"
#include "flues/pm/modules/FeedbackModule.hpp"
#include "flues/pm/modules/FilterModule.hpp"
#include "flues/pm/modules/InterfaceModule.hpp"
#include "flues/pm/modules/ModulationModule.hpp"
#include "flues/pm/modules/ReverbModule.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace downspout::floozy {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::size_t kVoiceCount = FloozyEngine::kMaxVoices;

float midiNoteToFrequency(const int midiNote)
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

float clampUnit(const float value)
{
    return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
}

float sanitizeAudio(const float value)
{
    if (!std::isfinite(value))
        return 0.0f;
    return std::tanh(std::clamp(value, -4.0f, 4.0f));
}

float equalPowerLeft(const float pan)
{
    return std::cos(std::clamp(pan, 0.0f, 1.0f) * kPi * 0.5f);
}

float equalPowerRight(const float pan)
{
    return std::sin(std::clamp(pan, 0.0f, 1.0f) * kPi * 0.5f);
}

struct FloozyParams {
    std::array<float, kParameterCount> values {};
    std::uint64_t version = 1;

    FloozyParams()
    {
        for (std::size_t i = 0; i < kParameterCount; ++i)
            values[i] = kParameterSpecs[i].defaultValue;
    }
};

class FloozySource {
public:
    explicit FloozySource(const float sampleRate)
        : oscillator_(sampleRate)
    {
    }

    void reset()
    {
        oscillator_.reset();
        dcBlockX1_ = 0.0f;
        dcBlockY1_ = 0.0f;
    }

    void sync(const FloozyParams& params)
    {
        algorithm_ = static_cast<flues::disyn::AlgorithmType>(
            static_cast<int>(std::round(std::clamp(params.values[0], 0.0f, 6.0f))));
        param1_ = clampUnit(params.values[1]);
        param2_ = clampUnit(params.values[2]);
        toneLevel_ = clampUnit(params.values[3]);
        noiseLevel_ = clampUnit(params.values[4]) * 0.25f;
        dcLevel_ = (clampUnit(params.values[5]) - 0.5f) * 0.20f;
    }

    float process(const float frequency)
    {
        const auto out = oscillator_.process(algorithm_, std::max(1.0f, frequency), param1_, param2_);
        const float osc = (out.primary * 0.75f + out.secondary * 0.25f) * toneLevel_;
        const float noise = rng_.uniformSignedFloat() * noiseLevel_;
        const float dcBlocked = dcBlock(osc + noise + dcLevel_);
        return sanitizeAudio(dcBlocked);
    }

private:
    float dcBlock(const float sample)
    {
        const float y = sample - dcBlockX1_ + 0.995f * dcBlockY1_;
        dcBlockX1_ = sample;
        dcBlockY1_ = y;
        return y;
    }

    flues::disyn::OscillatorModule oscillator_;
    flues::pm::Random rng_ {0x710020u};
    flues::disyn::AlgorithmType algorithm_ = flues::disyn::AlgorithmType::TANH_SQUARE;
    float param1_ = 0.55f;
    float param2_ = 0.50f;
    float toneLevel_ = 0.50f;
    float noiseLevel_ = 0.05f;
    float dcLevel_ = 0.0f;
    float dcBlockX1_ = 0.0f;
    float dcBlockY1_ = 0.0f;
};

class FloozyVoice {
public:
    FloozyVoice(const float sampleRate, const std::size_t slot)
        : source_(sampleRate)
        , envelope_(sampleRate)
        , interface_(sampleRate)
        , delayLines_(sampleRate)
        , filter_(sampleRate)
        , modulation_(sampleRate)
        , pan_(kVoiceCount > 1 ? static_cast<float>(slot) / static_cast<float>(kVoiceCount - 1) : 0.5f)
    {
    }

    void noteOn(const int midiNote, const std::uint8_t velocity, const FloozyParams& params, const std::uint64_t age)
    {
        midiNote_ = midiNote;
        frequency_ = midiNoteToFrequency(midiNote);
        velocityGain_ = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
        age_ = age;
        active_ = true;
        releasing_ = false;
        resetModules();
        envelope_.setGate(true);
        interface_.setGate(true);
        sync(params);
    }

    void noteOff()
    {
        if (!active_)
            return;
        releasing_ = true;
        envelope_.setGate(false);
        interface_.setGate(false);
    }

    void forceStop()
    {
        active_ = false;
        releasing_ = false;
        midiNote_ = -1;
        resetModules();
        envelope_.setGate(false);
        interface_.setGate(false);
    }

    StereoFrame process(const FloozyParams& params)
    {
        if (!active_)
            return {};

        sync(params);

        const auto mod = modulation_.process();
        const float source = source_.process(frequency_ * mod.fm) * velocityGain_;
        const float env = envelope_.process();
        const bool envActive = envelope_.isPlaying();
        const float feedbackSignal = feedback_.process(prevDelay1_, prevDelay2_, prevFilter_);

        if (envActive)
            releaseDamp_ = 1.0f;
        else
            releaseDamp_ *= 0.995f;

        const float interfaceInput = source * env + feedbackSignal * releaseDamp_;
        const float interfaceOut = interface_.process(sanitizeAudio(interfaceInput));
        const auto delays = delayLines_.process(sanitizeAudio(interfaceOut), frequency_);
        const float delayMix = (delays.delay1 + delays.delay2) * 0.5f;
        const float filtered = filter_.process(sanitizeAudio(delayMix));
        const float mono = sanitizeAudio(filtered * mod.am * velocityGain_ *
                                         params.values[static_cast<std::size_t>(ParamId::masterGain)]);

        prevDelay1_ = sanitizeAudio(delays.delay1);
        prevDelay2_ = sanitizeAudio(delays.delay2);
        prevFilter_ = mono;
        lastLevel_ = std::fabs(mono);

        if (!envActive && releaseDamp_ < 1.0e-4f && lastLevel_ < 1.0e-5f &&
            std::fabs(prevDelay1_) < 1.0e-5f && std::fabs(prevDelay2_) < 1.0e-5f)
            forceStop();

        return {mono * equalPowerLeft(pan_), mono * equalPowerRight(pan_)};
    }

    bool active() const { return active_; }
    bool releasing() const { return releasing_; }
    int note() const { return midiNote_; }
    std::uint64_t age() const { return age_; }
    float level() const { return lastLevel_; }

private:
    void sync(const FloozyParams& params)
    {
        if (paramsVersion_ == params.version)
            return;
        paramsVersion_ = params.version;

        source_.sync(params);
        envelope_.setAttack(params.values[static_cast<std::size_t>(ParamId::attack)]);
        envelope_.setRelease(params.values[static_cast<std::size_t>(ParamId::release)]);
        interface_.setType(static_cast<int>(std::round(params.values[static_cast<std::size_t>(ParamId::interfaceType)])));
        interface_.setIntensity(params.values[static_cast<std::size_t>(ParamId::interfaceIntensity)]);
        delayLines_.setTuning(params.values[static_cast<std::size_t>(ParamId::tuning)]);
        delayLines_.setRatio(params.values[static_cast<std::size_t>(ParamId::ratio)]);
        feedback_.setDelay1Gain(params.values[static_cast<std::size_t>(ParamId::delay1Feedback)]);
        feedback_.setDelay2Gain(params.values[static_cast<std::size_t>(ParamId::delay2Feedback)]);
        feedback_.setFilterGain(params.values[static_cast<std::size_t>(ParamId::filterFeedback)]);
        filter_.setFrequency(params.values[static_cast<std::size_t>(ParamId::filterFrequency)]);
        filter_.setQ(params.values[static_cast<std::size_t>(ParamId::filterQ)]);
        filter_.setShape(params.values[static_cast<std::size_t>(ParamId::filterShape)]);
        modulation_.setFrequency(params.values[static_cast<std::size_t>(ParamId::lfoFrequency)]);
        modulation_.setTypeLevel(params.values[static_cast<std::size_t>(ParamId::modulationMix)]);
    }

    void resetModules()
    {
        source_.reset();
        envelope_.reset();
        interface_.reset();
        delayLines_.reset();
        feedback_.reset();
        filter_.reset();
        modulation_.reset();
        prevDelay1_ = 0.0f;
        prevDelay2_ = 0.0f;
        prevFilter_ = 0.0f;
        releaseDamp_ = 1.0f;
        lastLevel_ = 0.0f;
        paramsVersion_ = 0;
    }

    FloozySource source_;
    flues::pm::EnvelopeModule envelope_;
    flues::pm::InterfaceModule interface_;
    flues::pm::DelayLinesModule delayLines_;
    flues::pm::FeedbackModule feedback_;
    flues::pm::FilterModule filter_;
    flues::pm::ModulationModule modulation_;
    float pan_ = 0.5f;
    float frequency_ = 440.0f;
    float velocityGain_ = 1.0f;
    bool active_ = false;
    bool releasing_ = false;
    int midiNote_ = -1;
    std::uint64_t age_ = 0;
    std::uint64_t paramsVersion_ = 0;
    float prevDelay1_ = 0.0f;
    float prevDelay2_ = 0.0f;
    float prevFilter_ = 0.0f;
    float releaseDamp_ = 1.0f;
    float lastLevel_ = 0.0f;
};

} // namespace

class FloozyEngine::Impl {
public:
    explicit Impl(const float sampleRate)
        : sampleRate_(std::max(1000.0f, sampleRate))
        , reverbLeft_(sampleRate_)
        , reverbRight_(sampleRate_)
    {
        createVoices();
        syncReverb();
    }

    void setSampleRate(const float sampleRate)
    {
        sampleRate_ = std::max(1000.0f, sampleRate);
        reverbLeft_ = flues::pm::ReverbModule(sampleRate_);
        reverbRight_ = flues::pm::ReverbModule(sampleRate_);
        createVoices();
        syncReverb();
    }

    void reset()
    {
        for (auto& voice : voices_)
            voice->forceStop();
        reverbLeft_.reset();
        reverbRight_.reset();
        ageCounter_ = 0;
    }

    float getParameter(const std::uint32_t index) const
    {
        if (index >= kParameterCount)
            return 0.0f;
        return params_.values[index];
    }

    void setParameter(const std::uint32_t index, const float value)
    {
        if (index >= kParameterCount)
            return;

        const auto& spec = kParameterSpecs[index];
        float clamped = std::clamp(std::isfinite(value) ? value : spec.defaultValue, spec.minimum, spec.maximum);
        if (spec.integer)
            clamped = std::round(clamped);

        if (std::fabs(params_.values[index] - clamped) <= 1.0e-7f)
            return;

        params_.values[index] = clamped;
        ++params_.version;

        if (index == static_cast<std::uint32_t>(ParamId::reverbSize) ||
            index == static_cast<std::uint32_t>(ParamId::reverbLevel))
            syncReverb();
    }

    void noteOn(const int midiNote, const std::uint8_t velocity)
    {
        if (midiNote < 0 || midiNote > 127)
            return;
        if (velocity == 0)
        {
            noteOff(midiNote);
            return;
        }

        if (auto* existing = findVoiceByNote(midiNote))
        {
            existing->noteOn(midiNote, velocity, params_, ++ageCounter_);
            return;
        }

        if (auto* idle = findIdleVoice())
        {
            idle->noteOn(midiNote, velocity, params_, ++ageCounter_);
            return;
        }

        if (auto* victim = selectVoiceToSteal())
            victim->noteOn(midiNote, velocity, params_, ++ageCounter_);
    }

    void noteOff(const int midiNote)
    {
        if (auto* voice = findVoiceByNote(midiNote))
            voice->noteOff();
    }

    void allNotesOff()
    {
        reset();
    }

    void handleMidi(const std::uint8_t* const data, const std::uint32_t size)
    {
        if (data == nullptr || size < 1)
            return;

        const std::uint8_t status = data[0] & 0xf0U;
        const std::uint8_t data1 = size > 1 ? data[1] : 0;
        const std::uint8_t data2 = size > 2 ? data[2] : 0;

        switch (status)
        {
        case 0x80:
            noteOff(data1);
            break;
        case 0x90:
            noteOn(data1, data2);
            break;
        case 0xb0:
            if (data1 == 120 || data1 == 123)
                allNotesOff();
            break;
        default:
            break;
        }
    }

    StereoFrame processStereo()
    {
        StereoFrame dry {};
        for (auto& voice : voices_)
        {
            const StereoFrame frame = voice->process(params_);
            dry.left += frame.left;
            dry.right += frame.right;
        }

        const float wetLeft = reverbLeft_.process(sanitizeAudio(dry.left + dry.right * 0.08f));
        const float wetRight = reverbRight_.process(sanitizeAudio(dry.right + dry.left * 0.05f));
        return {sanitizeAudio(wetLeft), sanitizeAudio(wetRight)};
    }

    void processBlock(float* const left,
                      float* const right,
                      const std::uint32_t frames,
                      const MidiMessage* const midi,
                      const std::uint32_t midiCount)
    {
        std::uint32_t eventIndex = 0;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            while (eventIndex < midiCount && midi[eventIndex].frame <= frame)
            {
                handleMidi(midi[eventIndex].data.data(), midi[eventIndex].size);
                ++eventIndex;
            }

            const StereoFrame out = processStereo();
            if (left)
                left[frame] = out.left;
            if (right)
                right[frame] = out.right;
        }

        while (eventIndex < midiCount)
        {
            handleMidi(midi[eventIndex].data.data(), midi[eventIndex].size);
            ++eventIndex;
        }
    }

    std::size_t activeVoiceCount() const
    {
        std::size_t count = 0;
        for (const auto& voice : voices_)
            if (voice->active())
                ++count;
        return count;
    }

    bool hasActiveVoices() const
    {
        return activeVoiceCount() > 0;
    }

    float sampleRate() const { return sampleRate_; }

private:
    void createVoices()
    {
        for (std::size_t i = 0; i < voices_.size(); ++i)
            voices_[i] = std::make_unique<FloozyVoice>(sampleRate_, i);
    }

    void syncReverb()
    {
        const float size = params_.values[static_cast<std::size_t>(ParamId::reverbSize)];
        const float level = params_.values[static_cast<std::size_t>(ParamId::reverbLevel)];
        reverbLeft_.setSize(size);
        reverbRight_.setSize(std::clamp(size * 0.93f + 0.04f, 0.0f, 1.0f));
        reverbLeft_.setLevel(level);
        reverbRight_.setLevel(level);
    }

    FloozyVoice* findVoiceByNote(const int midiNote)
    {
        for (auto& voice : voices_)
            if (voice->active() && voice->note() == midiNote)
                return voice.get();
        return nullptr;
    }

    FloozyVoice* findIdleVoice()
    {
        for (auto& voice : voices_)
            if (!voice->active())
                return voice.get();
        return nullptr;
    }

    FloozyVoice* selectVoiceToSteal()
    {
        FloozyVoice* candidate = nullptr;
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        for (auto& voice : voices_)
        {
            if (voice->releasing() && voice->age() < oldest)
            {
                oldest = voice->age();
                candidate = voice.get();
            }
        }

        if (candidate)
            return candidate;

        float lowestLevel = std::numeric_limits<float>::max();
        for (auto& voice : voices_)
        {
            if (voice->level() < lowestLevel)
            {
                lowestLevel = voice->level();
                candidate = voice.get();
            }
        }
        return candidate;
    }

    float sampleRate_;
    FloozyParams params_;
    std::array<std::unique_ptr<FloozyVoice>, kMaxVoices> voices_;
    flues::pm::ReverbModule reverbLeft_;
    flues::pm::ReverbModule reverbRight_;
    std::uint64_t ageCounter_ = 0;
};

FloozyEngine::FloozyEngine(const float sampleRate)
    : impl_(std::make_unique<Impl>(sampleRate))
    , sampleRate_(impl_->sampleRate())
{
}

FloozyEngine::~FloozyEngine() = default;

void FloozyEngine::setSampleRate(const float sampleRate)
{
    impl_->setSampleRate(sampleRate);
    sampleRate_ = impl_->sampleRate();
}

void FloozyEngine::reset()
{
    impl_->reset();
}

float FloozyEngine::getParameter(const std::uint32_t index) const
{
    return impl_->getParameter(index);
}

void FloozyEngine::setParameter(const std::uint32_t index, const float value)
{
    impl_->setParameter(index, value);
}

float FloozyEngine::getParameter(const ParamId id) const
{
    return getParameter(static_cast<std::uint32_t>(id));
}

void FloozyEngine::setParameter(const ParamId id, const float value)
{
    setParameter(static_cast<std::uint32_t>(id), value);
}

void FloozyEngine::noteOn(const int midiNote, const std::uint8_t velocity)
{
    impl_->noteOn(midiNote, velocity);
}

void FloozyEngine::noteOff(const int midiNote)
{
    impl_->noteOff(midiNote);
}

void FloozyEngine::allNotesOff()
{
    impl_->allNotesOff();
}

void FloozyEngine::handleMidi(const std::uint8_t* const data, const std::uint32_t size)
{
    impl_->handleMidi(data, size);
}

void FloozyEngine::handleMidi(const MidiMessage& message)
{
    impl_->handleMidi(message.data.data(), message.size);
}

StereoFrame FloozyEngine::processStereo()
{
    return impl_->processStereo();
}

void FloozyEngine::processBlock(float* const left,
                                float* const right,
                                const std::uint32_t frames,
                                const MidiMessage* const midi,
                                const std::uint32_t midiCount)
{
    impl_->processBlock(left, right, frames, midi, midiCount);
}

std::size_t FloozyEngine::activeVoiceCount() const
{
    return impl_->activeVoiceCount();
}

bool FloozyEngine::hasActiveVoices() const
{
    return impl_->hasActiveVoices();
}

} // namespace downspout::floozy
