#pragma once

#include "downspout/meter.hpp"

#include <array>
#include <cstdint>

namespace downspout::mmix {

inline constexpr int kMaxMidiMessageData = 4;
inline constexpr int kMaxScheduledMidiEvents = 2048;
inline constexpr int kHeldNoteCount = 16 * 128;
inline constexpr float kFadeMinFraction = 0.125f;

struct Parameters {
    float totalBars = 128.0f;
    float division = 16.0f;
    float steps = 8.0f;
    float offset = 0.0f;
    float fadeBars = 0.0f;
    float granularity = 4.0f;
    float maintain = 50.0f;
    float fade = 25.0f;
    float cut = 25.0f;
    float fadeDurMax = 1.0f;
    float bias = 50.0f;
    float velocityFades = 1.0f;
    float mute = 0.0f;
};

struct TransportSnapshot {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double beatType = 4.0;
    double bpm = 120.0;
    ::downspout::Meter meter {};
};

struct MidiMessage {
    std::uint32_t frame = 0;
    std::uint8_t size = 0;
    std::array<std::uint8_t, kMaxMidiMessageData> data {};
};

using InputMidiEvent = MidiMessage;
using ScheduledMidiEvent = MidiMessage;

struct BlockResult {
    std::array<ScheduledMidiEvent, kMaxScheduledMidiEvents> events {};
    int eventCount = 0;
    bool overflow = false;
};

struct EngineState {
    float currentGain = 1.0f;
    float targetGain = 1.0f;
    float fadeStep = 0.0f;
    std::uint32_t fadeRemaining = 0;
    std::uint32_t rngState = 0x12345678u;
    bool wasPlaying = false;
    bool gateWasOpen = true;
    double cycleOriginBar = 0.0;
    double lastBarPos = 0.0;
    std::array<std::uint8_t, kHeldNoteCount> heldNotes {};
};

}  // namespace downspout::mmix
