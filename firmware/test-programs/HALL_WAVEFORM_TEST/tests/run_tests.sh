#!/usr/bin/env bash
# HALL_WAVEFORM_TEST host tests — INVESTIGATORY / UNAPPROVED.
# Runs the capture-engine tests (g++), the direction-gate tests (g++), and
# the decoder tests (python3).
set -e
cd "$(dirname "$0")"

echo "== capture engine (g++) =="
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
    -o /tmp/hwt_capture_tests test_hall_capture.cpp
/tmp/hwt_capture_tests

echo
echo "== direction gate (g++) =="
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
    -o /tmp/hwt_direction_gate_tests test_direction_gate.cpp
/tmp/hwt_direction_gate_tests

echo
echo "== decoder / receiver (python3) =="
python3 test_decoder.py

echo
echo "== excursion analysis (python3) =="
python3 test_excursions.py

echo
echo "== source audit: no control authority in the recorder =="
python3 test_no_control_authority.py
