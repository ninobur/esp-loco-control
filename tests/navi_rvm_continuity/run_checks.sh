#!/bin/sh
# Host checks for NAVI_RVM_CONTINUITY_MANUAL. No hardware, no network.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd); SK="$ROOT/firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL"
OUT=${TMPDIR:-/tmp}/navi_rvm_continuity_checks; mkdir -p "$OUT"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_rvm_continuity/unit.cpp" -o "$OUT/unit" && "$OUT/unit"
python3 "$ROOT/tests/navi_rvm_continuity/authority_check.py" > "$OUT/authority.json" && echo "AUTHORITY CHECK PASSED ($OUT/authority.json)"
RVM_SKETCH=NAVI_RVM_CONTINUITY_MANUAL_TOBY python3 "$ROOT/tests/navi_rvm_continuity/authority_check.py" > "$OUT/authority_toby.json" && echo "AUTHORITY CHECK PASSED, TOBY ($OUT/authority_toby.json)"
for f in "$SK"/*.h; do b=$(basename "$f"); case "$b" in LocoConfig.h|LL_LocoConfig_*) ;; *) cmp -s "$f" "$SK"_TOBY/"$b" || { echo "TOBY HEADER DIFFERS: $b"; exit 1; } ;; esac; done && echo "TOBY HEADERS IDENTICAL TO OTTO'S (except LocoConfig)"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$SK" "$ROOT/tests/navi_rvm_continuity/replay.cpp" -o "$OUT/replay" && echo "REPLAY BUILT ($OUT/replay)"
echo "Recording parity and replays: analysis/navi_rvm_shape_20261018/unified/u10_firmware_parity.py (needs the NSR-derived native files)"
