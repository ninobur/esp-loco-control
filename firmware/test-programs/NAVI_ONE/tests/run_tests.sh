#!/bin/sh
# NAVI_ONE gates. ALL OF THEM must pass before anything is flashed.
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

echo ""
echo "== gate 5: waveform window + wire format =="
$CXX "$here/gate_waveform.cpp" -o "$tmp/waveform"; "$tmp/waveform"

echo ""
echo "== gate 6: polarity is decided by the whole passage =="
$CXX "$here/gate_polarity.cpp" -o "$tmp/polarity"
"$tmp/polarity" "$repo/field-records/logs/20260831_navi_one_waveforms/captured_passages.tsv"

echo ""
echo "== gate 7: survey polarity replay — 2026-08-28 circuit =="
$CXX "$here/replay_polarity_survey.cpp" -o "$tmp/polsurvey"
"$tmp/polsurvey" "$tmp/survey.log"

echo ""
echo "== gate 8: acquisition under a sustained DC offset =="
$CXX "$here/gate_baseline_latch.cpp" -o "$tmp/latch"; "$tmp/latch"

echo ""
echo "== gate 9: section cruise — the Grillers climb and its ramp-down =="
$CXX "$here/gate_section_cruise.cpp" -o "$tmp/section"; "$tmp/section"

echo ""
echo "== gate 10: stations — approach, stop, dwell, departure =="
$CXX "$here/gate_station.cpp" -o "$tmp/station"; "$tmp/station"
