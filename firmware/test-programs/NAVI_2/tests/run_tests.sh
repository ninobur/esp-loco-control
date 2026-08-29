#!/bin/bash
# Host regression suite for NAVI 2.0. Regenerates the prototype-inserted
# sketch with arduino-cli (the same generator the firmware build uses: one
# code path, no test copy), compiles it against tests/stubs/, and runs it.
set -e
cd "$(dirname "$0")"
mkdir -p gen
arduino-cli compile --fqbn esp32:esp32:esp32 --preprocess .. > gen/NAVI_pp.cpp
g++ -std=gnu++17 -g -O0 -Wno-format-security \
    -I stubs -I .. \
    harness_navi2.cpp -o gen/test_navi2 -lm
./gen/test_navi2
