# 0045 — Navigation has one authoritative position outcome; R3 proposes, the ladder disposes

Status: Accepted (operator via CODEX requirements, 2026-08-27)

## Decision

`navMm` is written in exactly one place on the event path — `acceptEvent()`.
The R3 identity layer no longer mutates position; it produces a **proposal**
(confirmation, or a bounded forward correction offset) that rides the event
through the inherited timing/quarantine ladder and commits **atomically with
acceptance or not at all**. `navOnMarker()`/`navLadder()` return an explicit
disposition (`ACCEPTED`, `QUARANTINED`, `PHANTOM_REJECTED`, `NO_POSITION`,
`NO_DIR`), and R3 resynchronizes its shadow **only on `ACCEPTED`** — never to
an unconfirmed result.

Additionally, physical reachability is a candidate **veto**, not a weighted
attribute: a candidate whose map distance over the measured elapsed time
implies more than 800 mm/s (the same bound `Q_FLOOR_MS` derives from) is
excluded from identity scoring outright.

## Context

TEMPLATES 0.3A's first field run drifted 82 markers (physical 042–043,
software 124). Trace: R3 confirmed a passage and the inherited conservation
gate `PHANTOM_REJECTED` the same passage; R3 neither received that
disposition nor knew its predicted advance had not occurred, and its
resynchronization then accepted the resulting `navMm` as truth. Corrections
are forward-only, so each such disagreement ratcheted position forward.
Separately, the run log showed corrections implying ~1,090 mm/s adopted at
76–83% confidence — physically impossible solutions that cost only the
timing attribute's 16 points.

## Alternatives considered

- **Vouch R3-confirmed passages past the phantom detector** (as
  quarantine-committed events are vouched). Rejected for now: the inherited
  detector is preserved unchanged; exempting R3 from it would be a separate,
  explicit design decision (CODEX requirement 6).
- **Roll back `navMm` after a refusal.** Rejected: the §3Q arbitration can
  legitimately advance position (pending commit) inside the same
  `navOnMarker()` call, so a blanket restore corrupts; threading the
  proposal to the single write point is exact.
- **Keep reachability as a weighted attribute.** Rejected: the firmware's
  own doctrine (`Q_FLOOR_MS`) treats physical impossibility as decisive,
  unconditioned. A veto refuses the impossible candidates the run actually
  adopted.

## Consequences

- A held (quarantined) event carries its proposal with it
  (`qPendingCorrOff`); the arbitration's H-genuine frame accounts for it.
- Confirmation now additionally requires the expected candidate to be the
  best non-excluded candidate; ambiguity (a farther candidate scoring higher
  without correction authority) holds instead of confirming — recoverable
  omission over false inclusion, per 0043.
- `diag/r3_admit` publishes proposal and committed outcome separately
  (short-key schema; mapping documented at `r3PublishDecision()`).
- A host regression suite now compiles the real sketch
  (`firmware/test-programs/TEMPLATES/tests/`, arduino-cli `--preprocess`,
  one code path) — 161 checks across ordinary confirmation, the MM101–104
  phantom-refusal regression, committed and refused corrections, quarantine
  commit/discard, relocation resync, and the EVALUATING bypass.

## References

- `docs/TEMPLATES_0_3B_POSITION_CONTRACT_REPORT.md` — implementation report
- `docs/decisions/0044-admission-identifies-targets-rather-than-rejecting-signals.md`
- Field run `9950012_20260827_180317` (Pi) — the 82-marker drift evidence
- CODEX synthesized requirements, 2026-08-27 (eight-point fix contract)
