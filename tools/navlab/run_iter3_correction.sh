#!/bin/sh
# Commands of record for the iteration-3 CORRECTION round (2026-08-22).
# No navigator change, no threshold change, no firmware touched. Run from the
# repo root, after tools/navlab/rebuild_db.sh has produced db/records_v1.jsonl.
set -e
R=tools/navlab/db/records_v1.jsonl
D=tools/navlab/db/timing_db_v1.json
C=field-records/logs/navlab-inputs/20260820_morning_session.log
O=tools/navlab/results

# 1. is the dt=0 one-interval grant fail-safe? (development probe)
python3 tools/navlab/probe_dt0_unknown_time.py --out $O/iter3_probe_dt0_unknown_time.json

# 2. route-wide episodes, the boot1 contradiction, C8 expectation evidence,
#    and the corridor-speed audit behind the episodes
python3 tools/navlab/classify_iter3_evidence.py --records $R --db $D --capture $C \
  --episodes-report $O/iter3_dev_otto_b16.json \
  --contradiction-report $O/iter3_dev_otto_b1.json \
  --c8-report $O/iter3_dev_toby_b2.json \
  --out $O/iter3_evidence_classification.json

# 3. corrected iteration-3 verdict (all three replays are DEVELOPMENT data)
python3 tools/navlab/acceptance_checks.py --records $R --db $D --capture $C \
  --dev-report $O/iter3_dev_toby_b2.json \
  --dev-report $O/iter3_dev_otto_b16.json \
  --dev-report $O/iter3_dev_otto_b1.json \
  --probe $O/iter3_probe_dt0_unknown_time.json \
  --evidence $O/iter3_evidence_classification.json \
  --out $O/iter3_acceptance_corrected.json || true

# 4. iteration 2 rescored under the same corrected checker
python3 tools/navlab/acceptance_checks.py --records $R --db $D --capture $C \
  --report $O/iter2_toby_strict.json --report $O/iter2_otto_strict.json \
  --probe $O/iter3_probe_dt0_unknown_time.json \
  --out $O/iter2_acceptance_corrected.json || true

# 5. behavioural tests (the dt=0 ones are development tests, not validation)
python3 tools/navlab/test_reachability_nav.py
python3 tools/navlab/test_navlab.py
