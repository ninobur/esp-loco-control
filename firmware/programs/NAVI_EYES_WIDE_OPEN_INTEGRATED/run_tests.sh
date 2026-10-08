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
"$cxx" $flags "$here/tests/test_integrated_core.cpp" -o "$build_dir/core"
"$build_dir/core"
"$cxx" $flags "$here/tests/test_recorder.cpp" -o "$build_dir/recorder"
"$build_dir/recorder"
# Retired from the active suite by the legacy station-machinery removal pass.
# Kept in tests/ as historical evidence of the removed StationMachine and
# EwoStationStopProfile operating behavior.
"$cxx" $flags "$here/tests/test_review_regressions.cpp" -o "$build_dir/review"
"$build_dir/review"
"$cxx" $flags "$here/tests/test_ir_authority.cpp" -o "$build_dir/authority"
"$build_dir/authority"
"$cxx" $flags "$here/tests/test_first_target_after_declare.cpp" -o "$build_dir/first_target"
"$build_dir/first_target"
"$cxx" $flags "$here/tests/test_pwm_zero_localization.cpp" -o "$build_dir/pwm_zero"
"$build_dir/pwm_zero"
"$cxx" $flags "$here/tests/test_ir_configuration.cpp" -o "$build_dir/ir"
"$build_dir/ir"
"$cxx" $flags "$here/tests/test_pulse_observation.cpp" -o "$build_dir/pulse"
"$build_dir/pulse"

sh "$here/../NAVI_EYES_WIDE_OPEN/run_tests.sh"
