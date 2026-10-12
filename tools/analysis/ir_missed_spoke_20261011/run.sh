#!/bin/sh
# Reproduce the October 11 IR missed-spoke analysis. Read-only: extracts inputs
# from repository history into a work directory and prints results.
set -eu
REPO=$(git rev-parse --show-toplevel)
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${1:-$(mktemp -d)}
mkdir -p "$WORK/data" "$WORK/code" "$WORK/build/tests" "$WORK/scripts"
cp "$HERE"/*.py "$WORK/scripts/"
P=firmware/programs/NAVI_EWO_0_3
for f in oct09_healthy oct09_run1_onset oct09_run2_onset oct10_121800; do
  git -C "$REPO" show 253faa4:$P/tests/fixtures/$f.txt.gz > "$WORK/data/$f.txt.gz"
done
git -C "$REPO" show bf95cc4:field-records/logs/20261009_MM045_EVENING/all_20261009.log.gz > "$WORK/data/all_20261009.log.gz"
git -C "$REPO" show 85a2cf2:firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h > "$WORK/code/RouteMap.h"
git -C "$REPO" show 253faa4:$P/NaviIrEvidence.h > "$WORK/build/NaviIrEvidence.h"
git -C "$REPO" show 253faa4:$P/tests/replay_timing.cpp > "$WORK/build/tests/replay_timing.cpp"
cd "$WORK"
echo "== previous 150%-of-median detector (253faa4)"
c++ -std=c++17 -O2 build/tests/replay_timing.cpp -o build/replay
for f in oct09_healthy oct09_run1_onset oct09_run2_onset oct10_121800; do printf '%s ' $f; gzip -dc data/$f.txt.gz | ./build/replay; done
python3 -I scripts/extract.py data/all_20261009.log.gz data/oct09_events.tsv
echo "== >185%-of-previous rule"
for f in oct09_healthy oct09_run1_onset oct09_run2_onset oct10_121800; do python3 -I scripts/rule185.py fx data/$f.txt.gz $f; done
python3 -I scripts/rule185.py ev data/oct09_events.tsv "Oct 9 full evening"
echo "== candidate spacing";  python3 -I scripts/period.py ev data/oct09_events.tsv scripts
echo "== map distance";       python3 -I scripts/mapcheck.py | head -8
echo "== atypical candidates"; python3 -I scripts/odd.py
echo "== comparators";        python3 -I scripts/comparators.py
echo "== segment detector";   python3 -I scripts/segment_detector.py
