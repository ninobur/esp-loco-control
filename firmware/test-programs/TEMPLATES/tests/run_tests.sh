#!/bin/bash
# Host regression suite for TEMPLATES 0.3B — see harness_r3.cpp.
# Regenerates the prototype-inserted sketch with arduino-cli (same generator
# the firmware build uses: one code path, no test copy), then compiles it
# against tests/stubs/ and runs the suite.
set -e
cd "$(dirname "$0")"
mkdir -p gen
arduino-cli compile --fqbn esp32:esp32:esp32 --preprocess .. > gen/TEMPLATES_pp.cpp
# strip the Arduino.h include the generator prepends at an awkward spot; the
# harness includes the stub Arduino.h itself before the sketch.
g++ -std=gnu++17 -g -O0 -Wno-format-security \
    -I stubs -I .. \
    harness_r3.cpp -o gen/test_r3 -lm
./gen/test_r3
