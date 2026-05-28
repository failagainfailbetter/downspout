#include "lifeform_processor.hpp"

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

bool containsNoteOn(const downspout::lifeform::ProcessResult& result)
{
    for (std::uint32_t i = 0; i < result.eventCount; ++i)
    {
        if (result.events[i].size == 3 &&
            (result.events[i].data[0] & 0xf0u) == 0x90u &&
            result.events[i].data[2] > 0u)
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    using downspout::lifeform::MidiMessage;
    using downspout::lifeform::Processor;
    using downspout::lifeform::TransportSnapshot;
    using downspout::lifeform::gridToNote;
    using downspout::lifeform::kParamCellStart;
    using downspout::lifeform::kParamClockMode;
    using downspout::lifeform::kParamLedFeedback;
    using downspout::lifeform::kParamRunning;
    using downspout::lifeform::kParamSeed;
    using downspout::lifeform::kParamStatusCellStart;
    using downspout::lifeform::kParamStatusGeneration;
    using downspout::lifeform::cellIndex;

    Processor processor;
    processor.init(48000.0);
    processor.setParameter(kParamLedFeedback, 0.0f);
    processor.setParameter(kParamRunning, 0.0f);
    for (std::uint32_t i = 0; i < 64; ++i)
        processor.setParameter(kParamCellStart + i, 0.0f);

    processor.setParameter(kParamCellStart + static_cast<std::uint32_t>(cellIndex(3, 2)), 1.0f);
    processor.setParameter(kParamCellStart + static_cast<std::uint32_t>(cellIndex(3, 3)), 1.0f);
    processor.setParameter(kParamCellStart + static_cast<std::uint32_t>(cellIndex(3, 4)), 1.0f);

    TransportSnapshot transport {};
    auto result = processor.processBlock(512, transport, nullptr, 0);
    require(!containsNoteOn(result), "lifeform should not advance while stopped");

    processor.setParameter(kParamRunning, 1.0f);
    processor.setParameter(kParamClockMode, 0.0f);
    result = processor.processBlock(512, transport, nullptr, 0);
    require(!containsNoteOn(result), "lifeform auto clock should not free-run while host transport is stopped");

    processor.setParameter(kParamClockMode, 2.0f);
    result = processor.processBlock(512, transport, nullptr, 0);
    require(containsNoteOn(result), "lifeform should emit MIDI for living cells on a generation beat");
    require(processor.getParameter(kParamStatusCellStart + cellIndex(2, 3)) == 1.0f,
            "lifeform blinker should rotate to row 2");
    require(processor.getParameter(kParamStatusCellStart + cellIndex(3, 3)) == 1.0f,
            "lifeform blinker should keep center cell");
    require(processor.getParameter(kParamStatusCellStart + cellIndex(4, 3)) == 1.0f,
            "lifeform blinker should rotate to row 4");
    require(processor.getParameter(kParamStatusCellStart + cellIndex(3, 2)) == 0.0f,
            "lifeform blinker should clear left arm");

    processor.activate();
    processor.setParameter(kParamLedFeedback, 0.0f);
    processor.setParameter(kParamRunning, 0.0f);
    MidiMessage press {};
    press.size = 3;
    press.data[0] = 0x90;
    press.data[1] = gridToNote(5, 6);
    press.data[2] = 127;
    result = processor.processBlock(128, transport, &press, 1);
    require(processor.getParameter(kParamStatusCellStart + cellIndex(5, 6)) == 1.0f,
            "lifeform Launchpad pad press should flip a cell on");
    result = processor.processBlock(128, transport, &press, 1);
    require(processor.getParameter(kParamStatusCellStart + cellIndex(5, 6)) == 0.0f,
            "lifeform Launchpad pad press should flip a cell off");

    processor.activate();
    processor.setParameter(kParamLedFeedback, 0.0f);
    processor.setParameter(kParamRunning, 0.0f);
    for (std::uint32_t i = 0; i < 64; ++i)
        processor.setParameter(kParamCellStart + i, 0.0f);
    processor.setParameter(kParamRunning, 1.0f);
    processor.setParameter(kParamClockMode, 2.0f);
    press.frame = 0;
    press.data[1] = gridToNote(4, 4);
    result = processor.processBlock(128, transport, &press, 1);
    require(processor.getParameter(kParamStatusCellStart + cellIndex(4, 4)) == 1.0f,
            "lifeform same-frame beat and pad press should leave the pressed cell visible");
    require(processor.getParameter(kParamStatusGeneration) == 1.0f,
            "lifeform same-frame beat should still advance exactly one generation");

    processor.setParameter(kParamSeed, 1.0f);
    require(processor.getStatus().activeCells >= 5.0f, "lifeform seed control should load an organism");

    return 0;
}
