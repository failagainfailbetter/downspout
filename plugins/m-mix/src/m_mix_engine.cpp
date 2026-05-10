#include "m_mix_engine.hpp"

#include <algorithm>
#include <cmath>

namespace downspout::mmix {
namespace {

[[nodiscard]] float clampf(const float value, const float minValue, const float maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

[[nodiscard]] int clampi(const int value, const int minValue, const int maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

[[nodiscard]] float randFloat(std::uint32_t& state) {
    state = (state * 1664525u) + 1013904223u;
    return static_cast<float>(state & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] bool euclidHit(const int step, const int pulses, const int offset, const int length) {
    if (length <= 0 || pulses <= 0) {
        return false;
    }
    if (pulses >= length) {
        return true;
    }

    const int base = (step + (offset % length)) % length;
    return ((base * pulses) % length) < pulses;
}

void appendMidi(BlockResult& result, const ScheduledMidiEvent& event) {
    if (result.eventCount >= kMaxScheduledMidiEvents) {
        result.overflow = true;
        return;
    }
    result.events[static_cast<std::size_t>(result.eventCount++)] = event;
}

[[nodiscard]] bool isNoteOn(const MidiMessage& event) {
    return event.size >= 3 && (event.data[0] & 0xf0) == 0x90 && (event.data[2] & 0x7f) > 0;
}

[[nodiscard]] bool isNoteOff(const MidiMessage& event) {
    if (event.size < 3) {
        return false;
    }
    const std::uint8_t type = event.data[0] & 0xf0;
    return type == 0x80 || (type == 0x90 && (event.data[2] & 0x7f) == 0);
}

[[nodiscard]] int heldIndex(const MidiMessage& event) {
    const int channel = event.data[0] & 0x0f;
    const int note = event.data[1] & 0x7f;
    return channel * 128 + note;
}

void startFade(EngineState& state,
               const float targetGain,
               const int granularity,
               const double framesPerBar,
               const float fadeDurMax) {
    if (framesPerBar <= 0.0) {
        return;
    }

    const float maxFraction = clampf(fadeDurMax, kFadeMinFraction, 1.0f);
    float fraction = kFadeMinFraction;
    if (maxFraction > kFadeMinFraction) {
        fraction = kFadeMinFraction + randFloat(state.rngState) * (maxFraction - kFadeMinFraction);
    }

    const double frames = framesPerBar * static_cast<double>(granularity) * static_cast<double>(fraction);
    const std::uint32_t totalFrames = frames > 1.0 ? static_cast<std::uint32_t>(std::llround(frames)) : 1u;
    state.fadeRemaining = totalFrames;
    state.targetGain = targetGain;
    state.fadeStep = (targetGain - state.currentGain) / static_cast<float>(totalFrames);
}

void applyProbabilisticTransition(EngineState& state,
                                  const std::int64_t bar,
                                  const Parameters& parameters,
                                  const double framesPerBar) {
    const int granularity = static_cast<int>(parameters.granularity);
    if (granularity <= 0 || (bar % granularity) != 0 || state.fadeRemaining > 0) {
        return;
    }

    float gain = state.currentGain;
    if (gain > 0.001f && gain < 0.999f) {
        gain = (gain < 0.5f) ? 0.0f : 1.0f;
        state.currentGain = gain;
    }

    float pMaintain = parameters.maintain;
    float pFade = parameters.fade;
    float pCut = parameters.cut;
    float sum = pMaintain + pFade + pCut;
    if (sum <= 0.0001f) {
        pMaintain = 1.0f;
        pFade = 0.0f;
        pCut = 0.0f;
        sum = 1.0f;
    }
    pMaintain /= sum;
    pFade /= sum;

    const float r = randFloat(state.rngState);
    if (r < pMaintain) {
        return;
    }

    const float biasNorm = clampf(parameters.bias * 0.01f, 0.0f, 1.0f);
    const float target = (randFloat(state.rngState) < biasNorm) ? 1.0f : 0.0f;
    if (r < (pMaintain + pFade)) {
        startFade(state, target, granularity, framesPerBar, parameters.fadeDurMax);
    } else {
        state.currentGain = target;
        state.targetGain = target;
        state.fadeRemaining = 0;
        state.fadeStep = 0.0f;
    }
}

[[nodiscard]] float euclideanGain(const EngineState& state, const Parameters& parameters, const double barPos) {
    const int totalBars = static_cast<int>(parameters.totalBars);
    const int division = static_cast<int>(parameters.division);
    const int steps = static_cast<int>(parameters.steps);
    const int offset = static_cast<int>(parameters.offset);
    const double cycleBars = static_cast<double>(totalBars);
    const double blockBars = cycleBars / static_cast<double>(division);

    double cyclePos = std::fmod(barPos - state.cycleOriginBar, cycleBars);
    if (cyclePos < 0.0) {
        cyclePos += cycleBars;
    }

    int blockIndex = static_cast<int>(std::floor(cyclePos / blockBars));
    blockIndex = clampi(blockIndex, 0, division - 1);

    if (!euclidHit(blockIndex, steps, offset, division)) {
        return 0.0f;
    }

    if (parameters.fadeBars <= 0.0f) {
        return 1.0f;
    }

    const double blockStart = static_cast<double>(blockIndex) * blockBars;
    const double localBar = cyclePos - blockStart;
    const double fade = static_cast<double>(parameters.fadeBars);
    const float inGain = clampf(static_cast<float>(localBar / fade), 0.0f, 1.0f);
    const float outGain = clampf(static_cast<float>((blockBars - localBar) / fade), 0.0f, 1.0f);
    return std::min(inGain, outGain);
}

void releaseHeldNotes(EngineState& state, BlockResult& result, const std::uint32_t frame) {
    for (int channel = 0; channel < 16; ++channel) {
        for (int note = 0; note < 128; ++note) {
            const int index = channel * 128 + note;
            if (state.heldNotes[static_cast<std::size_t>(index)] == 0) {
                continue;
            }

            ScheduledMidiEvent event {};
            event.frame = frame;
            event.size = 3;
            event.data[0] = static_cast<std::uint8_t>(0x80 | channel);
            event.data[1] = static_cast<std::uint8_t>(note);
            event.data[2] = 0;
            appendMidi(result, event);
            state.heldNotes[static_cast<std::size_t>(index)] = 0;
        }
    }
}

void processMidiEvent(EngineState& state,
                      BlockResult& result,
                      const InputMidiEvent& input,
                      const float gate,
                      const bool velocityFades) {
    ScheduledMidiEvent event = input;

    if (isNoteOn(input)) {
        if (gate <= 0.001f) {
            return;
        }

        if (velocityFades) {
            const int velocity = input.data[2] & 0x7f;
            const int scaled = clampi(static_cast<int>(std::lround(static_cast<float>(velocity) * gate)), 1, 127);
            event.data[2] = static_cast<std::uint8_t>(scaled);
        }

        const int index = heldIndex(input);
        std::uint8_t& held = state.heldNotes[static_cast<std::size_t>(index)];
        if (held < 255) {
            ++held;
        }
        appendMidi(result, event);
        return;
    }

    if (isNoteOff(input)) {
        const int index = heldIndex(input);
        std::uint8_t& held = state.heldNotes[static_cast<std::size_t>(index)];
        if (held == 0) {
            return;
        }
        --held;
        appendMidi(result, event);
        return;
    }

    appendMidi(result, event);
}

}  // namespace

Parameters clampParameters(const Parameters& raw) {
    Parameters parameters = raw;
    parameters.totalBars = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.totalBars)), 1, 4096));
    parameters.division = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.division)), 1, 512));
    const int division = static_cast<int>(parameters.division);
    parameters.steps = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.steps)), 0, division));
    parameters.offset = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.offset)), 0, division - 1));
    parameters.fadeBars = clampf(parameters.fadeBars, 0.0f, parameters.totalBars);
    parameters.granularity = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.granularity)), 1, 32));
    parameters.maintain = clampf(parameters.maintain, 0.0f, 100.0f);
    parameters.fade = clampf(parameters.fade, 0.0f, 100.0f);
    parameters.cut = clampf(parameters.cut, 0.0f, 100.0f);
    parameters.fadeDurMax = clampf(parameters.fadeDurMax, kFadeMinFraction, 1.0f);
    parameters.bias = clampf(parameters.bias, 0.0f, 100.0f);
    parameters.velocityFades = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.velocityFades)), 0, 1));
    parameters.mute = static_cast<float>(clampi(static_cast<int>(std::lround(parameters.mute)), 0, 1));
    return parameters;
}

