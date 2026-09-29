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
"$cxx" $flags "$here/tests/test_operational_adapters.cpp" -o "$build_dir/ops"
"$build_dir/ops"
"$cxx" $flags "$here/tests/test_review_regressions.cpp" -o "$build_dir/review"
"$build_dir/review"
ir="$here/../../reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4"
"$cxx" $flags -I"$ir" "$ir/tests/test_ir_architecture.cpp" -o "$build_dir/ir"
"$build_dir/ir"
stations="$here/../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH"
"$cxx" $flags -I"$stations" "$stations/tests/test_station_position.cpp" -o "$build_dir/stations"
"$build_dir/stations"

sh "$here/../NAVI_EYES_WIDE_OPEN/run_tests.sh"
