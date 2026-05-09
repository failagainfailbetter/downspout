#pragma once

#include "counterpointer_core_types.hpp"

#include <array>

namespace downspout::counterpointer {

struct EngineState {
    Controls controls = defaultControls();
    Controls previousControls = defaultControls();
    bool controlsInitialized = false;

    std::array<SegmentCapture, kMaxSegments> capture {};
    PhraseState basePhrase {};
    PhraseState playbackPhrase {};
    VariationState variation = defaultVariationState();

    std::array<bool, 128> heldNotes {};
    std::array<std::uint8_t, 128> heldVelocity {};
    bool wasPlaying = false;
    double lastAbsBeatsStart = 0.0;
    int lastInputChannel = 1;

    bool activeOutput = false;
    std::uint8_t activeOutputNote = 60;
    int activeOutputChannel = 1;
    bool noteOffPending = false;
    double pendingNoteOffBeat = 0.0;
    bool noteOnPending = false;
    double pendingNoteOnBeat = 0.0;
    PhraseStep pendingStep {};
};

struct BlockResult {
    std::array<ScheduledMidiEvent, kMaxScheduledMidiEvents> events {};
    int eventCount = 0;
    bool ready = false;
};

void activate(EngineState& state);
void deactivate(EngineState& state);
[[nodiscard]] BlockResult processBlock(EngineState& state,
                                       const Controls& controls,
                                       const TransportSnapshot& transport,
                                       std::uint32_t nframes,
                                       double sampleRate,
                                       const InputMidiEvent* midiEvents,
                                       std::uint32_t midiEventCount);

}  // namespace downspout::counterpointer
