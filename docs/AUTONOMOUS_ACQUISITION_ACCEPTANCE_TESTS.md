# Host-model acceptance tests — written before implementation

Companion to `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`. Status: Proposed.
These tests are to be written and committed **before** the navigator they
test. They run entirely off-locomotive.

## Ground rules

- **Generated cases with known ground truth.** Every case is synthesised on
  the real committed map (171 markers, the committed spacing table) with the
  true position and direction known by construction. No case's verdict depends
  on a firmware label, a heuristic classification, or an existing log.
- **Existing Toby and Otto sessions are development data.** They may be
  replayed for regression and for realism of the generated event
  distributions. A replay result is never an acceptance verdict, and no test
  below passes on replay evidence.
- **The global invariant outranks every individual test.** A case that
  produces a false confirmation fails the suite regardless of any other
  outcome.

## G — the global invariant

**G1. No false confirmation under any generated case.** Across every case in
every family below, and across a randomised sweep of at least 10,000 generated
sessions, the navigator never enters `POSITIONED` at a position or direction
other than the true one. Enforced as an assertion inside the harness, checked
at every confirmation, not as a summary statistic.

**G2. No under-approximation.** At every event, the true (position, direction)
is a member of `H` whenever `H` is marked `COMPLETE`. This is the property the
dt=0 one-interval grant violated, and it is checked continuously rather than at
confirmations only.

**G3. Stopping is always available.** No case reaches a state in which the
navigator has no safe action but to demand an operator declaration.

## T1 — orientation-known startup at every possible MM

Enter `ACQUIRING_ORIENTED` with the declared direction correct and the true
start position swept over **all 171 markers, in both directions (342 cases)**.
Clean marker stream.

- Pass: every case reaches `POSITIONED` at the true marker, or stops without
  confirming. No case confirms a wrong marker.
- Recorded, not gated: the distribution of events-to-acquisition. The design
  predicts ~10–14; a case needing materially more is a finding about the map,
  not a failure.

## T2 — orientation-unknown startup

`UNLOCATED` from all 342 (marker, direction) starts, clean stream.

- Pass: every case either resolves to the true (marker, direction) or stops.
- Additional check: orientation resolves no later than position in every case
  (§4.4 predicts it usually resolves earlier); a case where a position is
  confirmed while direction is still ambiguous is a defect.

## T3 — normal loss and recovery

From `POSITIONED`, inject an ambiguity that forces `RECOVERING` (a pending
overflow, then a clean stream). Swept over all 171 start markers × both
directions × loss at 1, 3 and 8 markers of travel.

- Pass: recovery to the true marker or a stop. `H` must contain the truth at
  every event (G2). No case may reach route-wide extent on a clean stream —
  that would reproduce the iteration-3 corridor defect.

## T4 — reversal during acquisition

Motor direction flips mid-acquisition, in `ACQUIRING_ORIENTED` and in
`RECOVERING`, at 1–10 events after entry.

- Pass: hypotheses are preserved and their travel direction reversed; the true
  position stays in `H`; no confirmation from a sequence that straddles the
  reversal unless it is unique in the actual travelled path.
- Explicit sub-case: reversal *between* the two halves of a would-be unique
  12-window. The window must not be treated as unique.

## T5 — dt=0 after operator declaration (Case D)

A declaration mid-run producing `dt == 0` on the next event, swept over all 171
markers, both directions, at PWM buckets 40/60/90/100.

- Pass: the navigator remains `POSITIONED`, `H = {declared}`, no corridor
  grant, no gap-bearing flag, and the following events track normally. A
  transition to `RECOVERING` here is a failure — it is the operational cost
  Strategy B would pay and Case D exists to avoid.

## T6 — dt=0 after internal redeclaration (Case I)

The direct successor to `probe_dt0_unknown_time.py`, and the test the frozen
navigator fails. Sweep all 171 start positions × both directions × true elapsed
gaps of **0, 1, 2, 3, 4, 6, 10 and 20 intervals** during the unknown time.

- Pass: **zero confirmed-wrong outcomes at every gap size**, including gaps far
  outside any assumed domain. The frozen navigator produces 7–25 wrong
  confirmations per 171 at gaps ≥ 2; the replacement must produce none at any
  gap.
- Pass: G2 holds at every gap size — the truth stays in `H`.
- Pass: the chain is marked gap-bearing and confirmation authority is suspended
  until §3.9 uniqueness clears it.
- Recorded: how many events uniqueness took, per gap size. This is the
  operational cost of Strategy A and it is measured, not assumed.
