#!/bin/sh
# Host checks for NAVI_RVM_SHAPE_MANUAL. No hardware, no network.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd); SK="$ROOT/firmware/test-programs/NAVI_RVM_SHAPE_MANUAL"
OUT=${TMPDIR:-/tmp}/navi_rvm_shape_checks; mkdir -p "$OUT"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_rvm_shape/unit.cpp" -o "$OUT/unit" && "$OUT/unit"
python3 "$ROOT/tests/navi_rvm_shape/authority_check.py" > "$OUT/authority.json" && echo "AUTHORITY CHECK PASSED ($OUT/authority.json)"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_rvm_shape/replay.cpp" -o "$OUT/replay"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_rvm_shape/blocks_replay.cpp" -o "$OUT/blocks_replay"
NATIVE="$HOME/NGR/NGR-Files/hall-ground-20261015/derived/native_15549e9385faa4d5.bin"
if [ -f "$NATIVE" ]; then
  echo "boot-1 replay available: run analysis/navi_rvm_shape_20261018/scripts/s14_firmware_parity.py (and s17_stopgo_replay.py / s18_stopgo_suite.py for STOP/GO) for the full parity check"
else
  echo "boot-1 native archive not present: replay parity skipped (results committed in analysis/navi_rvm_shape_20261018/results/local/parity.json)"
fi
