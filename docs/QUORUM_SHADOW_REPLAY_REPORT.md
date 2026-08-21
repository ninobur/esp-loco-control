# Shadow replay of the 0024 fix and the offsets widening — 2026-08-21

Harness-only, per the CODEX boundary. Nothing here is flashed. Two compile
flags, both proven inert when off (source line-identical to `b8b8814`;
firmware binaries embed `__DATE__`, so neutrality is proven at source level):
`Q_TIMING_MEASURED` (conservation gate) and `Q_OFFSETS_SYMMETRIC`
({−3…+4}, 8 candidates). Five real sessions extracted from the 30 h capture,
per-boot, replayed under per-locomotive profiles (Toby's through a
Toby-profile harness — the first attempt replayed him under Otto's quarantine
floor and diverged).

## Headline

| NO_QUORUMs | BASE | T4+O combined |
|---|---|---|
| Toby 08-21 | 2 (1 real + 1 replay artifact) | 1 (artifact only — **the real one is gone**) |
| Otto 08-21 (s05) | 7 | 1 |
| Otto 08-20 morning (s01) | 1 | 0 (via a **−2 adoption**) |
| Otto 08-20 16:0x (s02) | 0 | 1 (regression, retired-hardware era) |
| Otto 08-20 evening (s04) | 1 | 1 (relocated) |
| **total** | **11** | **4** |

Longest genuine-marker rejection run: **16 under BASE → 1–3 under T4**, by
construction.

## It took four attempts, and the failures are the finding

1. **T1 — the proposal as written** (`expectedDt = previousAcceptedDt`):
   spectacular on Otto's 08-21 session (7→1) and **catastrophic out-of-sample**:
   a dwell-spanning interval (40,441 ms observed; 65,535 saturated) becomes the
   predecessor, every genuine 1–3 s marker satisfies `dt ≤ 0.30×prev`, and the
   rejection freeze preserves the poison — runs of 43/42/28 consecutive
   genuine markers rejected on three of five sessions. **The proposal's §3
   claim that the trap cannot form is false**: it considered short poison only.
2. **T2 — trailing median**: worse (runs to 136; 2,514 rejections on one
   session). A median is robust and therefore STALE; after every speed change
   two new-regime intervals sum to one old-regime median, inside the band.
   The expectation must be reactive.
3. **T3 — T1 + predecessor sanity cap** (`Q_PREV_SANE_MS` 8000, the zero-dt
   seeding doctrine extended to the other extreme): totals 11→4, but a fresh
   17-run appeared — a legitimate 6 s crawl interval followed by acceleration
   still locks (0.3×6000 swallows genuine 1.8 s cadence).
4. **T4 — T3 + rhythm escape**: a phantom splits ONE interval ONCE, so two
   consecutive events at the same new cadence are a train, not noise. The
   event after a rejection, arriving within ±30% of the rejected interval, is
   accepted and re-seeds. **Caps every rejection run at ~1 structurally.**

## The offsets widening earns its place independently

With the fence {−3…+4}: s01's only NO_QUORUM resolves via **(mm 7, −2)**; on
s05 the 11:50:43 label-ahead cascade produces the predicted **(mm 30, −2)**
adoption. Toby's column is identical to BASE — his failures were
rejection-driven, exactly as diagnosed. The operator's gate ("proved through
the replay, not treated as independently proven") is met in direction, with
the caveat that under T4+O the cascade still ends one NO_QUORUM (trajectory
differs event-by-event; needs event-level review).

**Companion fix required:** with 8 candidates the nav-publish buffers overflow
(`char sc[48], ex[48]` — eight `false`s need 50 bytes), truncating EVERY nav
JSON mid-token. First replay of the O column parsed zero nav events because of
it. Fixed as `char sc[8*QUORUM_CANDIDATES]…` (identical at 6). Shipping the
widened fence without this corrupts all navigation telemetry.

## Honest limits

- **Not suite-clean:** the T4 flag fails 26 existing suite checks (rescue
  paths, pinned counterfactuals, IR Test A sequences). Each needs
  adjudication — "fixture encodes the old gate" vs "genuine regression" —
  before any firmware approval.
- **s02 regression** (noq at 26 where BASE had none): that session is the
  entry-gate-90 era with markers physically missed; the era's cause is
  retired, but the regression is unexplained at event level.
- **Overfit risk:** four variants iterated against the same five sessions.
  Hold-out validation against the 2026-08-11 and 2026-08-13 captures has NOT
  been run.
- Replay fidelity is imperfect (current-tree constants; amplitude map not in
  fielded firmware). BASE reproduces all four of Otto's 08-21 NO_QUORUM
  locations exactly (35/37/115/5), which is the fidelity that matters here.

## Recommended sequence

1. Event-level review of the s02 regression and the s05 TO trajectory.
2. Hold-out replays (2026-08-11 beta log, 2026-08-13 capture).
3. Adjudicate the 26 suite failures; re-pin fixtures that encode the defect.
4. CODEX review lifting (or upholding) the 0024 harness-only boundary.
5. Only then: decision record, firmware implementation, supervised field test.
