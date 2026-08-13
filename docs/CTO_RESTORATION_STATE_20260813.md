# State of the CTO restoration — 2026-08-13

**Purpose:** a starting brief for work toward **restoring two-train operation**
on the NGR Lowline. Written to be read cold.

**Goal, from `docs/ROAD_TO_CTO.md`:** two locomotives running the Lowline
simultaneously, coordinating their own spacing, without collisions or operator
intervention. That is milestone **M7**, and it depends on M1–M6.

**Premise that governs everything:** CTO2 was abandoned after repeated rear-end
collisions. Those were **not coordination failures**. The follower computed
clearance correctly from a leader position that was wrong — asserted at
certainty 1.000 after a handful of bad reads. *CTO cannot be safer than the
position reports it consumes.*

**The filter every proposal must pass** (operator, 2026-07-28): *does this make
navigation more reliable, or does it manage the consequences of unreliability?*
If the second, it is waste.

---

## 1. Milestone status, honestly

| | milestone | state |
|---|---|---|
| M0 | Established baseline | **done**, recorded |
| M1 | Position that cannot be confidently wrong | **satisfied by operator ruling, 2026-08-13** |
| M2 | Independent speed (IR) | **active work — integrate IR onto the locomotive** |
| M3 | Closed-loop stops | deferred; revisit after M2 |
| M4 | Correct behaviour when lost | largely built, not verified |
| M5 | Trustworthy self-report | payload exists, **never validated** |
| M6 | Peer awareness | harvest written, audit inventory not produced |
| M7 | Two-train operation | not started |

### M1 — satisfied. Operator ruling, 2026-08-13.

> **Operator ruling, 2026-08-13.** M1 is considered achieved to a reasonable
> degree. The milestone tests were commissioned from Sam as a planning aid and
> **are not to delay the project indefinitely.** The formal offline-aligned
> crossing test is not required before proceeding to M2.

The evidence supports the ruling on the plan's own terms. `ROAD_TO_CTO.md` sets
the viability threshold as *"at 0.5% error and 15-marker reacquisition, LOST
essentially never fires and the bounds are a formality"*. Toby, 2026-08-13,
`QUORUM_1_13`, 1682 markers over 64 minutes in both directions:

- **0.48% polarity mismatch** — below the stated threshold
- and that figure **overstates the error**: all 8 mismatches came from a single
  direction-reversal artefact, not 8 independent failures. Excluding it, **0%**
- 171 of 171 distinct markers crossed, `lost=0`, **zero weak events**
- the one offset that did occur was inside the fence, adopted and closed
  unaided in four seconds

The plan was written when the railway produced a phantom roughly once per lap.
Decision 0025 removed that condition. The threshold was set against a noise
floor that no longer exists.

**What remains formally undemonstrated**, recorded for honesty rather than as a
blocker: the offline alignment against the map was never run, so "zero position
assertions inconsistent with the odometer by more than 2 markers" is inferred
from firmware self-report rather than independently verified. The replay harness
in `firmware/QUORUM/tests/` can do this from existing captures at any time, with
no track time, if it is ever wanted.

<details>
<summary>The original M1 crossing test, for reference</summary>

Five laps, both directions, offline-aligned against the map: zero position
assertions inconsistent with the odometer by more than 2 markers; every phantom
and missed marker found by the aligner also flagged by the firmware at the time;
reacquisition landing within 2 markers of dead reckoning.

</details>

### What M1 delivered

All three parts of M1 are implemented in `QUORUM_1_13`:

- **1a, search from certainty** — the candidate fence `QUORUM_OFFSETS
  {-1,0,+1,+2,+3,+4}` with `REACQ_WINDOW_MARKERS 5`. Done.
- **1b, two confirmations** — exceeded: `QUORUM_TRIGGER 3`, `QUORUM_MARGIN 2`,
  `QUORUM_MAX 12`. Done.
