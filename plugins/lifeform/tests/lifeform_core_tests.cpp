#include "lifeform_processor.hpp"

#include <array>
#include <cstdlib>
#include <initializer_list>
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

bool containsMidi(const downspout::lifeform::ProcessResult& result,
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

bool containsSysexPrefix(const downspout::lifeform::ProcessResult& result, const std::initializer_list<std::uint8_t> prefix)
{
    for (std::uint32_t i = 0; i < result.eventCount; ++i)
    {
        if (result.events[i].size < prefix.size())
            continue;

        std::size_t offset = 0;
        bool matches = true;
        for (const std::uint8_t byte : prefix)
        {
            if (result.events[i].data[offset++] != byte)
            {
                matches = false;
                break;
            }
        }
        if (matches)
            return true;
    }
    return false;
}

bool containsLargeClearSysex(const downspout::lifeform::ProcessResult& result)
{
    for (std::uint32_t i = 0; i < result.eventCount; ++i)
    {
        if (result.events[i].size == 251 &&
            result.events[i].data[0] == 0xf0u &&
            result.events[i].data[6] == 0x03u &&
            result.events[i].data[250] == 0xf7u)
        {
            return true;
        }
    }
    return false;
}

bool containsLedChannelMusicNote(const downspout::lifeform::ProcessResult& result)
{
    for (std::uint32_t i = 0; i < result.eventCount; ++i)
    {
        if (result.events[i].size == 3 &&
            result.events[i].data[0] >= 0x90u &&
            result.events[i].data[0] <= 0x92u &&
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
    using downspout::lifeform::kParamPanic;
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
    require(!containsLedChannelMusicNote(result),
            "lifeform default musical MIDI should avoid Launchpad LED channels 1-3");
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
    press.data[2] = 0;
    result = processor.processBlock(128, transport, &press, 1);
    require(processor.getParameter(kParamStatusCellStart + cellIndex(5, 6)) == 1.0f,
            "lifeform Launchpad pad release should not flip or echo as LED-off");
    require(!containsMidi(result, 0x90, gridToNote(5, 6), 0),
            "lifeform Launchpad pad release should not be echoed to the hardware LED stream");
    press.data[2] = 127;
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

    processor.setParameter(kParamPanic, 1.0f);
    result = processor.processBlock(128, transport, nullptr, 0);
    require(processor.getParameter(kParamStatusCellStart + cellIndex(4, 4)) == 0.0f,
            "lifeform panic should clear cells");
    require(processor.getParameter(kParamRunning) == 0.0f,
            "lifeform panic should stop the generator");
    require(containsSysexPrefix(result, {0xf0u, 0x00u, 0x20u, 0x29u, 0x02u, 0x0du, 0x0eu, 0x01u}),
            "lifeform panic should send programmer-mode SysEx");
    require(containsLargeClearSysex(result),
            "lifeform panic should send a full Launchpad clear SysEx");
    require(containsMidi(result, 0x90, gridToNote(0, 0), 0),
            "lifeform panic should also send 3-byte LED-off fallback for grid pads");
    require(containsMidi(result, 0xb0, 99, 0),
            "lifeform panic should also send 3-byte LED-off fallback for the logo");

    processor.setParameter(kParamSeed, 1.0f);
    require(processor.getStatus().activeCells >= 5.0f, "lifeform seed control should load an organism");

    return 0;
}
