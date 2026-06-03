#include "ai_coordinator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace downspout::ai_coordinator {
namespace {

struct Reader {
    std::vector<std::uint8_t> bytes {};
    std::size_t pos = 0;

    [[nodiscard]] bool canRead(const std::size_t count) const
    {
        return pos + count <= bytes.size();
    }

    [[nodiscard]] std::uint8_t readU8()
    {
        return canRead(1) ? bytes[pos++] : 0u;
    }

    [[nodiscard]] std::uint16_t readU16()
    {
        const std::uint16_t a = readU8();
        const std::uint16_t b = readU8();
        return static_cast<std::uint16_t>((a << 8u) | b);
    }

    [[nodiscard]] std::uint32_t readU32()
    {
        const std::uint32_t a = readU8();
        const std::uint32_t b = readU8();
        const std::uint32_t c = readU8();
        const std::uint32_t d = readU8();
        return (a << 24u) | (b << 16u) | (c << 8u) | d;
    }

    [[nodiscard]] std::uint32_t readVarLen()
    {
        std::uint32_t value = 0;
        for (int i = 0; i < 4 && canRead(1); ++i) {
            const std::uint8_t byte = readU8();
            value = (value << 7u) | (byte & 0x7fu);
            if ((byte & 0x80u) == 0u)
                break;
        }
        return value;
    }

    void skip(const std::size_t count)
    {
        pos = std::min(bytes.size(), pos + count);
    }
};

struct ActiveNote {
    double startBeat = 0.0;
    int velocity = 0;
};

struct NoteSummary {
    double startBeat = 0.0;
    double durationBeats = 0.0;
    int note = 0;
    int velocity = 0;
};

[[nodiscard]] bool matchText(Reader& reader, const char* text)
{
    for (int i = 0; text[i] != '\0'; ++i) {
        if (!reader.canRead(1) || reader.readU8() != static_cast<std::uint8_t>(text[i]))
            return false;
    }
    return true;
}

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] float clampf(const float value, const float minimum, const float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

[[nodiscard]] std::uint32_t updateSeed(std::uint32_t seed, const std::uint32_t value)
{
    seed ^= value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
    return seed == 0u ? 1u : seed;
}

[[nodiscard]] std::optional<std::vector<NoteSummary>> readMidiNotes(const std::string& path, int& ticksPerQuarter)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;

    Reader reader {};
    reader.bytes = std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                             std::istreambuf_iterator<char>());
    if (!matchText(reader, "MThd"))
        return std::nullopt;

    const std::uint32_t headerSize = reader.readU32();
    const std::uint16_t format = reader.readU16();
    const std::uint16_t trackCount = reader.readU16();
    const std::uint16_t division = reader.readU16();
    if (headerSize > 6)
        reader.skip(headerSize - 6u);
    if ((division & 0x8000u) != 0u || trackCount == 0u || format > 1u)
        return std::nullopt;

    ticksPerQuarter = std::max(1, static_cast<int>(division));

    std::vector<NoteSummary> notes;
    for (std::uint16_t track = 0; track < trackCount && reader.canRead(8); ++track) {
        if (!matchText(reader, "MTrk"))
            return std::nullopt;
        const std::uint32_t trackSize = reader.readU32();
        const std::size_t trackEnd = std::min(reader.bytes.size(), reader.pos + trackSize);
        std::uint32_t tick = 0;
        std::uint8_t runningStatus = 0;
        std::map<int, ActiveNote> active;

        while (reader.pos < trackEnd) {
            tick += reader.readVarLen();
            if (!reader.canRead(1))
                break;

            std::uint8_t status = reader.readU8();
            if (status < 0x80u) {
                if (runningStatus == 0u)
                    break;
                --reader.pos;
                status = runningStatus;
            } else if (status < 0xf0u) {
                runningStatus = status;
            }

            if (status == 0xffu) {
                if (!reader.canRead(2))
                    break;
                reader.skip(1);
                const std::uint32_t length = reader.readVarLen();
                reader.skip(length);
                continue;
            }
            if (status == 0xf0u || status == 0xf7u) {
                const std::uint32_t length = reader.readVarLen();
                reader.skip(length);
                continue;
            }

            const std::uint8_t type = status & 0xf0u;
            const int channel = status & 0x0fu;
            if (type == 0xc0u || type == 0xd0u) {
                reader.skip(1);
                continue;
            }
            if (!reader.canRead(2))
                break;
            const int data1 = reader.readU8();
            const int data2 = reader.readU8();

            if (type != 0x80u && type != 0x90u)
                continue;

            const int key = channel * 128 + data1;
            const double beat = static_cast<double>(tick) / static_cast<double>(ticksPerQuarter);
            if (type == 0x90u && data2 > 0) {
                active[key] = ActiveNote {beat, data2};
            } else {
                const auto it = active.find(key);
                if (it != active.end()) {
                    notes.push_back(NoteSummary {it->second.startBeat,
                                                 std::max(0.05, beat - it->second.startBeat),
                                                 data1,
                                                 it->second.velocity});
                    active.erase(it);
                }
            }
        }
        reader.pos = trackEnd;
    }

    std::stable_sort(notes.begin(), notes.end(), [](const NoteSummary& a, const NoteSummary& b) {
        return a.startBeat < b.startBeat;
    });
    return notes;
}

}  // namespace