- Control: the no-reset run must be correct 171/171, as it is today, so any
  failure is attributable to the reset handling.

## T7 — missed markers during unknown time

Generated streams in which 1–15 consecutive markers produce no event, with and
without an accompanying dt reset, at every start position.

- Pass: G2 — the true position remains in `H` throughout. A missed run must
  widen `H`, never shift it off the truth.
- Pass: no confirmation from a 12-window that contains an admitted skip
  (§3.9-3-uniqueness), since the observed string is then not the route string.

## T8 — same-magnet rereads

A stationary or crawling locomotive re-reading one magnet, 2–20 times, at every
marker, with and without a dt reset straddling the run.

- Pass: no forward advance is claimed. The stay hypothesis survives.
- Pass: a reread run does not produce a confirmation by repetition — repeated
  identical evidence is one observation, not `K_CONFIRM` of them.
- Pass: polarity alternation within a run is treated as evidence of *motion*
  (a stationary re-read cannot alternate pole), and the stay hypothesis dies.

## T9 — genuine acceleration

Streams where dt shortens sharply between consecutive genuine markers, at every
PWM bucket, including transitions 40→90 and 60→99 within one interval.

- Pass: genuine events stay genuine. No genuine acceleration event is vetoed
  outright by the timing envelope; at worst it becomes pending and its
  successor restores it.
- Pass: no frozen run of consecutive rejections — the failure mode C6 was
  written for — appears in any generated case.

## T10 — weak short-duration ghosts

Single ghost events with peak and duration below the §3.7 floors, injected
between genuine markers at every position and at each PWM bucket. Includes the
exact signature that stopped the boot1 replay: peak 44, duration 42 ms, 83 ms
after a genuine event at the same marker.

- Pass: the ghost does not advance position, and does not stop the run either.
  It opens a pending branch; the successor resolves it to phantom; tracking
  continues.
- Pass: the ghost is not deleted — it is present in the published evidence with
  its classification.

## T11 — repeated ghosts

Ghost families of 2–6 consecutive events, at slow and low-PWM stretches where
they actually occur.

- Pass: no position advance from the family.
- Pass: at `PENDING_DEPTH_MAX` overflow the branch list collapses to its union,
  `INCOMPLETE` is raised, confirmation authority is suspended, and **no branch
  is discarded**.
- Pass: recovery from the family once genuine markers resume, without an
  operator declaration.

## T12 — ambiguous DNA sequences

Cases constructed on the real map where the observed polarity string is
route-ambiguous: W = 9 windows (which collide four ways by map fact), and
aliased sub-sequences inside otherwise unique windows.

- Pass: no confirmation while more than one route position matches.
- Pass: a collapse to one hypothesis inside an aliased corridor does **not**
  confirm when the set exceeded `COLLAPSE_MAX_SET` since the anchor — the
  aliasing pathway P11 forbids.

## T13 — route-wide reacquisition

From `UNLOCATED` and from `RECOVERING`-degraded-to-route-wide, with clean
streams, with one missed marker inside the acquisition window, and with a ghost
inside it.

- Pass: acquisition only on a genuinely unique sequence; silence otherwise.
- Pass: the clean case acquires the true position; the missed-marker and ghost
  cases either acquire the true position or acquire nothing. Neither may
  silently confirm an aliased position.

## T14 — two-locomotive safety while one position is uncertain

Two simulated locomotives, one `POSITIONED` and one in each of
`ACQUIRING_ORIENTED`, `RECOVERING` and `UNLOCATED`, swept over relative
separations from adjacent to half a circuit.

- Pass: the worst-case-pair separation test (§3.11) never authorises motion
  that could bring the two within the 0033 bubble under any member of either
  hypothesis set.
- Pass: an `INCOMPLETE` or route-wide span produces a fleet stop.
- Pass: the uncertain locomotive yields; a `POSITIONED` peer keeps authority up
  to the separation bound.
- Pass (G3): in every case where motion cannot be authorised, both locomotives
  stop and neither publishes a demand for a manual MM declaration.

## Regression, explicitly not acceptance

Replays of the existing Otto and Toby sessions are run and reported alongside,
for three purposes only: that the navigator processes a full session without
crashing, that its behaviour is explicable event by event, and that the
operational cost of Strategy A on a session with 21 dt-chain resets is
measured. Under Strategy A the eight Otto boot16 route-wide recoveries are
expected **not** to reappear as confirmations. A replay that produced more
confirmations than the frozen navigator would be a reason for suspicion, not a
result.
