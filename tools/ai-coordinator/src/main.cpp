#include "ai_coordinator.hpp"

#include "sidecar_serialization.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace {

void printUsage()
{
    std::cout
        << "usage:\n"
        << "  downspout-ai-coordinator generate state.json --out solo.mid [--phrase phrase.txt]\n";
}

}  // namespace

int main(const int argc, char** argv)
{
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printUsage();
        return argc < 2 ? 1 : 0;
    }

    const std::string command = argv[1];
    if (command != "generate") {
        std::cerr << "unknown command: " << command << '\n';
        printUsage();
        return 1;
    }

    if (argc < 5) {
        printUsage();
        return 1;
    }

    const std::string statePath = argv[2];
    std::string outputPath;
    std::string phrasePath;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--phrase" && i + 1 < argc) {
            phrasePath = argv[++i];
        } else {
            std::cerr << "unknown or incomplete argument: " << arg << '\n';
            return 1;
        }
    }

    if (outputPath.empty()) {
        std::cerr << "missing --out solo.mid\n";
        return 1;
    }

    const auto state = downspout::ai_coordinator::loadTuneStateJson(statePath);
    if (!state.has_value()) {
        std::cerr << "could not read tune state: " << statePath << '\n';
        return 1;
    }

    const downspout::sidecar::Phrase phrase = downspout::ai_coordinator::generateSoloPhrase(*state);
    if (!downspout::ai_coordinator::writeMidiFile(outputPath, phrase, state->tempo, state->channel, state->beatsPerBar)) {
        std::cerr << "could not write MIDI file: " << outputPath << '\n';
        return 1;
    }

    if (!phrasePath.empty()) {
        std::ofstream phraseOutput(phrasePath);
        if (!phraseOutput) {
            std::cerr << "could not write phrase file: " << phrasePath << '\n';
            return 1;
        }
        phraseOutput << downspout::sidecar::serializePhrase(phrase);
    }

    std::cout << "wrote " << outputPath << " with " << phrase.eventCount << " events\n";
    return 0;
}
