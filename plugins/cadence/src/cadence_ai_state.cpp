#include "cadence_ai_state.hpp"

#include <algorithm>
#include <sstream>

namespace downspout::cadence {
namespace {

void appendQuoted(std::ostringstream& out, const char* value)
{
    out << '"';
    while (*value != '\0') {
        if (*value == '"' || *value == '\\')
            out << '\\';
        out << *value++;
    }
    out << '"';
}

}  // namespace

const char* scaleName(const int scale) noexcept
{
    switch (scale) {
    case SCALE_CHROMATIC: return "chromatic";
    case SCALE_MAJOR: return "major";
    case SCALE_NAT_MINOR: return "natural_minor";
    case SCALE_HARM_MINOR: return "harmonic_minor";
    case SCALE_PENT_MAJOR: return "pentatonic_major";
    case SCALE_PENT_MINOR: return "pentatonic_minor";
    case SCALE_BLUES: return "blues";
    case SCALE_DORIAN: return "dorian";
    case SCALE_MIXOLYDIAN: return "mixolydian";
    case SCALE_PHRYGIAN: return "phrygian";
    case SCALE_LOCRIAN: return "locrian";
    case SCALE_PHRYGIAN_DOMINANT: return "phrygian_dominant";
    case SCALE_LYDIAN: return "lydian";
    case SCALE_MELODIC_MINOR: return "melodic_minor";
    case SCALE_WHOLE_TONE: return "whole_tone";
    case SCALE_ALTERED: return "altered";
    case SCALE_HALF_WHOLE_DIMINISHED: return "half_whole_diminished";
    case SCALE_WHOLE_HALF_DIMINISHED: return "whole_half_diminished";
    case SCALE_BEBOP_DOMINANT: return "bebop_dominant";
    case SCALE_BEBOP_MAJOR: return "bebop_major";
    case SCALE_BEBOP_MINOR: return "bebop_minor";
    default: return "natural_minor";
    }
}

const char* chordQualityName(const int quality) noexcept
{
    switch (quality) {
    case QUALITY_POWER: return "power";
    case QUALITY_MAJOR: return "major";
    case QUALITY_MINOR: return "minor";
    case QUALITY_SUS2: return "sus2";
    case QUALITY_SUS4: return "sus4";
    case QUALITY_DIM: return "diminished";
    case QUALITY_DOM7: return "dominant_7";
    case QUALITY_MAJ7: return "major_7";
    case QUALITY_MIN7: return "minor_7";
    case QUALITY_DOM9: return "dominant_9";
    case QUALITY_MAJ9: return "major_9";
    case QUALITY_MIN9: return "minor_9";
    case QUALITY_DOM13: return "dominant_13";
    case QUALITY_MAJ13: return "major_13";
    case QUALITY_MIN11: return "minor_11";
    default: return "major";
    }
}

const char* chordSizeName(const int chordSize) noexcept
{
    switch (chordSize) {
    case CHORD_SIZE_TRIADS: return "triads";
    case CHORD_SIZE_SEVENTHS: return "sevenths";
    case CHORD_SIZE_EXTENDED: return "extended";
    default: return "triads";
    }
}

const char* registerName(const int reg) noexcept
{
    switch (reg) {
    case REGISTER_LOW: return "low";
    case REGISTER_MID: return "mid";
    case REGISTER_HIGH: return "high";
    default: return "mid";
    }
}

std::string summarizeCadenceAiState(const Controls& rawControls,
                                    const ProgressionState& rawProgression)
{
    const Controls controls = clampControls(rawControls);
    ProgressionState progression = rawProgression;
    progression.segmentCount = clampi(progression.segmentCount, 0, kMaxSegments);

    std::ostringstream out;
    out << "{";
    out << "\"plugin\":\"cadence\",";
    out << "\"key\":" << controls.key << ',';
    out << "\"scale\":";
    appendQuoted(out, scaleName(controls.scale));
    out << ',';
    out << "\"cycle_bars\":" << controls.cycle_bars << ',';
    out << "\"granularity\":" << controls.granularity << ',';
    out << "\"complexity\":" << controls.complexity << ',';
    out << "\"movement\":" << controls.movement << ',';
    out << "\"color\":" << controls.color << ',';
    out << "\"chord_size\":";
    appendQuoted(out, chordSizeName(controls.chord_size));
    out << ',';
    out << "\"register\":";
    appendQuoted(out, registerName(controls.reg));
    out << ',';
    out << "\"spread\":" << controls.spread << ',';
    out << "\"arpeggio\":" << controls.arpeggio << ',';
    out << "\"progression_ready\":" << (progression.ready ? "true" : "false") << ',';
    out << "\"segment_count\":" << progression.segmentCount << ',';
    out << "\"chords\":[";

    int written = 0;
    for (int i = 0; i < progression.segmentCount; ++i) {
        const ChordSlot& slot = progression.slots[static_cast<std::size_t>(i)];
        if (!slot.valid)
            continue;
        if (written > 0)
            out << ',';
        out << "{\"segment\":" << i
            << ",\"root_pc\":" << static_cast<int>(slot.root_pc)
            << ",\"quality\":";
        appendQuoted(out, chordQualityName(slot.quality));
        out << ",\"velocity\":" << static_cast<int>(slot.velocity)
            << ",\"notes\":[";
        const int noteCount = std::min<int>(slot.note_count, kMaxChordNotes);
        for (int n = 0; n < noteCount; ++n) {
            if (n > 0)
                out << ',';
            out << static_cast<int>(slot.notes[static_cast<std::size_t>(n)]);
        }
        out << "]}";
        ++written;
    }

    out << "]}";
    return out.str();
}

}  // namespace downspout::cadence
