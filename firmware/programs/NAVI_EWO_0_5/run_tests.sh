#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
for test in test_ir_credibility test_map_adapter test_recorder test_stop_overlay test_ir_configuration test_pulse_observation test_dart test_ewo_0_5; do
  c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/tests/$test.cpp" -o "$build_dir/$test"
  "$build_dir/$test"
done
# Original detector/window and first-increment suites remain unedited in tests/.
# Their 0_5 outcomes are recorded in dart-field-validation/regression-results.json;
# active 0_5 behavior is exercised by test_ewo_0_5 and the extracted .ino adapters.
