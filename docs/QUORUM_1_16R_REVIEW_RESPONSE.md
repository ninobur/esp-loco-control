# QUORUM 1.16R — response to CODEX's seven-finding review

**Date:** 2026-08-14. **Verdict accepted in full:** 1.16 (`6ebb3b1`) was not
flashed and decision 0035 was not marked Accepted. All seven findings were
confirmed against the code and the captures, fixed or resolved as below, and
the entire proof chain re-run: both-era suite green, statediff enumeration
regenerated (`docs/QUORUM_1_16R_STATEFUL_DIFF_ENUMERATION.txt`), both
locomotive profiles compiling clean. Sketch renamed `QUORUM_1_16R` so the
reviewed binary and the fixed binary can never be confused. **Still NOT
flashed** — the gate is operator + CODEX re-review of this document and the
amended 0035.

Two findings turned out to be **live in the captures**, not just reachable:
finding 2 at mm 85 of the 2026-08-10 session, finding 5 in toby_0813_s02.
The reviewer read the integration; the replays then paid out the predictions.

---

## Finding 1 (blocking) — SELF_RESOLVED could restart an AUTO locomotive

**Confirmed exactly as written.** `enterNoQuorum()` never drops
`autoRunning` (field evidence agrees: the 2026-08-11 beta log
`20260811_QUORUM_1_13_beta_otto.log` holds 364 NO_QUORUM alerts with
`auto:1`, and the 2026-08-14 watch log repeats the pattern — M4 was a
promise, not a mechanism). `nqLearn()` restored
`NAV_NORMAL`, and `serviceStations()`'s idle branch would have re-requested
cruise on the next loop pass. A lost-and-found locomotive would have moved
with no operator in the loop.

**Fix:** `nqDropAutoInterlock()`, called by the resolution before it returns
to NORMAL: drops `autoRunning`, zeroes the request (belt and braces — it is
already zero in NO_QUORUM), clears any armed station phase
(`stationReset("SELF_RESOLVED")`), and publishes
`"SELF_RESOLVED: position recovered - BEGIN to resume"`. Enrolment is kept,
so a deliberate BEGIN resumes the session — the same shape as
DISPATCHER_STOP. Knowledge recovery is not motion recovery, now enforced.

**Coverage:** the harness never calls `serviceStations()` — CODEX correctly
identified why no fixture could catch this. The dump now reports the
`autoRunning` flag itself, and `syn_adv_self_resolution_interlock` pins it:
auto goes in 1 and must come out 0 (`final_auto` is a suite assertion key
now).

## Finding 2 (blocking) — the legacy gate could kill a successor-authenticated commit

**Confirmed, and live in the 2026-08-10 capture.** The reproduction's exact
parameters are spacing-dependent — which spacingMm[] entry the event lands
on decides whether the reject band swallows the folded interval — and that
arbitrariness is itself the point 0024 made against the PWM model. On the
capture, the mm 85 commit (dt 246, peak 38) was being committed by
arbitration and then `PHANTOM_REJECTED` one line later by the conservation
gate. The "reversibility ×2 on real data" claim in the first
implementation report was therefore half-illusory: one of the two
reinstatements never actually landed. The review caught what the
enumeration's summary statistics hid.

**Fix:** `navLadder()` takes a `vouched` parameter. A committed pending
event passes the conservation branch untested — the telemetry is still
computed and published, but the map's authentication outranks the model.
The unvouched path is byte-for-byte unchanged.

**Load-bearing coverage:** `syn_adv_commit_survives_gate`, built at pwm 60 /
2200 ms cadence with an 800 ms damning-credentials genuine event. Swept
against BOTH binaries: on the pre-fix build (rebuilt from `6ebb3b1`) it
ends mm 20 with 2 disagreements — commit, then killed; on 1.16R it ends
mm 21 with 0. Exactly 4 of 26 polarity-eligible start positions expose the
defect — the spacing dependence, measured.

**Consequence, pinned rather than hidden:** with the mm 85 commit landing,
its genuine successor is then eaten by the same legacy gate (246 + 1222 ms
sums into the reject band — 0024's documented defect biting the *other*
event of the pair; either way exactly one of the two advances, so the count
is identical, but the ring polarity differs). The second pass through the
defective stretch now ends in an honest HARD_BOUND terminal at mm 87
instead of a lucky +2 adoption, cleared four events later by the operator's
declare that was already in the record. (The first draft of this response
said 15; the verification fleet counted.) Both terminal boards are pinned in
QGOLD; the session still ends NORMAL at mm 8 with SELF_RESOLVED 13 → 8, and
input-invariance now also asserts the mm 87 terminal appears in both
counterfactual branches (it is phantom-independent, as it must be). The
stretch itself is the phantom-source record's maintenance territory,
physically fixed on the railway since.

