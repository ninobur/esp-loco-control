#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
for test in test_map_adapter test_recorder test_stop_overlay test_ir_configuration test_pulse_observation test_dart test_dart_invariants; do
  c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/tests/$test.cpp" -o "$build_dir/$test"
  "$build_dir/$test"
done
# The five legacy detector/window-policy suites remain unedited in tests/.
# Their identical-input baseline/experimental outcomes are retained in
# dart-validation/inherited-results.json, not relabelled as passing.
