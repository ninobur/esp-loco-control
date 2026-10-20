#!/bin/sh
# Compile NAVI_RVM_CONTINUITY_MANUAL for Otto's ESP32 (esp32:esp32:esp32). Compiles only:
# never uploads. Without --credentials-dir the inert credentials.example.h is used.
#   tools/navi_rvm_continuity/build.sh [--credentials-dir DIR] [--build-path DIR]
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SKETCH="$ROOT/firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL"
CRED=""; BUILD="${TMPDIR:-/tmp}/navi_rvm_continuity_build"
while [ $# -gt 0 ]; do case "$1" in
  --credentials-dir) CRED="$2"; shift 2;; --build-path) BUILD="$2"; shift 2;; *) echo "unknown $1"; exit 2;; esac; done
if [ -z "$CRED" ]; then CRED=$(mktemp -d); cp "$SKETCH/credentials.example.h" "$CRED/credentials.h"; fi
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path "$BUILD" \
  --build-property "compiler.cpp.extra_flags=-I$CRED -Werror=format" "$SKETCH"
