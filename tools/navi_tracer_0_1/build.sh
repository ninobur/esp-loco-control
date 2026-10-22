#!/bin/sh
# Compile NAVI_TRACER_0_1 for Otto's ESP32 (esp32:esp32:esp32). Compiles only:
# never uploads. Without --credentials-dir the inert credentials.example.h is used.
#   tools/navi_tracer_0_1/build.sh [--credentials-dir DIR] [--build-path DIR]
#
# NOT YET RUN SUCCESSFULLY END TO END: the environment this sketch was
# implemented in could not install arduino-cli (egress policy denies
# downloads.arduino.cc). This script mirrors tools/navi_rvm_shape/build.sh's
# proven invocation exactly; run it on a machine with arduino-cli and the
# esp32 board core installed before flashing. See
# docs/NAVI_TRACER_0_1_FIELD_TEST_AND_ROLLBACK.md.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SKETCH="$ROOT/firmware/test-programs/NAVI_TRACER_0_1"
CRED=""; BUILD="${TMPDIR:-/tmp}/navi_tracer_0_1_build"
while [ $# -gt 0 ]; do case "$1" in
  --credentials-dir) CRED="$2"; shift 2;; --build-path) BUILD="$2"; shift 2;; *) echo "unknown $1"; exit 2;; esac; done
if [ -z "$CRED" ]; then CRED=$(mktemp -d); cp "$SKETCH/credentials.example.h" "$CRED/credentials.h"; fi
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path "$BUILD" \
  --build-property "compiler.cpp.extra_flags=-I$CRED -Werror=format" "$SKETCH"
