#!/bin/sh
# run_checks.sh — host verification for the NAVI waveform recognizer.
# Builds and runs the unit tests, then replays the recorded data through
# the recognizer core and scores it. No Arduino toolchain needed.
#   sh run_checks.sh
# The replay fixtures are in fixtures/ (derived from the recorded NSR/CSV
# by make_tick_medians.py; the sources are named in the fixture JSONs).
set -e
SKETCH="$(cd "$(dirname "$0")/../../firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY" && pwd)"
DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="${TMPDIR:-/tmp}/navi_waveform_checks"
mkdir -p "$BUILD"
echo "== unit tests =="
g++ -std=c++17 -O2 -Wall -Wextra -I"$SKETCH" "$DIR/unit_tests.cpp" -o "$BUILD/unit_tests"
"$BUILD/unit_tests"
echo "== replay harness =="
g++ -std=c++17 -O2 -Wall -Wextra -I"$SKETCH" "$DIR/replay_harness.cpp" -o "$BUILD/replay_harness"
echo "-- Level 1 (Otto, 20 reference magnets) --"
"$BUILD/replay_harness" "$DIR/fixtures/level1.ticks.bin" > "$BUILD/level1_events.csv" 2>"$BUILD/level1.log"
cat "$BUILD/level1.log"
python3 "$DIR/score_events.py" "$BUILD/level1_events.csv" "$DIR/fixtures/level1.ticks.json" --ref "$DIR/level1_ref.json"
echo "-- panels (robustness: PWM 0, 21-45, 50, 90; no reference) --"
"$BUILD/replay_harness" "$DIR/fixtures/panels.ticks.bin" > "$BUILD/panels_events.csv" 2>"$BUILD/panels.log"
cat "$BUILD/panels.log"
python3 "$DIR/score_events.py" "$BUILD/panels_events.csv" "$DIR/fixtures/panels.ticks.json" --report
echo "== all checks done =="
