#!/bin/sh
# Rebuild the committed navlab timing database from COMMITTED inputs only.
# Every command here is the exact command of record (plan artifact 2:
# reproducibility). The two big captures are committed as filtered
# navlab-inputs (only the topics normalize_log.py consumes; proven equivalent
# to the full logs modulo source line numbers, sha256 82ecc565... on
# 2026-08-22); the beta log is committed in full.
set -eu
# Reproducibility claim, stated precisely (iteration-2 correction 6b):
# rebuilding with the SAME invocation is BYTE-IDENTICAL (metadata derives from
# input content, never the wall clock; demonstrated 2026-08-22 by build, copy,
# rebuild, cmp). Rebuilding into a DIFFERENT output path is semantically
# equivalent but not byte-identical: the header honestly records the
# build_command and records_file paths, which then differ. Re-verify:
#   sh tools/navlab/rebuild_db.sh && cp tools/navlab/db/timing_db_v1.json /tmp/a \
#   && sh tools/navlab/rebuild_db.sh && cmp /tmp/a tools/navlab/db/timing_db_v1.json
cd "$(dirname "$0")/../.."
OUT=${1:-tools/navlab/db}
mkdir -p "$OUT"
python3 tools/navlab/normalize_log.py \
  --capture field-records/logs/navlab-inputs/20260820_morning_session.log \
  --capture field-records/logs/navlab-inputs/20260813_otto_1_13_noquorum_watch.log \
  --capture field-records/logs/20260811_QUORUM_1_13_beta_otto.log \
  --out "$OUT/records_v1.jsonl"
python3 tools/navlab/build_timing_db.py \
  --records "$OUT/records_v1.jsonl" \
  --out "$OUT/timing_db_v1.json" \
  --holdout "20260820_morning_session.log:9950011:boot16" \
  --holdout "20260820_morning_session.log:9950012:boot2" \
  --holdout "20260811_QUORUM_1_13_beta_otto.log"