std::optional<TuneState> analyzeMidiFileToTuneState(const std::string& path)
{
    int ticksPerQuarter = 480;
    const auto notes = readMidiNotes(path, ticksPerQuarter);
    if (!notes.has_value() || notes->empty())
        return std::nullopt;

    std::array<int, 12> pitchClassCounts {};
    int minNote = 127;
    int maxNote = 0;
    double lastBeat = 0.0;
    std::uint32_t seed = 2166136261u;
    for (const NoteSummary& note : *notes) {
        const int pc = ((note.note % 12) + 12) % 12;
        ++pitchClassCounts[static_cast<std::size_t>(pc)];
        minNote = std::min(minNote, note.note);
        maxNote = std::max(maxNote, note.note);
        lastBeat = std::max(lastBeat, note.startBeat + note.durationBeats);
        seed = updateSeed(seed, static_cast<std::uint32_t>((note.note << 16) ^ (note.velocity << 8) ^ static_cast<int>(note.startBeat * 100.0)));
    }

    TuneState state {};
    state.hasMidiContext = true;
    state.key = static_cast<int>(std::max_element(pitchClassCounts.begin(), pitchClassCounts.end()) - pitchClassCounts.begin());
    state.scale = pitchClassCounts[static_cast<std::size_t>((state.key + 3) % 12)] >
                  pitchClassCounts[static_cast<std::size_t>((state.key + 4) % 12)]
        ? "minor"
        : "major";
    if (pitchClassCounts[static_cast<std::size_t>((state.key + 10) % 12)] >
        pitchClassCounts[static_cast<std::size_t>((state.key + 11) % 12)])
        state.scale = "mixolydian";
    state.genre = "midi";
    state.beatsPerBar = 4;
    state.bars = clampi(static_cast<int>((lastBeat + 3.999) / 4.0), 1, 8);
    state.registerLow = clampi(minNote + 7, 0, 127);
    state.registerHigh = clampi(maxNote + 19, 0, 127);
    if (state.registerHigh < state.registerLow)
        std::swap(state.registerHigh, state.registerLow);
    const float noteDensity = static_cast<float>(notes->size()) / std::max(1.0f, static_cast<float>(state.bars * state.beatsPerBar));
    state.density = clampf(0.25f + noteDensity * 0.35f, 0.20f, 0.95f);
    state.risk = clampf(0.20f + static_cast<float>(maxNote - minNote) / 80.0f, 0.20f, 0.75f);
    state.seed = seed;

    std::array<int, 12> sortedPcs {};
    for (int pc = 0; pc < 12; ++pc)
        sortedPcs[static_cast<std::size_t>(pc)] = pc;
    std::stable_sort(sortedPcs.begin(), sortedPcs.end(), [&](const int a, const int b) {
        return pitchClassCounts[static_cast<std::size_t>(a)] > pitchClassCounts[static_cast<std::size_t>(b)];
    });

    for (int i = 0; i < 12 && state.guidePitchClassCount < kMaxGuidePitchClasses; ++i) {
        const int pc = sortedPcs[static_cast<std::size_t>(i)];
        if (pitchClassCounts[static_cast<std::size_t>(pc)] <= 0)
            continue;
        state.guidePitchClasses[static_cast<std::size_t>(state.guidePitchClassCount++)] = pc;
    }

    return state;
}

}  // namespace downspout::ai_coordinator
