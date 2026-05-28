#include "paunchlad_processor.hpp"

#include <array>
#include <cmath>
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

bool containsMidi(const downspout::paunchlad::ProcessResult& result,
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

} // namespace

int main()
{
    using downspout::paunchlad::MidiMessage;
    using downspout::paunchlad::Processor;
    using downspout::paunchlad::gridToNote;
    using downspout::paunchlad::kLedPurple;
    using downspout::paunchlad::kParamLedFeedback;
    using downspout::paunchlad::kParamSirenLevel;
    using downspout::paunchlad::kParamStatusCellStart;

    Processor processor;
    processor.init(48000.0);

    std::array<float, 512> inLeft {};
    std::array<float, 512> inRight {};
    std::array<float, 512> outLeft {};
    std::array<float, 512> outRight {};
    inLeft[0] = 1.0f;
    inRight[0] = 1.0f;

    MidiMessage siren {};
    siren.size = 3;
    siren.data[0] = 0x90;
    siren.data[1] = gridToNote(2, 3);
    siren.data[2] = 127;

    auto result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &siren, 1);
    require(processor.getParameter(kParamStatusCellStart + downspout::paunchlad::cellIndex(2, 3)) == 1.0f,
            "paunchlad status cell should reflect triggered siren pad");
    require(containsMidi(result, 0x90, gridToNote(2, 3), kLedPurple),
            "paunchlad LED feedback should follow Luma-style programmer mode by default");

    float peak = 0.0f;
    for (std::size_t i = 0; i < outLeft.size(); ++i)
        peak = std::max(peak, std::max(std::fabs(outLeft[i]), std::fabs(outRight[i])));
    require(peak > 0.005f, "paunchlad siren should produce audible output");

    processor.setParameter(kParamLedFeedback, 0.0f);
    result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &siren, 1);
    require(result.eventCount == 0, "paunchlad should stop emitting LED MIDI when LED feedback is disabled");

    processor.activate();
    processor.setParameter(kParamSirenLevel, 0.0f);
    inLeft.fill(0.0f);
    inRight.fill(0.0f);
    result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &siren, 1);
    float mutedPeak = 0.0f;
    for (std::size_t i = 0; i < outLeft.size(); ++i)
        mutedPeak = std::max(mutedPeak, std::max(std::fabs(outLeft[i]), std::fabs(outRight[i])));
    processor.activate();
    processor.setParameter(kParamSirenLevel, 1.0f);
    result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &siren, 1);
    float loudPeak = 0.0f;
    for (std::size_t i = 0; i < outLeft.size(); ++i)
        loudPeak = std::max(loudPeak, std::max(std::fabs(outLeft[i]), std::fabs(outRight[i])));
    require(loudPeak > mutedPeak + 0.005f, "paunchlad siren level parameter should change generated siren output");

    processor.activate();
    processor.setParameter(kParamLedFeedback, 0.0f);
    inLeft[0] = 1.0f;
    inRight[0] = 1.0f;
    MidiMessage custom {};
    custom.size = 3;
    custom.data[0] = 0x90;
    custom.data[1] = 36 + 2 * 8 + 3;
    custom.data[2] = 127;
    result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &custom, 1);
    require(processor.getParameter(kParamStatusCellStart + downspout::paunchlad::cellIndex(2, 3)) == 1.0f,
            "paunchlad should map linear chromatic Launchpad-style notes when LEDs are disabled");

    MidiMessage echo {};
    echo.size = 3;
    echo.data[0] = 0x90;
    echo.data[1] = gridToNote(0, 7);
    echo.data[2] = 127;
    result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &echo, 1);
    require(processor.getStatus().activity > 0.0f, "paunchlad echo throw should update activity");

    return 0;
}
