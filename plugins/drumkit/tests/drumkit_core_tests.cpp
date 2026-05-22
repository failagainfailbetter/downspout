#include "drumkit_engine.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

float renderEnergy(downspout::drumkit::Engine& engine, const int frames)
{
    float energy = 0.0f;
    for (int i = 0; i < frames; ++i)
    {
        const auto frame = engine.processStereo();
        energy += std::fabs(frame.left) + std::fabs(frame.right);
    }
    return energy;
}

void noteOn(downspout::drumkit::Engine& engine, const std::uint8_t note, const std::uint8_t velocity = 110)
{
    const std::uint8_t data[] = {0x99, note, velocity};
    engine.handleMidi(data, 3);
}

void allNotesOff(downspout::drumkit::Engine& engine)
{
    const std::uint8_t data[] = {0xb0, 123, 0};
    engine.handleMidi(data, 3);
}

void kickRendersAudio()
{
    downspout::drumkit::Engine engine {48000.0f};
    noteOn(engine, 36);
    assert(renderEnergy(engine, 512) > 0.01f);
}

void mutedKickDoesNotTrigger()
{
    downspout::drumkit::Engine engine {48000.0f};
    engine.setInstrumentMuted(downspout::drumkit::InstrumentId::Kick, true);
    noteOn(engine, 36);
    assert(renderEnergy(engine, 512) < 0.0001f);

    engine.setInstrumentMuted(downspout::drumkit::InstrumentId::Kick, false);
    noteOn(engine, 36);
    assert(renderEnergy(engine, 512) > 0.01f);
}

void closedHatChokesOpenHatEvenWhenClosedHatMuted()
{
    downspout::drumkit::Engine engine {48000.0f};
    engine.setParameter(downspout::drumkit::kParamMasterReverb, 0.0f);
    engine.setInstrumentMuted(downspout::drumkit::InstrumentId::ClosedHH, true);

    noteOn(engine, 46);
    const float openEnergy = renderEnergy(engine, 96);
    noteOn(engine, 42);
    const float postChokeEnergy = renderEnergy(engine, 512);

    assert(openEnergy > 0.001f);
    assert(postChokeEnergy < openEnergy * 5.0f);
}

void allNotesOffStopsVoices()
{
    downspout::drumkit::Engine engine {48000.0f};
    engine.setParameter(downspout::drumkit::kParamMasterReverb, 0.0f);
    noteOn(engine, 51);
    assert(renderEnergy(engine, 64) > 0.001f);
    allNotesOff(engine);
    assert(renderEnergy(engine, 256) < 0.0001f);
}

} // namespace

int main()
{
    kickRendersAudio();
    mutedKickDoesNotTrigger();
    closedHatChokesOpenHatEvenWhenClosedHatMuted();
    allNotesOffStopsVoices();

    std::cout << "drumkit core tests passed\n";
    return 0;
}
