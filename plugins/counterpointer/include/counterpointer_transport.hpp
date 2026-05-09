#pragma once

#include "counterpointer_core_types.hpp"

namespace downspout::counterpointer {

double counterpointer_cycle_beats_for_controls(const Controls& controls, double beatsPerBar);
double counterpointer_segment_beats_for_controls(const Controls& controls, double beatsPerBar);
int counterpointer_segment_count_for_controls(const Controls& controls, double beatsPerBar);
double counterpointer_wrapped_cycle_position(double absBeats, const Controls& controls, double beatsPerBar);
int counterpointer_segment_index_for_time(const Controls& controls,
                                          double beatsPerBar,
                                          int segmentCount,
                                          double absBeats);
std::uint32_t counterpointer_frame_for_beat(double absBeatsStart,
                                            double absBeatsEnd,
                                            std::uint32_t nframes,
                                            double targetBeat);

}  // namespace downspout::counterpointer