void activate(EngineState& state) {
    state.currentGain = 1.0f;
    state.targetGain = 1.0f;
    state.fadeStep = 0.0f;
    state.fadeRemaining = 0;
    state.wasPlaying = false;
    state.gateWasOpen = true;
    state.cycleOriginBar = 0.0;
    state.lastBarPos = 0.0;
    state.heldNotes.fill(0);
}

void deactivate(EngineState& state) {
    state.heldNotes.fill(0);
    state.wasPlaying = false;
    state.gateWasOpen = true;
}

BlockResult processBlock(EngineState& state,
                         const Parameters& rawParameters,
                         const TransportSnapshot& transport,
                         const std::uint32_t nframes,
                         const double sampleRate,
                         const InputMidiEvent* const midiEvents,
                         const std::uint32_t midiEventCount) {
    BlockResult result {};
    if (nframes == 0) {
        return result;
    }

    const Parameters parameters = clampParameters(rawParameters);
    const bool muted = parameters.mute >= 0.5f;
    const bool velocityFades = parameters.velocityFades >= 0.5f;
    const bool hostPlaying = transport.valid &&
                             transport.playing &&
                             transport.bpm > 0.0 &&
                             transport.beatsPerBar > 0.0 &&
                             sampleRate > 0.0;

    if (!hostPlaying) {
        if (muted) {
            releaseHeldNotes(state, result, 0);
            state.wasPlaying = false;
            state.gateWasOpen = false;
            return result;
        }

        for (std::uint32_t i = 0; i < midiEventCount; ++i) {
            appendMidi(result, midiEvents[i]);
        }
        state.wasPlaying = false;
        state.gateWasOpen = true;
        return result;
    }

    const double barStep = transport.bpm / (60.0 * sampleRate * transport.beatsPerBar);
    const double framesPerBar = barStep > 0.0 ? 1.0 / barStep : 0.0;
    const double startBarPos = transport.bar + (transport.barBeat / transport.beatsPerBar);
    const bool restart = !state.wasPlaying || (startBarPos + 1e-6 < state.lastBarPos);
    if (restart) {
        releaseHeldNotes(state, result, 0);
        state.cycleOriginBar = startBarPos;
        state.gateWasOpen = true;
    }
    state.wasPlaying = true;

    std::uint32_t eventIndex = 0;
    for (std::uint32_t frame = 0; frame < nframes; ++frame) {
        const double barPos = startBarPos + (static_cast<double>(frame) * barStep);
        const double previousBarPos = frame == 0 ? startBarPos : startBarPos + (static_cast<double>(frame - 1) * barStep);
        const double barFraction = barPos - std::floor(barPos);
        const bool startsOnBoundary = frame == 0 && barFraction <= 1e-6;
        const bool crossedBoundary = frame > 0 && std::floor(previousBarPos + 1e-6) < std::floor(barPos + 1e-6);
        if (startsOnBoundary || crossedBoundary) {
            applyProbabilisticTransition(state, static_cast<std::int64_t>(std::floor(barPos + 1e-6)), parameters, framesPerBar);
        }

        if (state.fadeRemaining > 0) {
            state.currentGain += state.fadeStep;
            state.fadeRemaining -= 1;
            if (state.fadeRemaining == 0) {
                state.currentGain = state.targetGain;
                state.fadeStep = 0.0f;
            }
        }

        float gate = muted ? 0.0f : state.currentGain * euclideanGain(state, parameters, barPos);
        gate = clampf(gate, 0.0f, 1.0f);
        const bool gateOpen = gate > 0.001f;
        if (!gateOpen && state.gateWasOpen) {
            releaseHeldNotes(state, result, frame);
        }
        state.gateWasOpen = gateOpen;

        while (eventIndex < midiEventCount && midiEvents[eventIndex].frame == frame) {
            processMidiEvent(state, result, midiEvents[eventIndex], gate, velocityFades);
            ++eventIndex;
        }
    }

    while (eventIndex < midiEventCount) {
        const InputMidiEvent& event = midiEvents[eventIndex++];
        if (event.frame < nframes) {
            continue;
        }
        processMidiEvent(state, result, event, state.gateWasOpen ? state.currentGain : 0.0f, velocityFades);
    }

    state.lastBarPos = startBarPos + (static_cast<double>(nframes) * barStep);
    return result;
}

}  // namespace downspout::mmix
