#!/bin/sh
# NAVI_ONE gates. Both must pass before anything is flashed.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../../../.." && pwd)
survey="$repo/field-records/logs/20260828_survey/toby_1_13X_survey_waveforms.log.gz"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "== gate 1: survey replay (real recognizer, real waveforms) =="
gunzip -c "$survey" > "$tmp/survey.log"
c++ -std=c++17 -O1 -Wall -Wextra "$here/replay_survey.cpp" -o "$tmp/replay"
"$tmp/replay" "$tmp/survey.log"
