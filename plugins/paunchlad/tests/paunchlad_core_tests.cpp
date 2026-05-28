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
            "paunchlad LED feedback should light triggered siren pad");

    float peak = 0.0f;
    for (std::size_t i = 0; i < outLeft.size(); ++i)
        peak = std::max(peak, std::max(std::fabs(outLeft[i]), std::fabs(outRight[i])));
    require(peak > 0.005f, "paunchlad siren should produce audible output");

    MidiMessage echo {};
    echo.size = 3;
    echo.data[0] = 0x90;
    echo.data[1] = gridToNote(0, 7);
    echo.data[2] = 127;
    result = processor.processBlock(inLeft.data(), inRight.data(), outLeft.data(), outRight.data(), 512, &echo, 1);
    require(processor.getStatus().activity > 0.0f, "paunchlad echo throw should update activity");

    return 0;
}
