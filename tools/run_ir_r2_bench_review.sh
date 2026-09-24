#!/usr/bin/env bash
# Reproduces docs/IR_TX_R2_BENCH_FAILURE_REVIEW_20260924.md.
#
# Builds every IR detector host suite twice, against:
#   r2 - firmware/common/IrMovementDetector.h as committed (reviewed R2, 3887f2a)
#   r3 - tools/ir_r3_proposal/IrMovementDetector.h (REVIEW PROPOSAL, not firmware)
# Each variant is compiled from a staged copy of firmware/ in a temporary
# directory, so the NAVI headers' relative includes pick up that variant's
# detector. Nothing under firmware/ in the repository is modified.
#
# Usage: tools/run_ir_r2_bench_review.sh   (from the repository root)
# Exit status is non-zero if any gate below fails. Expected-failure baselines
# are run in their documented "expect" modes and say so.
set -u
cd "$(dirname "$0")/.."
REPO=$(pwd)
WORK=$(mktemp -d "${TMPDIR:-/tmp}/ir_r2_review.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
CXX=${CXX:-c++}
FLAGS="-std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
NAVI=firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH
status=0
run() {  # label, then command
  local label=$1; shift
  if "$@" >"$WORK/out.txt" 2>&1; then echo "ok    $label"; grep -E '^(INFO|PASS|FAIL) |^count |^R3 |^transmitted |^agreement ' "$WORK/out.txt" | sed 's/^/      /'; else echo "FAIL  $label"; sed 's/^/      /' "$WORK/out.txt" | tail -25; status=1; fi
}

python3 tools/ir_r2_capture_extract.py field-records/logs/20260923_ir_r2_bench_radio.log.gz \
  "$WORK/bench.fix" e9f8b7c4 8dc31ef4e9f8b7c4 || exit 2
python3 tools/ir_r2_capture_extract.py field-records/logs/20260923_ir_r2_three_stops_radio.log.gz \
  "$WORK/three.fix" e9f8b7c4 8dc31ef4e9f8b7c4 || exit 2
git show '1de06ae^:firmware/common/IrMovementDetector.h' >"$WORK/baseline_1_5.h" || exit 2

for v in r2 r3; do
  T=$WORK/$v; mkdir -p "$T/tools"; cp -R firmware "$T/"
  # Tests are staged too: some include NAVI headers by paths relative to tools/.
  cp tools/*.cpp tools/*.h "$T/tools/"
  [ "$v" = r3 ] && cp tools/ir_r3_proposal/IrMovementDetector.h "$T/firmware/common/IrMovementDetector.h"
  INC="-I$T/firmware/common -I$T/$NAVI -I$T/tools"
  echo "== $v"
  for t in test_ir_movement test_ir_phase_retention test_ir_movement_contract test_ir_movement_wire \
           test_ir_stationary test_ir_stationary_pipeline test_ir_stationary_revision test_ir_stationary_adversarial; do
    run "$v $t" sh -c "$CXX $FLAGS $INC $T/tools/$t.cpp -o $T/$t && $T/$t"
  done
  for t in test_ir_health_monitor test_ir_speed test_coherence test_proximal_recovery; do
    run "$v NAVI 0.6 $t" sh -c "cd $T/$NAVI && $CXX $FLAGS -I. tests/$t.cpp -o $T/$t && $T/$t"
  done
  run "$v fade while turning" sh -c "$CXX $FLAGS $INC $T/tools/test_ir_fade_turning.cpp -o $T/fade && $T/fade"
  run "$v default mode vs 1.5 (1de06ae^)" sh -c "$CXX $FLAGS $INC -DIR_BASELINE_HEADER='\"$WORK/baseline_1_5.h\"' $T/tools/test_ir_default_equivalence.cpp -o $T/eq && $T/eq"
  if [ "$v" = r2 ]; then
    run "r2 stop decay (Codex 78c84d8), expected R2 failure mode" sh -c "$CXX $FLAGS $INC $T/tools/test_ir_stop_decay.cpp -o $T/sd && $T/sd --expect-r2-failure"
    run "r2 captured-waveform replay (actual R2 detector)" sh -c "$CXX -std=c++17 -O2 $INC $T/tools/test_ir_r2_bench_replay.cpp -o $T/rp && $T/rp $WORK/bench.fix $WORK/three.fix"
  else
    run "r3 default mode vs R2 (bit-identical)" sh -c "$CXX $FLAGS $INC -DIR_BASELINE_HEADER='\"$REPO/firmware/common/IrMovementDetector.h\"' $T/tools/test_ir_default_equivalence.cpp -o $T/eq2 && $T/eq2"
    run "r3 stop decay (Codex 78c84d8), normal mode" sh -c "$CXX $FLAGS $INC $T/tools/test_ir_stop_decay.cpp -o $T/sd && $T/sd"
    run "r3 proposal cases" sh -c "$CXX $FLAGS $INC $T/tools/test_ir_r3_proposal.cpp -o $T/pc && $T/pc"
    run "r3 captured-waveform replay" sh -c "$CXX -std=c++17 -O2 $INC $T/tools/test_ir_r3_bench_replay.cpp -o $T/r3rp && $T/r3rp $WORK/bench.fix $WORK/three.fix"
  fi
done
exit $status
