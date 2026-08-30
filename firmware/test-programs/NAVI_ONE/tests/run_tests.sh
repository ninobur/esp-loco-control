#!/bin/sh
# NAVI_ONE gates. ALL FOUR must pass before anything is flashed.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../../../.." && pwd)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
CXX="c++ -std=c++17 -O1 -Wall -Wextra"

echo "== gate 1: survey replay — the real recognizer on real waveforms =="
gunzip -c "$repo/field-records/logs/20260828_survey/toby_1_13X_survey_waveforms.log.gz" > "$tmp/survey.log"
$CXX "$here/replay_survey.cpp" -o "$tmp/replay"; "$tmp/replay" "$tmp/survey.log"

echo ""
echo "== gate 2: the contract =="
$CXX -Wno-unused-parameter "$here/contract.cpp" -o "$tmp/contract"; "$tmp/contract"

echo ""
echo "== gate 4: commands, interlocks, capture — the .ino layer =="
$CXX "$here/gate_ops.cpp" -o "$tmp/ops"; "$tmp/ops"

echo ""
echo "== gate 3: real-lap replay — the 2026-08-29 circuit =="
$CXX "$here/replay_lap.cpp" -o "$tmp/lap"
"$tmp/lap" "$repo/field-records/logs/20260829_navi2_first_lap/20260829_toby_navi2_markers.tsv"
