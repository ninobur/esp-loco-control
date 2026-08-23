# hostnav — the replacement autonomous-acquisition host navigator

Host model of `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`, written against
the acceptance harness frozen at `db9bb54` *before* it existed (decision 0042).

It runs entirely off-locomotive. It authorises no firmware change, no flash
and no train run, and it touches no production firmware, no QUORUM code, no
MQTT/CTO behaviour and no station firmware.

## Module and factory

```
tools.navlab.hostnav.navigator:Navigator
```

Zero-argument factory satisfying `tools/navlab/acceptance/navapi.py`:
`start(mode, policy, mm=None, direction=None)`, `observe(detection)`,
`peer_report(report)`, `operator(command, **kw)`, `status()`.

The navigator lives **outside** `tools/navlab/acceptance/`, which stays the
independent frozen test harness.

## Exact acceptance command

```bash
NGR_NAVIGATOR=tools.navlab.hostnav.navigator:Navigator python3 -m tools.navlab.acceptance.run_acceptance --require-navigator
```

Run from the repository root; Python 3 standard library only. Result of
record, and the JSON behind it:

| status | count |
|---|---|
| `PASS` | **49** |
| `FAIL` | **0** |
| `NOT_IMPLEMENTED` | **0** |
| `NOT_DEMONSTRATED` | **3** — `N1`, `N2`, `N3` |

`tools/navlab/results/hostnav_generated_acceptance.json`

Unit tests for the internals:

```bash
python3 -m unittest tools.navlab.hostnav.test_hostnav
```

## Architecture

| file | role |
|---|---|
| `route.py` | the committed map parsed read-only from `firmware/QUORUM/QUORUM.ino`; geometry; `W_dir`/`W_both` **computed** over the map (§3.3), and the unique-window lookups |
| `params.py` | engineering parameters (§10). Every one uncalibrated |
| `envelopes.py` | robust timing-envelope interface and the PWM ring; the seam the calibrated §3.8 table plugs into |
| `branches.py` | propagation (§3.5.1), branch-local elapsed time (§3.6), the branch list (§3.10) |
| `occupancy.py` | conservative occupancy and arcs (§3.12), peer bounds (§3.12.2), decision-0033 separation |
| `navigator.py` | the four states, confirmation (§3.11), movement authority (§7), telemetry |

**The hypothesis space** is `(marker, travel direction)` over the whole route —
171 × 2 = 342 states. Growth is bounded by construction, so `INCOMPLETE` is
never raised by memory pressure on `H`. The branch list is the one capped
structure; overflow collapses it to its union and costs confirmation authority
only, never hypotheses.

**Two lanes over one evidence stream.** The navigator propagates the same
detections through two branch lists:

- the **track** lane preserves travel direction and carries the navigation
  claim — it is what `nav_state`, the published position and confirmation are
  computed from;
- the **safety** lane additionally admits a native reversal at every
  detection.

A native reversal carries no signature in the evidence record of §3.2 — the
detection has polarity, peak, duration, times and a PWM profile, and no motor
direction — so wherever travel direction is not held by a live operator
declaration it cannot be ruled out. The published set is the union of the two
lanes, which is why the true `(marker, direction)` stays inside a `COMPLETE`
set across a reversal the record cannot show. When the track chain dies and
the reversal-admitting set does not, the track set is replaced by it: that is
"hypotheses preserved, travel direction reversed", and the evidence window is
dropped at the same moment because a polarity string straddling a reversal is
not a route string and must never read as unique.

**Timing is branch-local.** Each branch carries its own `last_genuine` and its
own clock epoch; `elapsed(e, b) = e.t_detect − b.last_genuine`, or `UNKNOWN`
on an epoch mismatch. The genuine side of a fork advances the origin, the
phantom side does not, so no interval is folded into a successor and none is
counted twice. No `dt` is read from any event, because the harness's
`Detection` has none and elapsed is not a property of an event.

**Confirmation is §3.11 uniqueness only**, at the computed `W_dir` where a
live operator direction declaration holds and at `W_both` otherwise, gated on
all four conditions: `COMPLETE`, authority not suspended, a genuinely unique
window with no pending branch and no admitted missed marker inside it, and the
matched position inside `H`. The collapse path the specification also permits
is deliberately **not** used as a confirmation source — it is the weaker of
the two, and uniqueness alone meets every usefulness gate on the generated
families.

**Strategy A** for a Case-I discontinuity: the pre-discontinuity hypotheses
are retained, the affected detection propagates with `d_lo = 0` and
`d_hi = ∞`, the chain is marked gap-bearing and confirmation authority is
suspended until uniqueness clears it. A gap-bearing set that reaches
route-wide extent exits to `UNLOCATED`, which is strategy B as the floor.
Case R — an internal firmware redeclaration — is not a timing event at all: it
carries only a label, and labels are not evidence.