- **1c, timing validation** — the conservation gate exists
  (`dt_conserve_ratio`, `DT_CONSERVE_TOL 0.30`) but decision 0024 established
  that its expected interval comes from a PWM velocity model measured wrong by
  1.56–1.78× on grades and on acceleration out of stops. The replacement
  (`expectedDt = previousAcceptedDt`) was **withdrawn** by decision 0025 once
  the phantom's cause turned out to be physical. So 1c is implemented,
  demonstrably imperfect, and currently has no evidence justifying a change.

**The crossing test has never been run.** It requires five laps, both
directions, **offline-aligned against the map**, showing zero position
assertions inconsistent with the odometer by more than 2 markers, every phantom
and missed marker flagged by firmware at the time, and reacquisition landing
within 2 markers of dead reckoning.

The plan states plainly that **M1's crossing test is the viability test for the
whole project**: at 0.5% error LOST essentially never fires and the bounds are a
formality; at 5% it fires most laps and no amount of graceful handling rescues
it. If M1 cannot be crossed, CTO is not viable on this sensor — and that is
worth knowing before M2–M7 are built on top of it.

**Closest evidence to date, and it is encouraging:** Toby, 2026-08-13,
`QUORUM_1_13`, 1682 markers over 64 minutes, both directions, all 171 distinct
markers crossed, `lost=0`, **zero weak events**, 8 polarity mismatches (0.48%)
all inside one 17-second direction-reversal artefact from which QUORUM recovered
unaided in four seconds. See
`field-records/20260813_TOBY_QUORUM_1_13_VERIFICATION.md`.

**Known counterexample, and it must be resolved:** Otto, 2026-08-12, CW,
NO_QUORUM near mm 120 with the true offset **outside the fence** — offsets +3
and +4 tied at 8/12, and the best fit anywhere was offset −14 at 8/12. The ring
carried six consecutive N readings where the map has no such run. That is
exactly the failure M1's crossing test exists to exclude, and its cause is
**unknown**, because the marker trail was lost to the power failure described in
§4. Same class as the 2026-08-10 incident C in
`docs/QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md`.

### M2 — IR independent speed

`firmware/test-programs/IR_SPEED_LOCAL` flew 2026-08-10. Decisions 0021 (speed
is computed locally, telemetry reports summaries) and 0022 (the LGB plastic
10-spoke wheel is the target) are settled. The spoke publishes live — 1753
messages on 2026-08-13. **The crossing test has not been run:** agreement within
10% of marker-derived speed on clean segments, sanity through a deliberate
marker glitch, zero dropouts across a full lap in full sun, and 30 minutes
without baseline drift.

M2 matters twice over: it prevents the crawl that destroys the Hall baseline
(the 2026-07-27 Grillers stall produced 55% polarity errors in that stretch),
and it is the only independent witness that can catch the magnets lying.
Position and speed presently both come from the Hall sensor, so a bad read
corrupts both at once and nothing can notice the contradiction.

### M4 — LOST ends AUTO

`enterNoQuorum()` performs a controlled stop in the AUTO chamber only, snapshots
terminal evidence before deceleration, and leaves the motor untouched in MANUAL.
That is the required shape. Not verified against the crossing test.

### M5 — trustworthy self-report

The payload exists and is broadcasting (`rear`, `front`, `env` appear in every
alert). **Whether the bound is correct has never been tested.** M5 also requires
train extent folded into the published bounds — occupied track runs from Hall
+2 markers to Hall −4 markers, roughly 0.6 m ahead and 1.2 m behind — because a
follower holding clear of a reported *position* holds clear of the power car and
drives into the last coach. Confirm whether that is in the published bound or
still owed.

### M6 — peer awareness

`docs/CTO2_HARVEST.md` (Sam, 2026-07-28) and `docs/CTO3_DESIGN_NOTES.md` exist.
The plan's **first task of M6 is an audit, not a port** — a written inventory of
`archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` with an explicit disposition
per category (port as-is / port after review / review / **rewrite** anything
consuming position without a bound / **discard** speed coordination from Hall
timing). Verify whether that inventory exists in that form; the harvest is
narrative, not a disposition table.

