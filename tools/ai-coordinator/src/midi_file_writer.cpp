#include "ai_coordinator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace downspout::ai_coordinator {
namespace {

struct MidiTrackEvent {
    std::uint32_t tick = 0;
    std::array<std::uint8_t, 3> data {};
};

void appendByte(std::vector<std::uint8_t>& bytes, const std::uint8_t value)
{
    bytes.push_back(value);
}

void appendU16(std::vector<std::uint8_t>& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendText(std::vector<std::uint8_t>& bytes, const char* text)
{
    while (*text != '\0')
        bytes.push_back(static_cast<std::uint8_t>(*text++));
}

void appendVariableLength(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    std::uint8_t buffer[4] {};
    int index = 0;
    buffer[index++] = static_cast<std::uint8_t>(value & 0x7fu);
    while ((value >>= 7u) > 0u)
        buffer[index++] = static_cast<std::uint8_t>((value & 0x7fu) | 0x80u);
    while (--index >= 0)
        bytes.push_back(buffer[index]);
}

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

bool writeMidiFile(const std::string& path,
                   const downspout::sidecar::Phrase& phrase,
                   const int tempo,
                   const int channel,
                   const int beatsPerBar)
{
    constexpr std::uint16_t kTicksPerQuarter = 480;
    const int midiChannel = clampi(channel - 1, 0, 15);
    const int clampedTempo = clampi(tempo, 30, 300);
    const std::uint32_t microsecondsPerQuarter = static_cast<std::uint32_t>(60000000 / clampedTempo);

    std::vector<MidiTrackEvent> events;
    events.reserve(static_cast<std::size_t>(phrase.eventCount) * 2u);
    for (int i = 0; i < phrase.eventCount; ++i) {
        const downspout::sidecar::PhraseEvent& event = phrase.events[static_cast<std::size_t>(i)];
        const auto onTick = static_cast<std::uint32_t>(std::max(0.0f, std::round(event.beat * static_cast<float>(kTicksPerQuarter))));
        const auto offTick = static_cast<std::uint32_t>(std::max(0.0f, std::round((event.beat + event.duration) * static_cast<float>(kTicksPerQuarter))));
        events.push_back({onTick, {static_cast<std::uint8_t>(0x90 | midiChannel), static_cast<std::uint8_t>(event.note), static_cast<std::uint8_t>(event.velocity)}});
        events.push_back({std::max(onTick + 1u, offTick), {static_cast<std::uint8_t>(0x80 | midiChannel), static_cast<std::uint8_t>(event.note), 0u}});
    }

    std::stable_sort(events.begin(), events.end(), [](const MidiTrackEvent& left, const MidiTrackEvent& right) {
        return left.tick < right.tick;
    });

    std::vector<std::uint8_t> track;
    appendVariableLength(track, 0);
    appendByte(track, 0xff);
    appendByte(track, 0x51);
    appendByte(track, 0x03);
    appendByte(track, static_cast<std::uint8_t>((microsecondsPerQuarter >> 16u) & 0xffu));
    appendByte(track, static_cast<std::uint8_t>((microsecondsPerQuarter >> 8u) & 0xffu));
    appendByte(track, static_cast<std::uint8_t>(microsecondsPerQuarter & 0xffu));

    appendVariableLength(track, 0);
    appendByte(track, 0xff);
    appendByte(track, 0x58);
    appendByte(track, 0x04);
    appendByte(track, static_cast<std::uint8_t>(clampi(beatsPerBar, 1, 12)));
    appendByte(track, 0x02);
    appendByte(track, 24);
    appendByte(track, 8);

    std::uint32_t previousTick = 0;
    for (const MidiTrackEvent& event : events) {
        appendVariableLength(track, event.tick - previousTick);
        appendByte(track, event.data[0]);
        appendByte(track, event.data[1]);
        appendByte(track, event.data[2]);
        previousTick = event.tick;
    }

    appendVariableLength(track, 0);
    appendByte(track, 0xff);
    appendByte(track, 0x2f);
    appendByte(track, 0x00);

    std::vector<std::uint8_t> file;
    appendText(file, "MThd");
    appendU32(file, 6);
    appendU16(file, 0);
    appendU16(file, 1);
    appendU16(file, kTicksPerQuarter);
    appendText(file, "MTrk");
    appendU32(file, static_cast<std::uint32_t>(track.size()));
    file.insert(file.end(), track.begin(), track.end());

    std::ofstream output(path, std::ios::binary);
    if (!output)
        return false;
    output.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    return output.good();
}

}  // namespace downspout::ai_coordinator
