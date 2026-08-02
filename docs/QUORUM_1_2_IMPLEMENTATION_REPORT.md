# QUORUM 1.1 / 1.2 — implementation report

**Describes commits 3381570 (QUORUM 1.1) and 540962d (QUORUM 1.2).**
CODEX reviews of both are in `docs/QUORUM_1_0_CODEX_FINDINGS.md`. Navigator
control logic untouched in both versions; the twenty certified properties
stand (CODEX: "I found no regression in its certified properties").

## QUORUM 1.1 — terminal evidence fixes (CODEX 1.0 review, findings 1–6)

| Fix | Substance | CODEX 1.1 verdict |
|---|---|---|
| F1 | Snapshot `ld`/`ru` convert through `QUORUM_OFFSETS[]`, JSON `null` when absent — offsets, not indices | verified |
| F2 | Tear-free handoff: `portMUX` critical section; network task copies state+snapshot out under the mux, publishes from the copy; mux never held across `mqtt.publish()` | verified |
| F3 | Desired retained state persistent (NONE/SNAPSHOT/CLEAR, changed only by terminal entry and `navDeclare()`); per-connection `needsReconcile` flag re-armed on every reconnect; mirror-current-truth policy commented at the declaration | one remaining race → 1.2 |
| F4 | Alert compacted: worst case 497 chars + NUL = 498 ≤ 512, arithmetic shown field-by-field; non-dashboard names shortened (`motor_dir→mdir`, `rear_bound_mm→rear`, `front_bound_mm→front`, `envelope_mm→env`, `last_confirmed_mm→lc_mm`, `markers_since_confirmed→since`, `lost_markers→lostm`, `lost_count→losts`); `est_mm_s` clamped to 99999; oversize builds publish `ALERT_OVERSIZE`, never truncated JSON | verified |
| F5 | Event-bearing AGREE/DISAGREE ride `pubMarker()`; current-value state stays on `pub()`; `markerPubQueue` 64→128 (~66 KB heap); drain cap 8 unchanged | verified |

The retained DNA dead code stands as the single operator-approved deviation
from spec §6.2 (see the amended statement in
`docs/QUORUM_1_0_IMPLEMENTATION_REPORT.md`).

## QUORUM 1.2 — ABA race in reconciliation completion (CODEX 1.1 review)

The 1.1 success guard compared only the enum value, so a newer commit with
the same value was marked reconciled without ever being published: snapshot B
committed while snapshot A publishes (repeated `force_lost NOQUORUM`), or
SNAPSHOT→CLEAR→SNAPSHOT during a slow publish.

Fix: `noQuorumGeneration` (uint32_t) increments under `noQuorumMux` on every
SNAPSHOT or CLEAR commit; the network task copies it out under the mux with
the state and snapshot; on publish success it clears `noQuorumNeedsReconcile`
only if **both** state and generation still match, checked under the mux. The
reconnect re-arm also takes the mux, per CODEX, for consistency. All five
generation access sites sit inside mux sections; no lock is held across
`mqtt.publish()`.

## Build figures

Both profiles identical at each version; `LocoConfig.h` restored after every
Toby build (empty diff). Identity check passes: `SKETCH_NAME` and header
comment agree at each version.

| | flash | static RAM |
|---|---|---|
| QUORUM 1.0 | 951,451 (72%) | 50,076 (15%) |
| QUORUM 1.1 | 951,775 (72%) | 50,588 (15%) |
| QUORUM 1.2 | 951,831 (72%) | 50,596 (15%) |

The F5 queue growth is heap (~33 KB additional at runtime), not statics;
~210 KB of heap headroom remains after all queues.

## Status

CODEX verdict after 1.1: "Add the retained-state generation check, then it
is ready for the §8 field/replay campaign." The generation check is QUORUM
1.2. The §8 field/replay items pending are listed in
`docs/QUORUM_1_0_IMPLEMENTATION_REPORT.md`; no tag until the field run.