Settled already in `CTO3_DESIGN_NOTES.md`: a silent peer is an **unresolved
obstruction**, not empty track; separation is measured from the follower's front
bound to the leader's rear bound; `LOST_MIN_GAP_MM` is established by
measurement.

**The audit can begin at any time** — it is reading and writing, not building,
and it is the one M6 task with no dependency.

---

## 2. What changed this week, and why it helps

**The phantom is dead, and it was never a firmware defect.** Extra Hall marker
events tracked since 2026-08-10 were caused by **stacked double magnets**
installed as a remedy for markers reading weak. Replacing them with single disks
on the crosstie tops eliminated the phenomenon: Otto went from 12 weak events in
235 markers (minimum peak 39) to 0 in 214; Toby confirmed it independently with
**0 weak events in 1682 markers**, on different hardware and the opposite
`HALL_POLARITY_INVERTED` setting. Decision 0025.

This matters for CTO specifically: the phantom was injecting roughly one offset
repair per lap, and on 2026-08-10 a doubled pair produced offset −2, which the
fence cannot express, and the whole incident-C cascade followed. **A major noise
source that would have contaminated M1's crossing test is gone.**

Operator policy now: **an unexplained extra event means dig** — it is a reliable
pointer to a doubled or malformed magnet. This is an argument against adding
firmware containment, which would suppress the signal.

**Both locomotives are on the same navigator.** Otto and Toby both run
`QUORUM_1_13`, byte-identical `QUORUM.ino`. Toby's value as a pre-1.13 control
is spent.

**The HARD_BOUND advisory works in the field.** Decision 0023's exact-or-silent
advisory fired for the first time on 2026-08-12 and correctly published `null`
plus audit fields on a full ring with no unique window, rather than a guess.

---

## 3. Open issues, deliberately left open

Operator direction, 2026-08-13: issues will keep turning up and it is fine to
leave some open and discoverable later. Recorded here so they are discoverable.

1. **The CW NO_QUORUM near mm 120 (Otto, 2026-08-12).** True offset outside the
   fence, cause unknown, marker trail lost. **This is the most important open
   item for CTO** — it is an instance of the exact failure that killed CTO2.
   Otto passed mm 120 cleanly on 2026-08-13, so it is not a fixed track defect.
2. **The reversal off-by-one.** Three direction changes in 60 seconds reliably
   produced an off-by-one on Toby. It recovered because the offset landed inside
   the fence. Worth understanding before it happens where the fence cannot
   express it — and note the 2026-08-12 CW leg also began with a reversal.
3. **`ngr_runlog.py` does not treat `EROFS` as fatal.** On 2026-08-13 it logged
   a per-message error for fourteen hours while `systemctl` reported
   `active (running)` and nothing was recorded. Should exit non-zero.
4. **Decision number 0025 is used twice** — `0025-the-console-roster-is-
   discovered-not-declared` and `0025-the-phantom-was-a-maintenance-artefact`.
   The rule is sequential and never renumbered; one must become 0030.
5. **Console E-stop clear + RECOVER group.** Operator requirement 2026-08-11:
   clearing E-stop from the Dispatcher Console without leaving AUTO. Was blocked
   by the Pi; no longer is.
6. **`docs/STATUS.md` is stale** — it describes QUORUM 1.4 and dashboard
   v1.10.2, and its §9 item 1 (deploy `ngr_runlog.py`) is now done.

---

## 4. Environment — what is actually running

**Pi (`ngr-pi`, 192.168.68.142, also DHCP .73):** Debian 13 trixie, wired.
Broker on 1883, dashboard on **port 8080** (`/console`). `ngr-runlog` enabled and
active, subscribed to `ngr/#`, writing `NGR/telemetry/all_YYYYMMDD.log` plus
per-run files in `runs/`, with `# START` / `# DISCONNECT` / `# RECONNECT`
markers so silence is timestamped rather than inferred.