## Finding 3 (blocking) — suffix rescue held position authority with zero tests

**Confirmed.** No fixture produced `QUORUM_SUFFIX_RESCUE`; the only
hard-bound fixtures never had a cleanly-fitting suffix.

**Coverage built, all sweep-found rather than hand-picked — and every sweep
script is committed under `firmware/QUORUM/tests/sweeps/` with its headline
count, so each number below is re-runnable rather than testimonial:**

- `syn_adv_rescue_straddle` — the positive. A missed marker opens an
  evaluation toward +1, a same-pole insertion mid-window returns the true
  offset to 0, and the board arrives at the hard bound straddled (margin
  never ≥ 2 across twelve evaluations — the synthetic twin of the 17:21
  `[8,8,8,4,7,3]` board). Rescue adopts offset 0 on a suffix of 9; the
  session ends NORMAL at the TRUE position. 39 of 855 swept CW straddles
  fire; start 36 is pinned.
- `syn_adv_rescue_ccw` (start 50) and `syn_adv_rescue_cw_wrap` (start 150,
  window crossing the 170/000 seam — `routeMod()` inside `suffixLenAt()`
  is load-bearing). The strict CCW sweep (final state, direction AND
  position all verified) found 53 hits, none crossing the wrap — so
  direction and seam are two fixtures, and that map limit is stated. (An
  earlier draft claimed a CCW wrap hit; it was an artefact of a `motor 0`
  command silently flipping navDir back to CW, and the strict assertions
  killed it. The negatives earn their keep.)
- `syn_adv_rescue_refuses_excluded` — a correct +1 adoption broken by an
  insertion during validation (REOPENED, +1 excluded), a second miss making
  +1 true again, and at the hard bound +1 fits with a PERFECT suffix of 12 —
  and is still refused. One failed adoption per candidate per incident;
  recovery from that corner belongs to self-resolution, which starts from
  fresh evidence and no exclusions.
- The no-fit negative was already covered: the three outside-fence fixtures
  reach HARD_BOUND with no candidate fitting and must still go terminal.

**The probability claim is retired, replaced by a map fact the suite now
proves on every run:** two candidates can both fit a clean 7-suffix only if
the DNA agrees with itself at their lag (candidate offsets −1..4 → lags
1–5) for 7 consecutive positions. The longest such run on NGR_DNA1 is **6**.
Ambiguous rescue is impossible on this route, and `SUFFIX_RESCUE_N = 7` is
the *minimum* length with that guarantee — a magnet change that creates a
7-run fails the suite loudly. CODEX's sharper reading of the 1/128 figure
(per candidate, ~6 candidates) was right as probability; the exact
computation is stronger and replaces it. Honest residual: at lag 6 —
reachable only against a truth *outside* the fence — two runs of length ≥ 7
exist (one of 8, one of 7), so a false full-fit is not map-impossible
there; a 513-run sweep of the
out-of-fence shapes found no input that lands on it, and post-adoption
validation is the net if one ever does.

## Finding 4 — the successor testified before its own credentials were checked

**Confirmed.** The arbitration used the successor's polarity before asking
whether the successor itself deserved belief.

**Fix:** the witness is credential-checked FIRST, on the raw pending→e
interval — under H-genuine that IS the witness's own interval, so failing
on it disqualifies genuine testimony outright; under H-phantom the
witness's true interval is the folded one, and it is re-judged on exactly
that, like any other arrival. An unfit witness leaves only the primary
verdict for the pending (`SUCCESSOR_SUSPECT` — a new discard reason, and
`discard_reasons` is now an ordered suite assertion so a regression that
flips one verdict into another fails even when counts agree). The interval
fold runs on every *arbitration* discard path; the DIRECTION_CHANGED
discard on reversal stands apart, deliberately — the arbitration frame died
with the reversal and there is no successor to fold into.

**This is also the slow-consecutive-family answer** CODEX asked for: each
unfit witness discards its elder and is held in turn on the folded
interval, until a genuine event finally testifies.
`syn_adv_slow_phantom_family` pins the full chain — two conjunction-level
phantoms the floor cannot touch, verdicts
`CONJUNCTION → SUCCESSOR_SUSPECT → CONJUNCTION → SUCCESSOR_FITS_PHANTOM`,
exact count, zero disagreements — and `syn_adv_double_phantom` now pins the
same ordering for the floor-failing pair. On the ten capture segments the
witness check changes no outcome (every existing witness was fit), which is
the right null result: the rule is armour, not behaviour change.

