#!/bin/sh
# NAVI_TRACER_0_1 host-side verification: synthetic unit tests, then the
# Python/C++ event-stream equivalence suite across all fixtures and both
# duration configurations. No hardware, no network, no real recorded data
# (see docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md for that limitation).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SK="$ROOT/firmware/test-programs/NAVI_TRACER_0_1"
TS="$ROOT/tests/navi_tracer_0_1"
OUT="${TMPDIR:-/tmp}/navi_tracer_0_1_checks"
mkdir -p "$OUT" "$TS/out"

echo "== unit tests (ported 1:1 from test_target_acquisition.py) =="
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$SK" "$TS/unit.cpp" -o "$OUT/unit"
"$OUT/unit"

echo
echo "== fixtures =="
python3 "$TS/generate_fixtures.py"

echo
echo "== equivalence runner build =="
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$SK" "$TS/equivalence_runner.cpp" -o "$OUT/equivalence_runner"

echo
echo "== equivalence: python reference vs. C++ port, per fixture, per duration config =="
overall=0
run_one() {
  name="$1"; mm="$2"; dur="$3"
  pyout="$TS/out/${name}_d${dur}_py.csv"
  cppout="$TS/out/${name}_d${dur}_cpp.csv"
  python3 "$TS/run_python_reference.py" "$TS/fixtures/${name}.csv" "$mm" 1 "$dur" "$pyout" >/dev/null
  "$OUT/equivalence_runner" "$TS/fixtures/${name}.csv" "$mm" 1 "$dur" "$cppout" >/dev/null
  result=$(python3 "$TS/compare_events.py" "$pyout" "$cppout")
  echo "$name duration=$dur -> $result"
  case "$result" in PASS*) ;; *) overall=1 ;; esac
}
for dur in 0.02 0.11; do
  run_one basic_correct_sequence 45 "$dur"
  run_one wrong_polarity 45 "$dur"
  run_one short_noise_excursion 45 "$dur"
  run_one extended_no_timeout 45 "$dur"
  run_one stop_restart_mid_candidate 45 "$dur"
  run_one 284s_anomaly_synthetic 45 "$dur"
  run_one multi_magnet_sequence_synthetic 99 "$dur"
done
if [ "$overall" -ne 0 ]; then
  echo "EQUIVALENCE FAILED"
  exit 1
fi
echo "ALL EQUIVALENCE CHECKS PASSED"
