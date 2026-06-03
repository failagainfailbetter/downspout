#include "sidecar_engine.hpp"

#include "sidecar_protocol.hpp"

#include <algorithm>
#include <cmath>

namespace downspout::sidecar {
namespace {

[[nodiscard]] int clampi(const int value, const int minimum, const int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

void appendMidi(BlockResult& result,
                const MidiEventType type,
                const std::uint32_t frame,
                const int channel,
                const int note,
                const int velocity)
{
    if (result.eventCount >= static_cast<int>(result.events.size()))
        return;

    ScheduledMidiEvent& event = result.events[result.eventCount++];
    event.type = type;
    event.frame = frame;
    event.channel = static_cast<std::uint8_t>(clampi(channel - 1, 0, 15));
    event.data1 = static_cast<std::uint8_t>(clampi(note, 0, 127));
    event.data2 = static_cast<std::uint8_t>(clampi(velocity, 0, 127));
}

void clearActiveNote(EngineState& state, BlockResult& result, const std::uint32_t frame)
{
    if (state.activeNote < 0)
        return;
    appendMidi(result, MidiEventType::noteOff, frame, state.controls.channel, state.activeNote, 0);
    state.activeNote = -1;
}

[[nodiscard]] std::uint32_t frameForBeat(const double startBeat, const double endBeat, const std::uint32_t nframes, const double beat)
{
    if (endBeat <= startBeat)
        return 0;
    const double normalized = (beat - startBeat) / (endBeat - startBeat);
    return static_cast<std::uint32_t>(clampi(static_cast<int>(std::lround(normalized * static_cast<double>(nframes))),
                                            0,
                                            static_cast<int>(nframes > 0 ? nframes - 1 : 0)));
}

}  // namespace

void activate(EngineState& state, const Controls& controls)
{
    state.controls = clampControls(controls);
    state.activeNote = -1;
    state.wasPlaying = false;
}

void setPhrase(EngineState& state, const Phrase& phrase)
{
    if (!validatePhrase(phrase, state.controls).valid)
    {
        clearPhrase(state);
        return;
    }
    state.phrase = phrase;
    state.phraseReady = true;
}

void clearPhrase(EngineState& state)
{
    state.phrase = Phrase {};
    state.phraseReady = false;
    state.activeNote = -1;
}

BlockResult processBlock(EngineState& state,
                         const Controls& rawControls,
                         const TransportSnapshot& transport,
                         const std::uint32_t nframes,
                         const double sampleRate)
{
    BlockResult result {};
    if (nframes == 0)
        return result;

    state.controls = clampControls(rawControls);
    result.phraseReady = state.phraseReady;

    const bool playing = transport.valid && transport.playing && transport.bpm > 1.0 && transport.beatsPerBar > 0.0;
    if (!playing || state.controls.mute || !state.phraseReady)
    {
        clearActiveNote(state, result, 0);
        state.wasPlaying = playing;
        return result;
    }

    const double startBeat = transport.bar * transport.beatsPerBar + transport.barBeat;
    const double blockBeats = (static_cast<double>(nframes) * transport.bpm) / (60.0 * sampleRate);
    const double endBeat = startBeat + blockBeats;
    const double phraseBeats = static_cast<double>(std::max(1, state.phrase.bars) * std::max(1, state.phrase.beatsPerBar));
    const double firstCycle = std::floor((startBeat - phraseBeats) / phraseBeats);
    const double lastCycle = std::ceil((endBeat + phraseBeats) / phraseBeats);

    for (int eventIndex = 0; eventIndex < state.phrase.eventCount; ++eventIndex)
    {
        const PhraseEvent& event = state.phrase.events[eventIndex];
        for (int cycle = static_cast<int>(firstCycle); cycle <= static_cast<int>(lastCycle); ++cycle)
        {
            const double noteOnBeat = static_cast<double>(cycle) * phraseBeats + event.beat;
            const double noteOffBeat = noteOnBeat + event.duration;
            if (noteOnBeat >= startBeat && noteOnBeat < endBeat)
            {
                const std::uint32_t frame = frameForBeat(startBeat, endBeat, nframes, noteOnBeat);
                clearActiveNote(state, result, frame);
                appendMidi(result, MidiEventType::noteOn, frame, state.controls.channel, event.note, event.velocity);
                state.activeNote = event.note;
            }
            if (noteOffBeat >= startBeat && noteOffBeat < endBeat)
            {
                const std::uint32_t frame = frameForBeat(startBeat, endBeat, nframes, noteOffBeat);
                clearActiveNote(state, result, frame);
            }
        }
    }

    std::stable_sort(result.events.begin(),
                     result.events.begin() + result.eventCount,
                     [](const ScheduledMidiEvent& a, const ScheduledMidiEvent& b) {
                         return a.frame < b.frame;
                     });

    state.wasPlaying = true;
    return result;
}

}  // namespace downspout::sidecar
