#pragma once

#include <array>
#include <cstdint>

namespace downspout::sidecar {

inline constexpr int kMaxPhraseEvents = 256;
inline constexpr int kMaxScheduledMidiEvents = kMaxPhraseEvents * 2 + 16;
inline constexpr int kPhraseStateVersion = 1;

enum class RegisterId : std::int32_t {
    low = 0,
    mid,
    high,
    custom,
    count
};

enum class MidiEventType : std::uint8_t {
    noteOff = 0,
    noteOn
};

struct Controls {
    int channel = 1;
    int bars = 4;
    RegisterId reg = RegisterId::mid;
    int registerLow = 55;
    int registerHigh = 82;
    float density = 0.50f;
    float risk = 0.35f;
    float humanize = 0.0f;
    bool mute = false;
};

struct PhraseEvent {
    float beat = 0.0f;
    float duration = 0.5f;
    int note = 60;
    int velocity = 90;
};

struct Phrase {
    int version = kPhraseStateVersion;
    int bars = 4;
    int beatsPerBar = 4;
    int eventCount = 0;
    std::array<PhraseEvent, kMaxPhraseEvents> events {};
};

struct TransportSnapshot {
    bool valid = false;
    bool playing = false;
    double bar = 0.0;
    double barBeat = 0.0;
    double beatsPerBar = 4.0;
    double bpm = 120.0;
};

struct ScheduledMidiEvent {
    MidiEventType type = MidiEventType::noteOn;
    std::uint32_t frame = 0;
    std::uint8_t channel = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
};

struct ValidationResult {
    bool valid = true;
    int firstInvalidEvent = -1;
};

}  // namespace downspout::sidecar