**A failing 5 V supply destroyed two filesystems in 36 hours** and was
misdiagnosed twice as bad SD cards. Neither card was faulty. A Geekworm
20 W / 5 V 4 A supply fixed it: `throttled` `0x50005` → `0x0`. Decision 0029 now
requires `get_throttled` to read `0x0` on a freshly booted, bare machine before
any card is called faulty. **Keep ESP32s off the Pi's USB** — neither the
dispatcher nor the runlog needs one, both are Python on the Pi.

**Nightly retrieval:** `tools/fetch_pi_telemetry.sh` via launchd at 00:00 pulls
telemetry to `~/ngr-telemetry` — deliberately outside the git working tree,
because a branch switch deleted an active capture on 2026-08-13. It records card
serial, `throttled`, mount flags and runlog state beside the data and warns on
read-only root, `throttled != 0x0`, or an inactive runlog. Check with
`tail ~/ngr-telemetry/fetch.log`.

**Locomotives:** Otto 9950011 and Toby 9950012, both `QUORUM_1_13`.
`firmware/QUORUM/LocoConfig.h` is a live per-flash selector — **read the active
`#include`, never its comments**; on 2026-08-12 the header said Toby while the
active include was Otto.

**Host replay harness:** `firmware/QUORUM/tests/` compiles the real firmware and
replays captures. It achieved 1890/1890 odometer and 40/40 decision fidelity on
the incident fixtures. **This is the tool for M1's offline alignment** and is
already built.

**Repository:** branches are tangled. Today's work is spread across
`agent/phantom-verdict-20260812`, `agent/toby-1-13-flash` and `dashboard-redesign`;
`main` is well behind. Worth consolidating before a campaign.

---

## 5. Recommended first moves

M1 is closed by operator ruling. The active work is **M2 — integrate the IR
sensor onto the locomotive.**

1. **Integrate IR with the locomotive.** Operator direction, 2026-08-13. Today
   the IR sensor is a separate spoke on a towed car publishing
   `ngr/spoke/IR_SPEED_SENSOR/#`; M2 as specified wants it on the power car,
   read by the same ESP32 that runs QUORUM, with no radio in the measurement
   path. `IR_SPEED_LOCAL 1.2` already proves that contract and exposes
   `LocalSpeedSnapshot` as the integration seam. Decision 0021 is explicit that
   *"nothing is promoted to QUORUM by this decision alone"* — promotion owes its
   own record. Known constraint: QUORUM's Hall sits on GPIO 33 with a 1 ms
   sampler task pinned to core 0, so IR needs a second ADC pin and a second
   task on the same chip.
2. **Do the CTO2 audit in parallel.** No dependency, and it is the one M6 task
   that costs nothing to have ready early. The disposition inventory, not a
   port.
3. **Carry the mm 120 CW failure as an open item** — see §3. It is an offset
   outside the fence, which is the shape that killed CTO2, so it should be
   resolved before M5 and M6 consume published bounds. It is not a blocker on
   M2.

Do not start M6 until M5 has passed. That ordering is what CTO2 violated, and
it is the one sequencing rule worth keeping strictly.

---

## References

- `docs/ROAD_TO_CTO.md` — the milestone plan and every crossing test
- `docs/CTO3/CTO3_INTENT_BASELINE.md` — design intent; `AUTHORITY_MODEL.md` — Manual/AUTO fidelity
- `docs/CTO3/station-stop-v1/` — the deliberately narrow first proof
- `docs/CTO2_HARVEST.md`, `docs/CTO3_DESIGN_NOTES.md` — what to preserve from CTO2
- `docs/decisions/` — 0023 advisory, 0024 conservation gate, 0025 phantom, 0029 supply
- `field-records/20260813_TOBY_QUORUM_1_13_VERIFICATION.md` — best M1-adjacent evidence
- `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md` — the phantom, closed
- `field-records/20260813_PI_POWER_SUPPLY_ROOT_CAUSE.md` — why recording works now

**Bicameral rule, constitutional (spec §0.2):** the Dashboard is MANUAL, the
Dispatcher Console is AUTO. E-stop crosses both.
