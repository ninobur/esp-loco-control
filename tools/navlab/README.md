# navlab — the off-locomotive navigation laboratory

Implements the bounded deliverable of
`docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (commit `1df5927`). One work item,
five artifacts, binary acceptance criteria. Nothing here touches firmware,
MQTT, or the Pi contract.

| artifact | file | status |
|---|---|---|
| 1. log normalizer | `normalize_log.py` | built; validated against two independent earlier counts (14,564 records; per-boot sessions match `extract_session.py`) |
| 2. timing-database generator | `build_timing_db.py` | built; first query reproduced the mm 101 incident's cause (measured 1317–1647 ms where the PWM model demanded 2226) |
| 3. reachability navigator harness | — | next |
| 4. hold-out replay fixtures | — | after 3; sessions excluded via `--holdout` from day one |
| 5. event-level comparison report | — | last |

Known label caveat, stated in artifact 1's docstring: `phantom` labels trust
uncontradicted firmware rejections and are therefore contaminated by the
firmware's own proven false-rejection defect. Envelopes are built from
`genuine` labels only. Dwell contamination affects only the MAX side of
envelopes; the reachability-critical MIN side is immune.
