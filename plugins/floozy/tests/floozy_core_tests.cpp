#include "floozy_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using downspout::floozy::FloozyEngine;
using downspout::floozy::MidiMessage;
using downspout::floozy::ParamId;
using downspout::floozy::kParameterSpecs;

void require(const bool condition, const char* const message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

float renderPeak(FloozyEngine& engine, const int frames)
{
    float peak = 0.0f;
    for (int i = 0; i < frames; ++i)
    {
        const auto frame = engine.processStereo();
        require(std::isfinite(frame.left) && std::isfinite(frame.right), "floozy rendered non-finite audio");
        peak = std::max(peak, std::max(std::fabs(frame.left), std::fabs(frame.right)));
    }
    return peak;
}

float renderEnergy(FloozyEngine& engine, const int frames)
{
    float energy = 0.0f;
    for (int i = 0; i < frames; ++i)
    {
        const auto frame = engine.processStereo();
        require(std::isfinite(frame.left) && std::isfinite(frame.right), "floozy rendered non-finite audio");
        energy += std::fabs(frame.left) + std::fabs(frame.right);
    }
    return energy;
}

void defaultsAndClamping()
{
    FloozyEngine engine {48000.0f};
    require(std::fabs(engine.getParameter(ParamId::masterGain) -
                      kParameterSpecs[static_cast<std::size_t>(ParamId::masterGain)].defaultValue) < 1.0e-6f,
            "floozy master gain default mismatch");

    engine.setParameter(ParamId::sourceAlgorithm, 2.7f);
    require(std::fabs(engine.getParameter(ParamId::sourceAlgorithm) - 3.0f) < 1.0e-6f,
            "floozy integer parameter should round");

    engine.setParameter(ParamId::sourceAlgorithm, 99.0f);
    require(std::fabs(engine.getParameter(ParamId::sourceAlgorithm) - 6.0f) < 1.0e-6f,
            "floozy integer parameter should clamp high");

    engine.setParameter(ParamId::sourceLevel, -1.0f);
    require(std::fabs(engine.getParameter(ParamId::sourceLevel)) < 1.0e-6f,
            "floozy unit parameter should clamp low");
}

void allAlgorithmsRender()
{
    for (int algorithm = 0; algorithm <= 6; ++algorithm)
    {
        FloozyEngine engine {48000.0f};
        engine.setParameter(ParamId::sourceAlgorithm, static_cast<float>(algorithm));
        engine.setParameter(ParamId::sourceLevel, 0.85f);
        engine.setParameter(ParamId::masterGain, 0.9f);
        engine.noteOn(48 + algorithm, 116);
        require(renderPeak(engine, 4096) > 0.0005f, "floozy algorithm did not render audible output");
    }
}

void velocityAffectsOutput()
{
    FloozyEngine soft {48000.0f};
    soft.setParameter(ParamId::sourceLevel, 0.9f);
    soft.noteOn(57, 28);
    const float softEnergy = renderEnergy(soft, 2048);

    FloozyEngine loud {48000.0f};
    loud.setParameter(ParamId::sourceLevel, 0.9f);
    loud.noteOn(57, 122);
    const float loudEnergy = renderEnergy(loud, 2048);

    require(loudEnergy > softEnergy * 1.8f, "floozy velocity should scale output");
}

void allInterfacesRender()
{
    for (int interfaceType = 0; interfaceType <= 11; ++interfaceType)
    {
        FloozyEngine engine {48000.0f};
        engine.setParameter(ParamId::interfaceType, static_cast<float>(interfaceType));
        engine.setParameter(ParamId::interfaceIntensity, 0.72f);
        engine.setParameter(ParamId::sourceLevel, 0.70f);
        engine.setParameter(ParamId::noiseLevel, 0.18f);
        engine.setParameter(ParamId::masterGain, 0.85f);
        engine.noteOn(55 + (interfaceType % 12), 112);
        require(renderPeak(engine, 8192) > 0.0002f, "floozy interface did not render audible output");
    }
}

void midiVoiceLifecycle()
{
    FloozyEngine engine {48000.0f};
    const std::uint8_t noteOn[] = {0x90, 60, 100};
    const std::uint8_t noteOff[] = {0x80, 60, 0};
    const std::uint8_t panic[] = {0xb0, 123, 0};

    engine.handleMidi(noteOn, 3);
    require(engine.activeVoiceCount() == 1, "floozy note on should activate one voice");
    engine.handleMidi(noteOff, 3);
    require(engine.activeVoiceCount() == 1, "floozy note off should release rather than hard-stop");
    engine.handleMidi(panic, 3);
    require(engine.activeVoiceCount() == 0, "floozy all-notes-off should clear voices");
}

void noteOffEventuallyStopsAllInterfaces()
{
    for (int interfaceType = 0; interfaceType <= 11; ++interfaceType)
    {
        FloozyEngine engine {48000.0f};
        engine.setParameter(ParamId::interfaceType, static_cast<float>(interfaceType));
        engine.setParameter(ParamId::interfaceIntensity, 0.80f);
        engine.setParameter(ParamId::delay1Feedback, 0.70f);
        engine.setParameter(ParamId::delay2Feedback, 0.45f);
        engine.setParameter(ParamId::filterFeedback, 0.15f);
        engine.noteOn(60, 112);
        require(renderEnergy(engine, 4096) > 0.0001f, "floozy should render before note-off");
        engine.noteOff(60);

        for (int i = 0; i < 144000 && engine.activeVoiceCount() > 0; ++i)
            (void)engine.processStereo();

        require(engine.activeVoiceCount() == 0, "floozy note-off should let every interface stop");
    }
}

void polyphonyIsCapped()
{
    FloozyEngine engine {48000.0f};
    for (int note = 48; note < 57; ++note)
        engine.noteOn(note, 100);
    require(engine.activeVoiceCount() == FloozyEngine::kMaxVoices, "floozy polyphony cap mismatch");
}

void processBlockSchedulesMidi()
{
    FloozyEngine engine {48000.0f};
    std::array<float, 8192> left {};
    std::array<float, 8192> right {};
    std::array<MidiMessage, 2> midi {};
    midi[0].frame = 32;
    midi[0].size = 3;
    midi[0].data = {0x90, 60, 110, 0};
    midi[1].frame = 7000;
    midi[1].size = 3;
    midi[1].data = {0x80, 60, 0, 0};

    engine.processBlock(left.data(), right.data(), static_cast<std::uint32_t>(left.size()), midi.data(), 2);
    float preEnergy = 0.0f;
    float postEnergy = 0.0f;
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        require(std::isfinite(left[i]) && std::isfinite(right[i]), "floozy processBlock emitted non-finite audio");
        if (i < 32)
            preEnergy += std::fabs(left[i]) + std::fabs(right[i]);
        else
            postEnergy += std::fabs(left[i]) + std::fabs(right[i]);
    }
    require(preEnergy < 0.0001f, "floozy processBlock should respect note-on frame offset");
    require(postEnergy > 0.0001f, "floozy processBlock should render after scheduled note-on");
}

void extremeParametersStayFinite()
{
    FloozyEngine engine {96000.0f};
    for (std::uint32_t i = 0; i < downspout::floozy::kParameterCount; ++i)
        engine.setParameter(i, kParameterSpecs[i].maximum);
    engine.setParameter(ParamId::masterGain, 1.0f);
    engine.noteOn(36, 127);
    require(renderPeak(engine, 8192) <= 1.0f, "floozy output should remain bounded");
}

} // namespace

int main()
{
    defaultsAndClamping();
    allAlgorithmsRender();
    velocityAffectsOutput();
    allInterfacesRender();
    midiVoiceLifecycle();
    noteOffEventuallyStopsAllInterfaces();
    polyphonyIsCapped();
    processBlockSchedulesMidi();
    extremeParametersStayFinite();

    std::cout << "floozy core tests passed\n";
    return 0;
}
