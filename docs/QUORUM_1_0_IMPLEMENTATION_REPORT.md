# QUORUM 1.0 — implementation report

**Describes commit fa59756, renamed at 4c80c3a, amended per CODEX review.**

Implementation of `docs/QUORUM_v3_0_implementation_spec.md` Revision 20 on the
v2.22 baseline. Originally shipped as `SOLONAV_3_0` (fa59756), renamed to
`firmware/QUORUM/QUORUM.ino`, `SKETCH_NAME "QUORUM_1_0"` (4c80c3a, no code
changes). The CODEX implementation review of this commit is in
`docs/QUORUM_1_0_CODEX_FINDINGS.md`; its four High findings are fixed in
QUORUM 1.1.

## Build figures

Both locomotive profiles (`LL_LocoConfig_9950011.h`, `LL_LocoConfig_9950012.h`)
compile identically:

| | flash | static RAM |
|---|---|---|
| v2.22 baseline | 72% | 15% |
| QUORUM 1.0 | 951,451 bytes (72%) | 50,076 bytes (15%) |

`LocoConfig.h` restored to the 9950011 selector after the Toby build; empty
diff.

## Certified-property statement (amended)

**Zero unapproved deviations; one operator-approved deviation from §6.2 (DNA
recovery code retained as compiling dead code by operator instruction).**

CODEX contested the original "zero touches" wording, correctly: the spec's
§6.2 orders `dnaBuf`, `dnaBufLen`, `dnaPush()`, `dnaMatch()`, `DNA_W` and the
reacquisition constants removed, and a separate operator instruction ordered
them retained as dead code — present, unreferenced, compiling. That
instruction legitimately overrides the spec, but it is a deviation and is
recorded as one. `DNA_W` and `REACQ_WINDOW_MARKERS` are retained with it
because `dnaMatch()` does not compile without them; the rest of §6.2's list
(`pendingMm`/`pendingValid`/`pendingConfirms`, `REACQ_CONFIRMS`,
`lastOdomDisagreement`, `navConfidence`, `navEnterLost()`) is deleted.

The navigator control logic itself was translated as certified — the state
machine and its eight transitions per §2's table; `navMm` advancing on every
navigation-accepted event in all post-declaration states; offsets applied only
as `routeMod(navMm + navDir*offset)`; plain match-count scoring against each
entry's recorded coordinate; validation owning the comparison outright;
reopening removing the correction from the current odometer and rebasing the
ring; `evalCount` starting at 3; adoption tested before the hard bound;
`NO_PREV` bootstrapping and refusing zero-dt; rejected events never touching
the predecessor; every accepted ACTIVE event replacing it;
`conserveIntervalIndex` as the interval that ended at `navMm`;
`endSuccessfulIncident()` and `closeIncidentNoQuorum()` distinct; snapshot
preceding the stop request. CODEX confirmed all twenty properties against the
source.

## Ambiguity resolutions (eight, with CODEX verdicts)

1. **SKETCH_NAME** — spec header said `"SOLONAV_3_0_QUORUM"`, the task
   instruction said `"SOLONAV_3_0"`. The instruction won. Superseded by the
   QUORUM rename; ratified.
2. **Dead DNA recovery code retained** — operator instruction overrides §6.2.
   Contested by CODEX as written; resolved by the amended deviation statement
   above and closed as a documented deviation.
3. **`publishBootId`** reported `conf_max` (a `navConfidence` constant the
   spec deletes) in a function not on the permitted-edit list. Replaced
   `dna_w`/`conf_max` with `quorum_trigger`/`quorum_margin`/`quorum_max` —
   reporting deleted machinery in the identity message would be misleading.
   Ratified.
4. **`state` and `nav_state` both published**, same value via
   `navStateName()` — §0.1 says "add nav_state" while `state` already exists
   and has consumers. Ratified.
5. **`QUORUM_OPEN` / `QUORUM_CLOSED`** — §5.1 requires incident open/close
   decision events but never names them. Ratified.
6. **AGREE/DISAGREE kept on `pub()`** — read as state-stream messages rather
   than §5.1 decision events. **Contested by CODEX** (they are one-time
   observations and were evictable); recommendation adopted in QUORUM 1.1:
   event-bearing AGREE/DISAGREE ride `pubMarker()`.
7. **loopstat `conf` → `miss_streak`** — `navConfidence` deleted with all its
   constants; §5 requires `miss_streak` reconstructable from the log.
   Ratified.
8. **Alert payload** — `confidence` removed (never reintroduced as
   telemetry); `candidate_mm` repurposed as the leader's implied position or
   −1; `viable` array added per §2.5 step 3. Semantics ratified;
   implementation contested on size (the 640-byte build could exceed the
   512-byte transport) — fixed in QUORUM 1.1 with worst-case arithmetic of
   498 bytes.

## §8 desk-check results

**Verified by grep:**

- `grep mqtt\.` — hits only in `subscribeAll()` (called solely from
  `attemptReconnect`), `attemptReconnect()`, `networkTask()`, `setup()`.
  Nothing in Layer 3.
- `grep no_quorum` — the topic appears in its declaration, `buildTopics()`,
  and the network task's reconciliation block only; no `pub()`/`pubMarker()`
  call site touches it.
- `grep delay(` — only `calibrate()` and `setup()`; every publish out of
  `navOnMarker()` enqueues or writes the RAM slot.
- `grep navState` — all fifteen consumers go through `navPositionUsable()` /
  `navStateName()` / `navAlertLevel()` or the enum comparisons the spec
  itself specifies.

**Verified by compile:** both config headers, clean, figures above.

**Verified by construction** (the code implements the checklist item
directly): direction/wrap arithmetic via `routeMod` only; viability strictly
`< QUORUM_MARGIN`; conservation index at MM000 (CW→`spacingMm[170]`,
CCW→`spacingMm[0]`); units (×1000, `expectedDtMs`); `fabsf`; NO_PREV
bootstrap + zero-dt refusal; twelfth-event adoption precedence; publish-
before-clear in `adoptLeader()`; `evalCount = 3` in both entry paths with no
re-push of the triggering three; timing-gate priority (LOW_PWM before RAMP);
event-open PWM pair beside `evStartBaseline`; stop-edge invalidation in
`servicePwmRamp()`; snapshot 512-byte budget with `SNAPSHOT_TRUNCATED` alert;
marker payload worst case 174 ≤ 320; both declaration paths through
`navDeclare()`; `lastMarkerMs = 0` in the full reset; `cmd/force_lost`
parsing per §6.5 (strtol + endptr, −8..+8, NOQUORUM-before-numeric,
rejections published).

## Pending field/replay items (listed, not skipped)

- Broker-stopped drive: `navMm` advances, adoption under induced offset,
  `loop_max_gap_ms` under ~80 ms
- Run 3 replay from MM154 (TIED at −1/+1, adopt −1, no stop) — and the
  2026-08-01 weak-magnet corridor as the acceptance scenario (offset 0 held
  through mm 124–115, peaks 40–98)
- Conservation-gate replay displaced −1/+1/+4, and under load, grade and
  battery states
- Snapshot-survives-broker-outage and clear-survives-outage reconciliation
  tests
- Live station reset after NO_QUORUM during ST_DEPART; stations arming and
  completing during EVALUATING
- Live fixture behaviour (`-4`, `NOQUORUM`, rejects); hard-bound stop;
  post-adoption tolerance sequences; AGREE finalisation sequences;
  NO_QUORUM deceleration marker handling
- Real-hardware timing: event-time PWM decisions under a 30 s drain delay

Per the spec's final line, no tag was created — the tag waits for the field.
