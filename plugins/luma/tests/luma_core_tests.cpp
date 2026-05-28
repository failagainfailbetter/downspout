#include "luma_processor.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool hasMessage(const downspout::luma::ProcessResult& result,
                const std::uint8_t status,
                const std::uint8_t data1,
                const std::uint8_t data2)
{
    for (std::uint32_t i = 0; i < result.eventCount; ++i)
    {
        if (result.events[i].size == 3 &&
            result.events[i].data[0] == status &&
            result.events[i].data[1] == data1 &&
            result.events[i].data[2] == data2)
        {
            return true;
        }
    }
    return false;
}

bool hasNoteOn(const downspout::luma::ProcessResult& result)
{
    for (std::uint32_t i = 0; i < result.eventCount; ++i)
    {
        if (result.events[i].size == 3 &&
            (result.events[i].data[0] & 0xf0u) == 0x90u &&
            result.events[i].data[2] > 0u &&
            result.events[i].data[0] != 0x90u)
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    using downspout::luma::MidiMessage;
    using downspout::luma::Processor;
    using downspout::luma::TransportSnapshot;
    using downspout::luma::gridToNote;
    using downspout::luma::kParamCellStart;
    using downspout::luma::kParamClear;
    using downspout::luma::kParamDensity;
    using downspout::luma::kParamEnergy;
    using downspout::luma::kParamLedFeedback;
    using downspout::luma::kParamStatusCellStart;
    using downspout::luma::kParamStatusActive;

    Processor processor;
    processor.init(48000.0);

    require(processor.getParameter(kParamStatusActive) == 0.0f, "luma should start with no active pads");

    MidiMessage pad {};
    pad.size = 3;
    pad.data[0] = 0x90;
    pad.data[1] = gridToNote(0, 0);
    pad.data[2] = 127;

    TransportSnapshot stopped {};
    auto result = processor.processBlock(64, stopped, &pad, 1);
    require(processor.getParameter(kParamCellStart) == 1.0f, "luma Launchpad pad should toggle a cell");
    require(processor.getParameter(kParamStatusCellStart) == 1.0f, "luma status pad mirror should track hardware toggles");
    require(processor.getParameter(kParamStatusActive) == 1.0f, "luma active pad status should update");
    require(hasMessage(result, 0x90, gridToNote(0, 0), downspout::luma::kLedBlue),
            "luma LED feedback should light the toggled bass pad");

    processor.setParameter(kParamDensity, 1.0f);
    processor.setParameter(kParamEnergy, 1.0f);

    TransportSnapshot transport {};
    transport.valid = true;
    transport.playing = true;
    transport.bar = 0.0;
    transport.barBeat = 0.0;
    transport.beatsPerBar = 4.0;
    transport.bpm = 120.0;
    result = processor.processBlock(4096, transport, nullptr, 0);
    require(hasNoteOn(result), "luma active cell should emit MIDI while transport runs");

    processor.setParameter(kParamClear, 1.0f);
    require(processor.getParameter(kParamStatusActive) == 0.0f, "luma clear should remove active pads");

    processor.setParameter(kParamLedFeedback, 0.0f);
    result = processor.processBlock(64, stopped, nullptr, 0);
    require(result.eventCount == 0, "luma LED disabled should suppress idle LED output");

    return 0;
}