## Finding 5 — PWM 39 vs 40 decided whether an event faced scrutiny

**Confirmed, and live in toby_0813_s02.** The `pwm >= 40` condition was
inherited from the calibration filter, but as a runtime gate it meant a
*reported* PWM below 40 exempted an event from examination — PWM as motion
evidence again, decision 0024's own argument turned against the design.

**Fix:** the condition is deleted; the trailing-median interval band
(600–4000 ms) IS the operating-regime test, measured rather than asked
for. The capture paid out immediately: two toby_0813_s02 events at PWM 24
and 19 — each with ~20 SECOND durations, the crawl-artefact fingerprint —
had been exempt, admitted, and each cost a full evaluation cycle with a
corrective adoption. Now both are quarantined (duration credential) and
discarded; the segment's disagreements fall 24 → 4, agreement rises
331 → 353, same final position. This is the only capture-level behaviour
change the review round introduced outside the 2026-08-10 goldens **on the
ten fidelity-verified segments**; of the two segments excluded for runlog
reception gaps, toby_0813_s03 also improves the same way (one more
quarantine, disagree 17 → 7 vs pre-review 1.16). Enumerated in the R diff
file.

## Finding 6 — "3 further confirmations" vs 2 in the code

**Confirmed as a words-vs-code mismatch; resolved in the RECORD, not the
code.** CODEX offered both remedies. The capture decided: the resolution
counts three consistent matches in all (the unique match plus two further
advancing confirmations), and each match is a route-wide-unique 12-window
(suite-proven map fact) re-verified under coherent advance. The stricter
reading was implemented and measured first: on the 2026-08-10 capture it
pushed the recovery — which lands on the session's FINAL event, fresh = 17 —
past the end of the record, un-healing incident C and breaking
input-invariance, while buying a fourth verification of evidence that is
already unique three times over. In live running that cost is one marker
of delay; on the strongest real-data demonstration it is the difference
between recovered and stranded. 0035 now states the rule precisely, with
this datum.

## Finding 7 — documentation corrections

All accepted and applied, plus two the re-audit found:

- **+1, not −1** for a wrongly discarded genuine event (odometer behind →
  true position ahead). Fixed in 0035 and in the arbitration comment; both
  now state the direction of the error, not just the sign.
- **1.81×, not "more than twice"**: 280 mm / 350 ms = 800 mm/s vs the
  441 mm/s p99.9. The 2× factor was applied deriving 317 ms; rounding up to
  350 lands at 1.81×, and the record now says so.
- **Fixture count**: the report said 24, the generator held 23. The suite
  now holds **30**; the count in the report is taken from the generator's
  output, not remembered.
- **The 0025 collision**: every citation of the phantom-source maintenance
  record now names its branch (`agent/phantom-verdict-20260812`) and notes
  the collision with this branch's console-roster 0025. Renumbering stays
  queued for the branch consolidation.
- **`firmware/README.md` catalog row** was staged but in neither cited
  commit; it is included in the 1.16R commit.
- *(Found during the re-audit, same class:)* the 0035 totals line
  ("5,644 markers, 14 held") did not match its own cited enumeration file
  (5,544 / 15 pre-review). Corrected against the file: 5,544 / 17 / 17 / 0
  for 1.16R.

---

## What the re-run proof chain now shows

| check | result |
|---|---|
| Both profiles compile (`--warnings all`) | clean, 999,019 bytes (76%) |
| Quarantine-era suite (30 fixtures, map facts, QGOLD, counterfactual) | all checks passed |
| Legacy-era suite against the frozen pre-1.16 binary | all checks passed |
| Statediff, 10 verified segments, old vs 1.16R | zero adverse; only toby_0813_s02 changed vs 1.16 (finding 5, improvement) |
| 2026-08-10 goldens | A/B prevented; C byte-identical, advisory 18, SELF_RESOLVED 13 → 8; final NORMAL mm 8; honest new HARD_BOUND at mm 87, pinned board-by-board |
| Input-invariance | holds, including the phantom-independence of the mm 87 terminal |
| Pre-fix build cross-check | `syn_adv_commit_survives_gate` fails on `6ebb3b1` exactly as finding 2 predicts |

## What this round did NOT fix, on purpose

The legacy PWM conservation gate still stands and still costs one genuine
marker under maximum acceleration (`syn_adv_accel_max`) — and, newly
visible at mm 85→86, one of any vouched-commit-plus-successor pair whose
folded intervals land in its reject band. Its removal in favour of
quarantine alone is separate work with its own record, exactly as 0035
already says. The identity-error class (weak-first, on-time, wrong
polarity) remains out of scope; `syn_pair_weak_then_strong` still asserts
the limitation.
