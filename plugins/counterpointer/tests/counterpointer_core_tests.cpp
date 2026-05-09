#include "counterpointer_engine.hpp"
#include "counterpointer_serialization.hpp"
#include "counterpointer_transport.hpp"

#include <array>
#include <cassert>

using namespace downspout::counterpointer;

namespace {

InputMidiEvent makeEvent(const std::uint32_t frame,
                         const std::uint8_t status,
                         const std::uint8_t data1,
                         const std::uint8_t data2)
{
    InputMidiEvent event {};
    event.frame = frame;
    event.size = 3;
    event.data[0] = status;
    event.data[1] = data1;
    event.data[2] = data2;
    return event;
}

TransportSnapshot runningTransport(const double bar, const double barBeat = 0.0)
{
    TransportSnapshot transport;
    transport.valid = true;
    transport.playing = true;
    transport.bar = bar;
    transport.barBeat = barBeat;
    transport.beatsPerBar = 4.0;
    transport.beatType = 4.0;
    transport.bpm = 120.0;
    return transport;
}

bool hasNoteOn(const BlockResult& result)
{
    for (int i = 0; i < result.eventCount; ++i)
    {
        const ScheduledMidiEvent& event = result.events[static_cast<std::size_t>(i)];
        if (event.size >= 3 && (event.data[0] & 0xf0) == 0x90 && event.data[2] > 0)
            return true;
    }
    return false;
}

int countNoteOns(const BlockResult& result)
{
    int count = 0;
    for (int i = 0; i < result.eventCount; ++i)
    {
        const ScheduledMidiEvent& event = result.events[static_cast<std::size_t>(i)];
        if (event.size >= 3 && (event.data[0] & 0xf0) == 0x90 && event.data[2] > 0)
            ++count;
    }
    return count;
}

int firstNoteOnPitch(const BlockResult& result)
{
    for (int i = 0; i < result.eventCount; ++i)
    {
        const ScheduledMidiEvent& event = result.events[static_cast<std::size_t>(i)];
        if (event.size >= 3 && (event.data[0] & 0xf0) == 0x90 && event.data[2] > 0)
            return event.data[1];
    }
    return -1;
}

void testTransportHelpers()
{
    Controls controls = defaultControls();
    controls.cycle_bars = 2;
    controls.granularity = GRANULARITY_BEAT;
    assert(counterpointer_segment_count_for_controls(controls, 4.0) == 8);
    assert(counterpointer_segment_index_for_time(controls, 4.0, 8, 0.0) == 0);
    assert(counterpointer_segment_index_for_time(controls, 4.0, 8, 3.25) == 3);
    assert(counterpointer_segment_index_for_time(controls, 4.0, 8, 7.99) == 7);
}

void testStoppedTransportPassThrough()
{
    EngineState state;
    activate(state);

    Controls controls = defaultControls();
    controls.pass_input = true;
    const InputMidiEvent event = makeEvent(0, 0x90, 60, 100);

    TransportSnapshot transport;
    transport.valid = false;

    const BlockResult result = processBlock(state, controls, transport, 256, 48000.0, &event, 1);
    assert(!result.ready);
    assert(result.eventCount == 1);
    assert(result.events[0].data[0] == 0x90);
    assert(result.events[0].data[1] == 60);
}

void testSerializationRoundTrip()
{
    Controls controls = defaultControls();
    controls.key = 9;
    controls.scale = SCALE_DORIAN;
    controls.cycle_bars = 3;
    controls.granularity = GRANULARITY_HALF_BAR;
    controls.follow = 0.24f;
    controls.counter = 0.81f;
    controls.short_random = 0.33f;
    controls.long_random = 0.44f;
    controls.density = 0.72f;
    controls.rhythm_follow = 0.61f;
    controls.syncopation = 0.35f;
    controls.consonance = 0.93f;
    controls.embellish = 0.84f;
    controls.regularity = 0.91f;
    controls.reg = REGISTER_HIGH;
    controls.span = 0.42f;
    controls.gate = 0.58f;
    controls.velocity_follow = 0.25f;
    controls.pass_input = false;
    controls.output_channel = 5;
    controls.action_learn = 7;
    controls.freeze = true;

    const auto controlsRoundTrip = deserializeControls(serializeControls(controls));
    assert(controlsRoundTrip.has_value());
    assert(controlsRoundTrip->key == 9);
    assert(controlsRoundTrip->scale == SCALE_DORIAN);
    assert(controlsRoundTrip->cycle_bars == 3);
    assert(controlsRoundTrip->reg == REGISTER_HIGH);
    assert(controlsRoundTrip->embellish > 0.83f && controlsRoundTrip->embellish < 0.85f);
    assert(controlsRoundTrip->regularity > 0.90f && controlsRoundTrip->regularity < 0.92f);
    assert(!controlsRoundTrip->pass_input);
    assert(controlsRoundTrip->output_channel == 5);
    assert(controlsRoundTrip->freeze);

    PhraseState phrase;
    phrase.segmentCount = 2;
    phrase.ready = true;
    phrase.steps[0].active = true;
    phrase.steps[0].note = 72;
    phrase.steps[0].velocity = 100;
    phrase.steps[0].onset = 0.25;
    phrase.steps[0].gate = 0.50;
    phrase.steps[0].hitCount = 2;
    phrase.steps[0].hits[0].active = true;
    phrase.steps[0].hits[0].note = 72;
    phrase.steps[0].hits[0].velocity = 100;
    phrase.steps[0].hits[0].onset = 0.25;
    phrase.steps[0].hits[0].gate = 0.50;
    phrase.steps[0].hits[1].active = true;
    phrase.steps[0].hits[1].note = 76;
    phrase.steps[0].hits[1].velocity = 88;
    phrase.steps[0].hits[1].onset = 0.72;
    phrase.steps[0].hits[1].gate = 0.30;
    phrase.steps[1].active = false;
    phrase.steps[1].note = 74;

    const auto phraseRoundTrip = deserializePhraseState(serializePhraseState(phrase));
    assert(phraseRoundTrip.has_value());
    assert(phraseRoundTrip->ready);
    assert(phraseRoundTrip->segmentCount == 2);
    assert(phraseRoundTrip->steps[0].active);
    assert(phraseRoundTrip->steps[0].note == 72);
    assert(phraseRoundTrip->steps[0].hitCount == 2);
    assert(phraseRoundTrip->steps[0].hits[1].note == 76);
    assert(phraseRoundTrip->steps[1].note == 74);

    VariationState variation = defaultVariationState();
    variation.completed_cycles = 10;
    variation.last_mutation_cycle = 8;
    variation.mutation_serial = 4;
    const auto variationRoundTrip = deserializeVariationState(serializeVariationState(variation));
    assert(variationRoundTrip.has_value());
    assert(variationRoundTrip->completed_cycles == 10);
    assert(variationRoundTrip->mutation_serial == 4);
}

void testLearnsIncomingPatternAndEmitsCounterMelody()
{
    EngineState state;
    activate(state);

    Controls controls = defaultControls();
    controls.cycle_bars = 1;
    controls.granularity = GRANULARITY_BEAT;
    controls.pass_input = false;
    controls.output_channel = 2;
    controls.density = 1.0f;
    controls.rhythm_follow = 0.8f;

    constexpr std::uint32_t frameCount = 96000;
    const std::array<InputMidiEvent, 8> firstCycle = {{
        makeEvent(0, 0x90, 48, 100),
        makeEvent(24000, 0x80, 48, 0),
        makeEvent(24000, 0x90, 50, 96),
        makeEvent(48000, 0x80, 50, 0),
        makeEvent(48000, 0x90, 52, 96),
        makeEvent(72000, 0x80, 52, 0),
        makeEvent(72000, 0x90, 55, 104),
        makeEvent(95999, 0x80, 55, 0),
    }};

    const BlockResult learned = processBlock(state,
                                             controls,
                                             runningTransport(0.0),
                                             frameCount,
                                             48000.0,
                                             firstCycle.data(),
                                             static_cast<std::uint32_t>(firstCycle.size()));
    assert(learned.ready);
    assert(state.playbackPhrase.ready);

    const BlockResult playback = processBlock(state,
                                              controls,
                                              runningTransport(1.0),
                                              24000,
                                              48000.0,
                                              nullptr,
                                              0);
    assert(playback.ready);
    assert(hasNoteOn(playback));
    assert(firstNoteOnPitch(playback) >= 0);
    assert((playback.events[0].data[0] & 0x0f) == 1 || playback.eventCount > 1);
}

void testDeterministicForSameInput()
{
    Controls controls = defaultControls();
    controls.cycle_bars = 1;
    controls.granularity = GRANULARITY_BEAT;
    controls.pass_input = false;
    controls.density = 1.0f;
    controls.short_random = 0.35f;

    constexpr std::uint32_t frameCount = 96000;
    const std::array<InputMidiEvent, 4> firstCycle = {{
        makeEvent(0, 0x90, 60, 100),
        makeEvent(24000, 0x80, 60, 0),
        makeEvent(48000, 0x90, 67, 100),
        makeEvent(72000, 0x80, 67, 0),
    }};

    EngineState a;
    EngineState b;
    activate(a);
    activate(b);

    const BlockResult learnedA =
        processBlock(a, controls, runningTransport(0.0), frameCount, 48000.0, firstCycle.data(), firstCycle.size());
    const BlockResult learnedB =
        processBlock(b, controls, runningTransport(0.0), frameCount, 48000.0, firstCycle.data(), firstCycle.size());
    assert(learnedA.ready);
    assert(learnedB.ready);
    const BlockResult outA = processBlock(a, controls, runningTransport(1.0), 48000, 48000.0, nullptr, 0);
    const BlockResult outB = processBlock(b, controls, runningTransport(1.0), 48000, 48000.0, nullptr, 0);

    assert(outA.eventCount == outB.eventCount);
    for (int i = 0; i < outA.eventCount; ++i)
    {
        assert(outA.events[i].frame == outB.events[i].frame);
        assert(outA.events[i].size == outB.events[i].size);
        assert(outA.events[i].data == outB.events[i].data);
    }
}

void testEmbellishCanOutnumberInputNotes()
{
    EngineState state;
    activate(state);

    Controls controls = defaultControls();
    controls.cycle_bars = 1;
    controls.granularity = GRANULARITY_BAR;
    controls.pass_input = false;
    controls.density = 1.0f;
    controls.embellish = 1.0f;
    controls.regularity = 1.0f;

    constexpr std::uint32_t frameCount = 96000;
    const std::array<InputMidiEvent, 2> firstCycle = {{
        makeEvent(0, 0x90, 48, 100),
        makeEvent(95999, 0x80, 48, 0),
    }};

    const BlockResult learned =
        processBlock(state, controls, runningTransport(0.0), frameCount, 48000.0, firstCycle.data(), firstCycle.size());
    assert(learned.ready);

    const BlockResult playback = processBlock(state,
                                              controls,
                                              runningTransport(1.0),
                                              frameCount,
                                              48000.0,
                                              nullptr,
                                              0);
    assert(playback.ready);
    assert(countNoteOns(playback) > 1);
}

void testFreezeKeepsLearnedPhrase()
{
    EngineState state;
    activate(state);

    Controls controls = defaultControls();
    controls.cycle_bars = 1;
    controls.granularity = GRANULARITY_BEAT;
    controls.pass_input = false;
    controls.density = 1.0f;

    constexpr std::uint32_t frameCount = 96000;
    const std::array<InputMidiEvent, 2> firstCycle = {{
        makeEvent(0, 0x90, 60, 100),
        makeEvent(95999, 0x80, 60, 0),
    }};
    const BlockResult learned =
        processBlock(state, controls, runningTransport(0.0), frameCount, 48000.0, firstCycle.data(), firstCycle.size());
    assert(learned.ready);
    const int learnedPitch = static_cast<int>(state.playbackPhrase.steps[0].note);

    controls.freeze = true;
    const std::array<InputMidiEvent, 2> differentCycle = {{
        makeEvent(0, 0x90, 36, 100),
        makeEvent(95999, 0x80, 36, 0),
    }};
    const BlockResult frozen =
        processBlock(state, controls, runningTransport(1.0), frameCount, 48000.0, differentCycle.data(), differentCycle.size());
    assert(frozen.ready);
    assert(static_cast<int>(state.playbackPhrase.steps[0].note) == learnedPitch);
}

}  // namespace

int main()
{
    testTransportHelpers();
    testStoppedTransportPassThrough();
    testSerializationRoundTrip();
    testLearnsIncomingPatternAndEmitsCounterMelody();
    testDeterministicForSameInput();
    testEmbellishCanOutnumberInputNotes();
    testFreezeKeepsLearnedPhrase();
    return 0;
}
