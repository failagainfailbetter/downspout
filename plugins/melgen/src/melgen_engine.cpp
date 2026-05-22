#include "melgen_engine.hpp"

#include "melgen_pattern.hpp"
#include "melgen_transport.hpp"
#include "melgen_variation.hpp"

#include <cmath>
#include <cstdint>

namespace downspout::melgen {
namespace {

[[nodiscard]] int clampi(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

void appendMidi(BlockResult& result,
                MidiEventType type,
                std::uint32_t frame,
                int channel,
                int data1,
                int data2) {
    if (result.eventCount >= kMaxScheduledMidiEvents) {
        return;
    }

    ScheduledMidiEvent& event = result.events[result.eventCount++];
    event.type = type;
    event.frame = frame;
    event.channel = static_cast<std::uint8_t>(clampi(channel - 1, 0, 15));
    event.data1 = static_cast<std::uint8_t>(clampi(data1, 0, 127));
    event.data2 = static_cast<std::uint8_t>(clampi(data2, 0, 127));
}

void emitNoteOff(BlockResult& result, const EngineState& state, std::uint32_t frame, int note) {
    if (note < 0) {
        return;
    }
    appendMidi(result, MidiEventType::noteOff, frame, state.controls.channel, note, 0);
}

void emitNoteOn(BlockResult& result, const EngineState& state, std::uint32_t frame, int note, int velocity) {
    appendMidi(result, MidiEventType::noteOn, frame, state.controls.channel, note, velocity);
}

[[nodiscard]] bool isNoteOn(const InputMidiEvent& event)
{
    return event.size >= 3 && (event.data[0] & 0xf0u) == 0x90u && event.data[2] > 0;
}

[[nodiscard]] bool isNoteOff(const InputMidiEvent& event)
{
    return event.size >= 3 &&
           (((event.data[0] & 0xf0u) == 0x80u) || ((event.data[0] & 0xf0u) == 0x90u && event.data[2] == 0));
}

[[nodiscard]] bool isAllNotesOff(const InputMidiEvent& event)
{
    return event.size >= 3 && (event.data[0] & 0xf0u) == 0xb0u && (event.data[1] == 120 || event.data[1] == 123);
}

void consumeInputMidi(EngineState& state, const InputMidiEvent& event)
{
    if (isNoteOn(event)) {
        state.followNote = clampi(event.data[1], 0, 127);
    } else if (isNoteOff(event) && state.followNote == static_cast<int>(event.data[1])) {
        state.followNote = -1;
    } else if (isAllNotesOff(event)) {
        state.followNote = -1;
    }
}

void consumeInputUntil(EngineState& state,
                       const InputMidiEvent* midiEvents,
                       std::uint32_t midiEventCount,
                       std::uint32_t& inputIndex,
                       std::uint32_t frame)
{
    if (midiEvents == nullptr) {
        return;
    }
    while (inputIndex < midiEventCount && midiEvents[inputIndex].frame <= frame) {
        consumeInputMidi(state, midiEvents[inputIndex]);
        ++inputIndex;
    }
}

[[nodiscard]] std::uint32_t hashFollow(const EngineState& state, const int sourceNote, const int localStep)
{
    std::uint32_t value = state.controls.seed;
    value ^= static_cast<std::uint32_t>(sourceNote + 129) * 2246822519u;
    value ^= static_cast<std::uint32_t>(state.followNote + 257) * 3266489917u;
    value ^= static_cast<std::uint32_t>(localStep + state.pattern.generationSerial * 17) * 668265263u;
    value ^= value >> 15;
    value *= 2246822519u;
    value ^= value >> 13;
    return value;
}

[[nodiscard]] int applyFollow(const EngineState& state, const int sourceNote, const int localStep)
{
    const float follow = state.controls.follow;
    if (follow <= 0.001f || state.followNote < 0) {
        return sourceNote;
    }

    int target = state.followNote;
    while (target < sourceNote - 12) {
        target += 12;
    }
    while (target > sourceNote + 12) {
        target -= 12;
    }

    const float blend = follow * 0.72f;
    int shaped = sourceNote + static_cast<int>(std::lround(static_cast<float>(target - sourceNote) * blend));
    const std::uint32_t hash = hashFollow(state, sourceNote, localStep);
    if (follow > 0.20f && (hash % 100u) < static_cast<std::uint32_t>(follow * 36.0f)) {
        shaped += static_cast<int>((hash / 101u) % 3u) - 1;
    }

    return nearestScaleNote(state.controls, shaped, sourceNote - 12, sourceNote + 12);
}

void clearActiveNote(EngineState& state, BlockResult& result, std::uint32_t frame) {
    if (state.activeNote >= 0) {
        emitNoteOff(result, state, frame, state.activeNote);
        state.activeNote = -1;
    }
}

void syncNoteStateToPosition(EngineState& state, BlockResult& result, double localStepPos) {
    const NoteEvent* event = state.patternValid ? findActiveEvent(state.pattern, localStepPos) : nullptr;
    const int localStep = state.pattern.patternSteps > 0
        ? clampi(static_cast<int>(std::floor(localStepPos)), 0, state.pattern.patternSteps - 1)
        : 0;
    const int shouldNote = event ? applyFollow(state, event->note, localStep) : -1;
    if (state.activeNote >= 0 && state.activeNote != shouldNote) {
        emitNoteOff(result, state, 0, state.activeNote);
        state.activeNote = -1;
    }
    if (shouldNote >= 0 && state.activeNote < 0) {
        emitNoteOn(result, state, 0, shouldNote, event->velocity);
        state.activeNote = shouldNote;
    }
}

void resetTransportState(EngineState& state) {
    state.wasPlaying = false;
    state.lastTransportStep = -1;
}

void handleStoppedTransport(EngineState& state, BlockResult& result) {
    clearActiveNote(state, result, 0);
    resetTransportState(state);
}

void handleTransportRestart(EngineState& state, BlockResult& result, double absStepsStart) {
    clearActiveNote(state, result, 0);
    const double localStep = localStepFromAbsolute(state.pattern, absStepsStart);
    syncNoteStateToPosition(state, result, localStep);
}

[[nodiscard]] ::downspout::Meter resolvedMeterFor(const EngineState& state, const TransportSnapshot& transport) {
    if (transport.valid && transport.beatsPerBar > 0.0) {
        return ::downspout::sanitizeMeter(transport.meter);
    }
    if (state.pattern.stepsPerBar > 0) {
        return ::downspout::sanitizeMeter(state.pattern.meter);
    }
    return ::downspout::Meter {};
}

void updatePatternIfNeeded(EngineState& state,
                           const Controls& fresh,
                           const ::downspout::Meter& targetMeter) {
    const bool paramsChanged = structuralControlsChanged(fresh, state.previousControls);
    const bool triggerNew = fresh.actionNew != state.previousControls.actionNew;
    const bool triggerNotes = fresh.actionNotes != state.previousControls.actionNotes;
    const bool triggerRhythm = fresh.actionRhythm != state.previousControls.actionRhythm;
    const bool varyJustEnabled = state.previousControls.vary <= 0.0001f && fresh.vary > 0.0001f;
    const bool meterChanged = state.patternValid && !::downspout::metersEqual(state.pattern.meter, targetMeter);

    state.controls = fresh;

    if (!state.patternValid || paramsChanged || triggerNew || meterChanged) {
        regeneratePattern(state.pattern, state.controls, targetMeter, true, true);
        state.patternValid = true;
        resetVariationProgress(state.variation);
    } else if (triggerRhythm) {
        regeneratePattern(state.pattern, state.controls, targetMeter, true, false);
        resetVariationProgress(state.variation);
    } else if (triggerNotes) {
        regeneratePattern(state.pattern, state.controls, targetMeter, false, true);
        resetVariationProgress(state.variation);
    } else if (varyJustEnabled) {
        resetVariationProgress(state.variation);
    }

    state.previousControls = fresh;
}

void processBoundary(EngineState& state,
                     BlockResult& result,
                     std::uint32_t nframes,
                     double absStepsStart,
                     double absStepsEnd,
                     std::int64_t boundary,
                     const ::downspout::Meter& meter,
                     double beatsPerBar) {
    const std::uint32_t frame = frameForBoundary(absStepsStart, absStepsEnd, nframes, boundary);
    const int localStep = localStepForBoundary(state.pattern, boundary);

    if (localStep == 0 && applyLoopVariation(state.pattern, state.variation, state.controls, meter, beatsPerBar)) {
        clearActiveNote(state, result, frame);
    }

    if (anyEventEndsAt(state.pattern, localStep)) {
        clearActiveNote(state, result, frame);
    }

    const NoteEvent* startEvent = findEventStartingAt(state.pattern, localStep);
    if (startEvent) {
        const std::uint32_t onFrame = static_cast<std::uint32_t>(
            clampi(static_cast<int>(frame) + kSafetyGapSamples, 0, static_cast<int>(nframes) - 1));
        const int note = applyFollow(state, startEvent->note, localStep);
        clearActiveNote(state, result, frame);
        emitNoteOn(result, state, onFrame, note, startEvent->velocity);
        state.activeNote = note;
    }
}

}  // namespace

void activate(EngineState& state, const Controls& controls) {
    state.activeNote = -1;
    state.followNote = -1;
    resetTransportState(state);
    state.controls = clampControls(controls);
    state.previousControls = state.controls;
    if (!state.patternValid) {
        regeneratePattern(state.pattern, state.controls, ::downspout::Meter {}, true, true);
        state.patternValid = true;
        resetVariationProgress(state.variation);
    }
}

void deactivate(EngineState& state) {
    state.activeNote = -1;
    state.followNote = -1;
    resetTransportState(state);
}

BlockResult processBlock(EngineState& state,
                         const Controls& controls,
                         const TransportSnapshot& transport,
                         std::uint32_t nframes,
                         double sampleRate) {
    return processBlock(state, controls, transport, nframes, sampleRate, nullptr, 0);
}

BlockResult processBlock(EngineState& state,
                         const Controls& controls,
                         const TransportSnapshot& transport,
                         std::uint32_t nframes,
                         double sampleRate,
                         const InputMidiEvent* midiEvents,
                         std::uint32_t midiEventCount) {
    BlockResult result;
    if (nframes == 0) {
        return result;
    }

    const Controls freshControls = clampControls(controls);
    const ::downspout::Meter targetMeter = resolvedMeterFor(state, transport);
    updatePatternIfNeeded(state, freshControls, targetMeter);
    if (state.controls.follow <= 0.001f) {
        state.followNote = -1;
        midiEvents = nullptr;
        midiEventCount = 0;
    }

    const bool playing = transport.valid && transport.playing && transport.bpm > 0.0 && transport.beatsPerBar > 0.0;
    if (!playing || !state.patternValid) {
        handleStoppedTransport(state, result);
        return result;
    }

    const int stepsPerBeat = state.pattern.stepsPerBeat;
    const double absBeatsStart = transport.bar * transport.beatsPerBar + transport.barBeat;
    const double absBeatsStep = (static_cast<double>(nframes) * transport.bpm) / (60.0 * sampleRate);
    const double absStepsStart = absBeatsStart * static_cast<double>(stepsPerBeat);
    const double absStepsEnd = (absBeatsStart + absBeatsStep) * static_cast<double>(stepsPerBeat);
    const std::int64_t startStepFloor = static_cast<std::int64_t>(std::floor(absStepsStart + 1e-9));
    std::uint32_t inputIndex = 0;

    if (transportRestartDetected(state.wasPlaying, state.lastTransportStep, startStepFloor)) {
        consumeInputUntil(state, midiEvents, midiEventCount, inputIndex, 0);
        handleTransportRestart(state, result, absStepsStart);
    }

    state.wasPlaying = true;
    state.lastTransportStep = startStepFloor;

    std::int64_t boundary = static_cast<std::int64_t>(std::floor(absStepsStart)) + 1;
    const std::int64_t boundaryEnd = static_cast<std::int64_t>(std::floor(absStepsEnd + 1e-9));

    while (boundary <= boundaryEnd) {
        const std::uint32_t frame = frameForBoundary(absStepsStart, absStepsEnd, nframes, boundary);
        consumeInputUntil(state, midiEvents, midiEventCount, inputIndex, frame);
        processBoundary(state, result, nframes, absStepsStart, absStepsEnd, boundary, targetMeter, transport.beatsPerBar);
        boundary += 1;
    }
    consumeInputUntil(state, midiEvents, midiEventCount, inputIndex, nframes - 1);

    return result;
}

}  // namespace downspout::melgen