**Movement authority is derived, never fixed.** Entering a recovery state
commands no speed change. The ceiling falls only when a peer's conservative
occupancy, an armed station or a configured protected region actually binds,
with the §7.4.1 hysteresis so a single doubtful detection produces no
speed-state transition. `STOPPED_FOR_NAVIGATION_SAFETY` is the only
navigation-commanded motion order; there is no `HOLD` state for an absence.

## Design decisions worth disagreeing with

These are choices, not deductions. Each is the conservative reading of a
clause that admits more than one.

1. **`d_lo` is zero.** Nothing in the record excludes the locomotive having
   been slower than nominal inside an interval the navigator samples only at
   its ends, so any positive lower bound derived from nominal speed is an
   under-approximation and can exclude the truth. Standstill is handled from
   the PWM profile instead: the same-marker candidate is admitted only where
   the profile leaves standstill possible, which is what keeps a moving
   locomotive from accumulating stall hypotheses without ever excluding a
   genuinely slow interval.
2. **C1 (alone) is never established here.** §4.2 requires absence of a peer
   to come from the decision-0031 membership rules and forbids inferring it
   from silence. The navigator contract carries no membership channel, so on
   generated evidence the only context that authorises acquisition motion is
   C2. An explicit membership statement (`operator('declare_alone')`) is
   wired in for when one exists.
3. **A contradiction latches a fault and a stop.** §7.5 names a contradiction
   as one of the four cases that require an operator restart, and §7.6
   requires deliberate restart after investigation. The navigator keeps
   observing, reasoning and publishing — it does not re-anchor itself out of a
   falsified model unasked. `operator('restart')` clears it.
4. **The first detection after an anchor with no timing origin** is
   propagated as at most one interval rather than as unknown time. An exact
   declaration is made with the locomotive deliberately stationary at the
   declared marker (§4.0 mode 1, §7.5), and a launch-region or route-wide
   seed is a statement about where it is standing. Missing that first marker
   is not special-cased away: it produces a polarity mismatch and relocates
   through the ordinary branch mechanism.
5. **`MODE_UNKNOWN` with a direction is `ACQUIRING_ORIENTED`**, not
   `UNLOCATED`: an orientation declaration with no position is exactly §4.2's
   entry condition, and P10 restricts the search to the declared plane.
   `MODE_UNKNOWN` with nothing declared is `UNLOCATED` over all 342 bits —
   the launch region is never presumed.

## Known unvalidated assumptions

Nothing here is calibrated, tuned or field-validated. `N2` stays
NOT_DEMONSTRATED for precisely this list, and §10 blocks candidate freeze
until each item carries committed calibration evidence.

- **`params.NOMINAL_SPEED_MM_PER_MS`** is a nominal PWM→speed
  characterisation standing in for the versioned envelope table of §3.8,
  which does not exist. Every distance window, and therefore every acceptance
  conclusion that depends on one, depends on this table.
- **`SPEED_BAND_HI = 1.30`** is the only thing keeping the true distance
  inside `d_hi`. It has no measured basis. Too small and the completeness
  invariant is lost; too large and acquisition slows.
- **`PEAK_FLOOR = 80`, `DUR_FLOOR = 100`** separate the generated genuine and
  ghost distributions cleanly. Real detector separation, and the false-hold
  and false-pass rates, are unmeasured.
- **`PENDING_DEPTH_MAX`, `BRANCH_MAX`, `COLLAPSE_MAX_SET`, `K_CONFIRM`,
  `MARGINAL_SLACK`, `RECOVER_WINDOW_*`, the hysteresis constants and
  `SPEED_STEP_MIN_PCT`** are the specification's recommended defaults. The
  generated families do not exercise most of them near their limits, so they
  are untested as much as uncalibrated.
- **`V_PEER_MAX_MM_PER_MS` and `CTO_PEER_STALE_MS`** govern how fast a silent
  peer's occupancy expands. No peer envelope has been measured.
- **No protected-region mechanism exists** (open operator decision 4, `N3`),
  so §4.2 C2 is reachable in this model only because the harness supplies
  `PEER_BOUNDED` directly.
- **No untouched capture has been spent on this** (`N1`). Passing generated
  cases is not validation, and every existing Toby and Otto session is
  permanently development data.

## Operator policy remains configurable

Open decisions stay in `navapi.Policy` and are read, not assumed: fleet
stop/yield on unbounded occupancy, station-final behaviour on entry to
`RECOVERING`, the orientation and launch-region command semantics, the
protected-region mechanism, the occupancy publication form and arc budget, and
the Strategy A/B choice.

Rulings already closed are **not** configurable anywhere in this package: the
launch region is MM036–MM045 and is never presumed, self-acquisition is
optional, exact-MM startup enters `POSITIONED` immediately, manual operation
without a position remains possible, there is no launch-ordering state, there
is no routine operator position confirmation and no routine post-recovery GO,
STOP/HOLD are reluctant, and the first-station references are Grillers CW and
Patio CCW.
