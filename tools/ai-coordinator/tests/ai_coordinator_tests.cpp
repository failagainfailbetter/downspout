#include "ai_coordinator.hpp"

#include "sidecar_protocol.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<unsigned char> readBytes(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(input)),
                                      std::istreambuf_iterator<char>());
}

void testParseTuneState()
{
    const std::string json = R"json({
      "key": "D",
      "scale": "dorian",
      "genre": "jazz",
      "tempo": 142,
      "bars": 8,
      "beats_per_bar": 4,
      "channel": 3,
      "register_low": 57,
      "register_high": 88,
      "density": 0.7,
      "risk": 0.45,
      "seed": 1234
    })json";

    const std::optional<downspout::ai_coordinator::TuneState> state =
        downspout::ai_coordinator::parseTuneStateJson(json);
    require(state.has_value(), "state JSON should parse");
    require(state->key == 2, "key name should map to semitone");
    require(state->scale == "dorian", "scale should parse");
    require(state->tempo == 142, "tempo should parse");
    require(state->bars == 8, "bars should parse");
    require(state->channel == 3, "channel should parse");
    require(state->registerLow == 57 && state->registerHigh == 88, "register should parse");
}

void testGeneratePhraseValidates()
{
    downspout::ai_coordinator::TuneState state {};
    state.key = 9;
    state.scale = "minor";
    state.bars = 4;
    state.beatsPerBar = 4;
    state.registerLow = 55;
    state.registerHigh = 84;
    state.density = 0.65f;
    state.risk = 0.35f;
    state.seed = 99;

    const downspout::sidecar::Phrase phrase = downspout::ai_coordinator::generateSoloPhrase(state);
    const downspout::sidecar::Controls controls = downspout::ai_coordinator::controlsFromTuneState(state);
    require(phrase.eventCount > 0, "generated phrase should contain events");
    require(downspout::sidecar::validatePhrase(phrase, controls).valid, "generated phrase should validate");
}

void testWriteMidiFile()
{
    downspout::ai_coordinator::TuneState state {};
    state.seed = 12;
    const downspout::sidecar::Phrase phrase = downspout::ai_coordinator::generateSoloPhrase(state);
    const std::string path = "/tmp/downspout-ai-coordinator-test.mid";
    require(downspout::ai_coordinator::writeMidiFile(path, phrase, 120, 1, 4), "MIDI file should write");

    const std::vector<unsigned char> bytes = readBytes(path);
    require(bytes.size() > 32, "MIDI file should not be empty");
    require(bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'h' && bytes[3] == 'd', "MIDI header should be present");
    require(bytes[14] == 'M' && bytes[15] == 'T' && bytes[16] == 'r' && bytes[17] == 'k', "MIDI track header should be present");
    std::remove(path.c_str());
}

void testAnalyzeMidiFileToTuneState()
{
    downspout::ai_coordinator::TuneState source {};
    source.key = 2;
    source.scale = "dorian";
    source.bars = 2;
    source.registerLow = 48;
    source.registerHigh = 72;
    source.seed = 44;

    const downspout::sidecar::Phrase phrase = downspout::ai_coordinator::generateSoloPhrase(source);
    const std::string path = "/tmp/downspout-ai-coordinator-analyze.mid";
    require(downspout::ai_coordinator::writeMidiFile(path, phrase, 120, 1, 4), "analysis fixture MIDI should write");

    const std::optional<downspout::ai_coordinator::TuneState> analyzed =
        downspout::ai_coordinator::analyzeMidiFileToTuneState(path);
    require(analyzed.has_value(), "MIDI context should analyze");
    require(analyzed->hasMidiContext, "analyzed state should mark MIDI context");
    require(analyzed->guidePitchClassCount > 0, "analyzed state should include guide pitch classes");
    require(analyzed->registerHigh >= analyzed->registerLow, "analyzed register should be ordered");
    require(analyzed->density > 0.0f, "analyzed density should be nonzero");
    std::remove(path.c_str());
}

}  // namespace

int main()
{
    testParseTuneState();
    testGeneratePhraseValidates();
    testWriteMidiFile();
    testAnalyzeMidiFileToTuneState();
    std::cout << "ai coordinator tests passed\n";
    return 0;
}
