#!/bin/sh
# Legacy host checks for NAVI_POWER_OF_NORMAL_MANUAL_ONLY. No hardware, no network.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd); SK="$ROOT/NAVI_WAVEFORM_0_1"
OUT=${TMPDIR:-/tmp}/navi_power_of_normal_checks; mkdir -p "$OUT"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_power_of_normal/unit.cpp" -o "$OUT/unit" && "$OUT/unit"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_power_of_normal/replay.cpp" -o "$OUT/replay"
NATIVE="$HOME/NGR/NGR-Files/hall-ground-20261015/derived/native_15549e9385faa4d5.bin"
DECL="$HOME/NGR/NGR-Files/navi-rvm-replay-20261016/derived/declarations.csv"
if [ -f "$NATIVE" ] && [ -f "$DECL" ]; then
  "$OUT/replay" "$NATIVE" "$DECL" 15549e9385faa4d5 "$OUT"
  echo "boot-1 replay written to $OUT (encounters.csv, nav.csv) -- compare onset/confirmed counts against"
  echo "analysis/navi_rvm_shape_20261018/results/local/nav_summary.json (old recognizer, same boot) by hand;"
  echo "this harness does not yet score against the annotation CSV automatically."
else
  echo "boot-1 native archive or declarations.csv not present: replay skipped"
fi
echo "Full-sketch arduino-cli compile (esp32:esp32:esp32) succeeds as of 2026-10-24; run separately:"
echo "  arduino-cli compile --fqbn esp32:esp32:esp32 \"$SK\""
