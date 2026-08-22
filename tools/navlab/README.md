# navlab — the off-locomotive navigation laboratory

Implements the bounded deliverable of
`docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (commit `1df5927`). One work item,
five artifacts, binary acceptance criteria. Nothing here touches firmware,
MQTT, or the Pi contract.

| artifact | file | status |
|---|---|---|
| 1. log normalizer | `normalize_log.py` | built; validated against two independent earlier counts (14,564 records; per-boot sessions match `extract_session.py`) |
| 2. timing-database generator | `build_timing_db.py` | built; first query reproduced the mm 101 incident's cause (measured 1317–1647 ms where the PWM model demanded 2226) |
| 3. reachability navigator harness | `reachability_nav.py` + `test_reachability_nav.py` (8 behavioural tests) | built; STRICT evaluation stops at the first contradiction |
| 4. hold-out replay fixtures | committed filtered inputs + `rebuild_db.sh` hold-out list | inputs committed under `field-records/logs/navlab-inputs/`, proven byte-equivalent to the full captures modulo line numbers |
| 5. event-level comparison report | `docs/QUORUM_NAVLAB_ITER2_REPORT.md` + `results/iter2_acceptance.json` | iteration 2: FAIL 7/9, 0 false confirmations; single named root cause |

Reproducibility: `rebuild_db.sh` is the exact command of record; it consumes
only committed files and regenerates `db/timing_db_v1.json` (the intermediate
records file is derived, not committed). Hold-out leak prevention is
structural (`dup_sources` exclusion, tested) and empirical for these captures
(`holdout_leaks_blocked: 0` — the 08-13 capture ends at 05:51, the morning
capture starts 06:02).

Evaluation modes — read before quoting any number:

- **STRICT** (default): stops at the first contradiction; every reported
  confirmation is the navigator's own. Toby held-out: 168 confirmations,
  1 pending, stopped at 10:06:48 in the known incident aftermath. Otto
  held-out: 145 confirmations, 27 pending, 1 full-circle window, stopped at
  its first contradiction.
- **CONTINUE-ANALYSIS** (`--continue-analysis`): after each contradiction the
  position is re-seeded FROM THE FIRMWARE LABEL, logged as EXTERNAL_RESEED
  and counted; the tool prints that the run does not measure navigator
  performance past the first contradiction. Exists solely so artifact 5 can
  classify every contradiction in one pass.



Known label caveat, stated in artifact 1's docstring: `phantom` labels trust
uncontradicted firmware rejections and are therefore contaminated by the
firmware's own proven false-rejection defect. Envelopes are built from
`genuine` labels only. Dwell contamination affects only the MAX side of
envelopes; the reachability-critical MIN side is immune.
