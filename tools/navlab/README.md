# navlab — the off-locomotive navigation laboratory

Implements the bounded deliverable of
`docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (commit `1df5927`). One work item,
five artifacts, binary acceptance criteria. Nothing here touches firmware,
MQTT, or the Pi contract.

| artifact | file | status |
|---|---|---|
| 1. log normalizer | `normalize_log.py` | built; validated against two independent earlier counts (14,564 records; per-boot sessions match `extract_session.py`) |
| 2. timing-database generator | `build_timing_db.py` | built; first query reproduced the mm 101 incident's cause (measured 1317–1647 ms where the PWM model demanded 2226) |
| 3. reachability navigator harness | `reachability_nav.py` | built; first held-out evaluations in `results/` |
| 4. hold-out replay fixtures | `db/timing_db_v1.json` hold-out list + normalized records | sessions declared at DB build; formal fixture files pending |
| 5. event-level comparison report | — | pending; first numbers below |

First held-out evaluations (sessions excluded from every envelope):

- **Toby 08-21 (mm 101 incident day)**: 1,506 events, 438 confirmations,
  4 doubtful events held, **1 contradiction, 0 corridor blowouts**, ends
  NORMAL. The navigator steps through the five-straight kill zone without
  hesitation and is locked on at mm 127 at the exact second of the
  firmware's NO_QUORUM. The firmware's day: a five-marker massacre, a
  wrong-station stop, one NO_QUORUM, one operator reset.
- **Otto 08-21 (cascade day)**: 1,455 events, 242 confirmations, 120 held,
  31 contradictions, 2 full-circle windows, ends NORMAL. Otto's evidence
  stream that day was genuinely corrupted for long stretches (slow-speed
  wrong-pole reads, fringe ghosts, saturated events); stopping with a
  contradiction there is the DESIGNED behaviour, against the firmware's
  silent 12-label slips. Each contradiction still needs event-level
  classification for the artifact-5 report (the evaluation harness
  re-seeds at the firmware label after each, and says so).

Known label caveat, stated in artifact 1's docstring: `phantom` labels trust
uncontradicted firmware rejections and are therefore contaminated by the
firmware's own proven false-rejection defect. Envelopes are built from
`genuine` labels only. Dwell contamination affects only the MAX side of
envelopes; the reachability-critical MIN side is immune.
