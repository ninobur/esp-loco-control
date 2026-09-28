#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/navi_eyes_wide_open_test"
c++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_core.cpp -o "$OUT"
"$OUT"
