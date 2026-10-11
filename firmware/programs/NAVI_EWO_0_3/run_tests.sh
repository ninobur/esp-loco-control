#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../../.." && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
# LSAN cannot inspect /proc tasks in the Work container; ASAN/UBSAN stay enabled.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
flags='-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined'
sh "$here/../NAVI_EWO_0_1_MM045_STOP/run_tests.sh"
for test in evidence ir judgment bridge navigation pulse_observation; do
  c++ $flags "$here/tests/test_$test.cpp" -o "$build_dir/$test"
  "$build_dir/$test"
  echo "PASS: test_$test"
done
python3 "$here/tests/run_unchanged_regressions.py"
cd "$root"
python3 -m unittest discover -s tools/tests
python3 "$here/tests/test_operational_shell.py"
python3 "$here/tests/test_stop_handoff.py"
python3 "$here/tests/test_pulse_ingress.py"
python3 "$here/tests/test_preservation.py"
python3 "$here/tests/replay_pulses.py"
python3 tools/test_ir_pulse_transport.py
python3 tools/test_ir_transport_status.py
