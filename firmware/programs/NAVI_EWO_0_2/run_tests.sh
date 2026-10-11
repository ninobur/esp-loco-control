#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
cxx='c++'
flags='-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined'

"$cxx" $flags -DHALL_POLARITY_INVERTED=true "$here/tests/test_map_adapter.cpp" -o "$build_dir/map_otto"
"$build_dir/map_otto"
"$cxx" $flags -DHALL_POLARITY_INVERTED=false "$here/tests/test_map_adapter.cpp" -o "$build_dir/map_toby"
"$build_dir/map_toby"
# 0_1 target-only assertions are historical; 0_2 tests the approved evidence model.
"$cxx" $flags "$here/tests/test_evidence_reconciliation.cpp" -o "$build_dir/evidence"
"$build_dir/evidence"
"$cxx" $flags "$here/tests/test_recorder.cpp" -o "$build_dir/recorder"
"$build_dir/recorder"
# Target-only review fixtures remain retained as 0_1 historical evidence.
"$cxx" $flags "$here/tests/test_stop_overlay.cpp" -o "$build_dir/stop_overlay"
"$build_dir/stop_overlay"
# Target-only IR-authority and first-target fixtures are retained for 0_1
# comparison; their authority assertions conflict with the approved 0_2 model.
# PWM-zero fixture is retained; its setup assumes 0_1 target-only confirmation.
"$cxx" $flags "$here/tests/test_ir_configuration.cpp" -o "$build_dir/ir"
"$build_dir/ir"
"$cxx" $flags "$here/tests/test_pulse_observation.cpp" -o "$build_dir/pulse"
"$build_dir/pulse"

sh "$here/../NAVI_EYES_WIDE_OPEN/run_tests.sh"
