#include "gremlin_processor.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool nearlyEqual(const float a, const float b, const float epsilon = 1.0e-5f)
{
    return std::fabs(a - b) <= epsilon;
}

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool containsMidi(const downspout::gremlin::MidiMessage* events,
                  const std::uint32_t count,
                  const std::uint8_t status,
                  const std::uint8_t data1,
                  const std::uint8_t data2)
{
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (events[i].size == 3 &&
            events[i].data[0] == status &&
            events[i].data[1] == data1 &&
            events[i].data[2] == data2)
        {
            return true;
        }
    }
    return false;
}

}  // namespace

int main()
{
    using downspout::gremlin::ActionId;
    using downspout::gremlin::LiveParamId;
    using downspout::gremlin::MidiMessage;
    using downspout::gremlin::Processor;
    using downspout::gremlin::SceneId;

    Processor processor;
    processor.init(48000.0);

    require(nearlyEqual(processor.getLiveParameter(LiveParamId::mode), 0.0f), "gremlin default mode mismatch");

    processor.loadScene(SceneId::rust);
    require(nearlyEqual(processor.getLiveParameter(LiveParamId::mode), 2.0f), "gremlin rust scene mode mismatch");
    require(processor.getStatus().currentScene == static_cast<std::uint32_t>(SceneId::rust), "gremlin rust scene status mismatch");

    processor.triggerAction(ActionId::panic);
    require(!processor.getMomentary(downspout::gremlin::MomentaryId::freeze), "gremlin panic should clear momentaries");

    MidiMessage cc {};
    cc.size = 3;
    cc.data[0] = 0xB0;
    cc.data[1] = downspout::gremlin::kMacroFaderCCs[0];
    cc.data[2] = 127;
    float left[16] {};
    float right[16] {};
    processor.processBlock(left, right, 16, &cc, 1);
    require(nearlyEqual(processor.getMacro(downspout::gremlin::MacroId::source), 1.0f), "gremlin macro CC mapping mismatch");

    std::array<MidiMessage, Processor::kMaxOutputMidiEvents> ledEvents {};
    std::uint32_t ledEventCount = 0;
    processor.processBlock(left, right, 16, nullptr, 0, ledEvents.data(), &ledEventCount, ledEvents.size());
    require(containsMidi(ledEvents.data(), ledEventCount, 0x90, downspout::gremlin::kRecArmNotes[2], 127),
            "gremlin LED feedback should light current mode");

    MidiMessage muteDown {};
    muteDown.size = 3;
    muteDown.data[0] = 0x90;
    muteDown.data[1] = downspout::gremlin::kMuteNotes[0];
    muteDown.data[2] = 127;
    ledEventCount = 0;
    processor.processBlock(left, right, 16, &muteDown, 1, ledEvents.data(), &ledEventCount, ledEvents.size());
    require(containsMidi(ledEvents.data(), ledEventCount, 0x90, downspout::gremlin::kMuteNotes[0], 127),
            "gremlin LED feedback should light held momentary");

    processor.setLiveParameter(LiveParamId::mode, 4.0f);
    ledEventCount = 0;
    processor.processBlock(left, right, 16, nullptr, 0, ledEvents.data(), &ledEventCount, ledEvents.size());
    require(containsMidi(ledEvents.data(), ledEventCount, 0x90, downspout::gremlin::kBankLeftNote, 127),
            "gremlin LED feedback should indicate extended mode on bank LED");

    MidiMessage noteOn {};
    noteOn.size = 3;
    noteOn.data[0] = 0x90;
    noteOn.data[1] = 60;
    noteOn.data[2] = 100;

    float synthLeft[512] {};
    float synthRight[512] {};
    processor.processBlock(synthLeft, synthRight, 512, &noteOn, 1);

    float peak = 0.0f;
    for (int i = 0; i < 512; ++i)
        peak = std::max(peak, std::max(std::fabs(synthLeft[i]), std::fabs(synthRight[i])));

    require(peak > 0.01f, "gremlin should emit audio after note-on");

    for (std::size_t mode = 0; mode < downspout::gremlin::kModeCount; ++mode)
    {
        Processor modeProcessor;
        modeProcessor.init(48000.0);
        modeProcessor.setLiveParameter(LiveParamId::mode, static_cast<float>(mode));
        modeProcessor.processBlock(synthLeft, synthRight, 512, &noteOn, 1);

        float modePeak = 0.0f;
        for (int i = 0; i < 512; ++i)
            modePeak = std::max(modePeak, std::max(std::fabs(synthLeft[i]), std::fabs(synthRight[i])));

        require(modePeak > 0.005f, "gremlin mode should emit audio after note-on");
    }

    float monoOnly[128] {};
    processor.processBlock(monoOnly, nullptr, 128, nullptr, 0);
    float monoPeak = 0.0f;
    for (float sample : monoOnly)
        monoPeak = std::max(monoPeak, std::fabs(sample));

    require(monoPeak > 0.001f, "gremlin mono-output render should still emit audio");

    return 0;
}
