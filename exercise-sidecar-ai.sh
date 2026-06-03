#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${DOWNSPOUT_BUILD_DIR:-$repo_root/build}"
state_file="${DOWNSPOUT_AI_STATE:-$repo_root/tools/ai-coordinator/examples/state.json}"
out_dir="${DOWNSPOUT_AI_OUT_DIR:-/tmp/downspout-sidecar-exercise}"
build_type="${CMAKE_BUILD_TYPE:-Release}"

coordinator="$build_dir/tools/ai-coordinator/downspout-ai-coordinator"
solo_midi="$out_dir/sidecar-solo.mid"
phrase_txt="$out_dir/sidecar-phrase.txt"

if [[ ! -f "$state_file" ]]; then
  echo "Missing state file: $state_file" >&2
  exit 1
fi

mkdir -p "$out_dir"

echo "Configuring downspout"
cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type"

echo "Building Sidecar AI coordinator"
cmake --build "$build_dir" --target downspout-ai-coordinator downspout_ai_coordinator_tests

if cmake --build "$build_dir" --target help | grep -q "sidecar-vst3"; then
  echo "Building Sidecar VST3"
  cmake --build "$build_dir" --target sidecar-vst3
fi

echo "Running Sidecar/coordinator tests"
ctest --test-dir "$build_dir" -R "downspout_(ai_coordinator|sidecar)_.*tests" --output-on-failure

echo "Generating sample solo MIDI"
"$coordinator" generate "$state_file" --out "$solo_midi" --phrase "$phrase_txt"

if [[ ! -s "$solo_midi" ]]; then
  echo "Expected non-empty MIDI output: $solo_midi" >&2
  exit 1
fi

if [[ ! -s "$phrase_txt" ]]; then
  echo "Expected non-empty phrase output: $phrase_txt" >&2
  exit 1
fi

echo
echo "Generated:"
echo "  MIDI:   $solo_midi"
echo "  Phrase: $phrase_txt"

echo
echo "Phrase preview:"
sed -n '1,12p' "$phrase_txt"

echo
echo "Import the MIDI file into the DAW to audition the generated Sidecar phrase."
