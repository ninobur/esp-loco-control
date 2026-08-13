# Dashboard and console redesign — design record

Design phase only. **Nothing here is implemented.** The running app is still
`server/ngr_app_v1_10_11.py`; this directory holds the agreed layout and the
reasoning behind it, for whoever builds `ngr_app_v1_11_0.py`.

Branch: `dashboard-redesign`, cut from `main` 2026-08-12.

## Current mockup

**`mockup_v7.html`** — open it in a browser. The black MOCKUP bar at the top is
scaffolding, not part of the design: it toggles each locomotive on and off the
air and cycles Otto through the quorum and authority states, so the layout can
be judged under the states that matter rather than only the happy one.

`mockup_v1` … `v6` are the trail and are superseded. v4 is the only other one
worth opening — it holds the five-locomotive and row-per-locomotive layouts
that were explored and rejected.

Source of the layout: `Dashboard.csv` and `Console.csv`, supplied by the
operator as spreadsheet sketches (column position = grouping, row order =
vertical order).

## Console

Three commands across the top — E-STOP ALL, END AO, CIRCUIT EXPRESS — then one
column per locomotive. No `AUTO OPERATIONS` title. Two locomotives is the
design target.

Each column, top to bottom:

| Element | Notes |
|---|---|
| Name + tiny link chip | `ONLINE` / `STALE`, never `YES` / `NO` |
| Mode + role chips | `MANUAL` / `ENLISTED` / `RUNNING` / `CE`, and `LEAD` / `TRAIL` |
| BEGIN, PAUSE, E-STOP | E-STOP becomes CLEAR for a locomotive reporting E-stopped |
| QUORUM | `QUORUM` / `EVALUATING` / `NO QUORUM` / `NOT DECLARED` / `UNKNOWN` |
| `%` | rolling agreement — sits with quorum because it answers the same question |
| MM, KPH, PWM, V | label left, value right, both at the same size |

### Rules the layout depends on

**Nothing may move when a locomotive goes quiet.** The mode and role chips take
a fixed half-row each and read `UNKNOWN` rather than collapsing to a hyphen;
the link chip has a floor width; no status string may wrap. Verified across all
four authority states, plus stale, plus running solo — one identical geometry.
The reason is not neatness: the two columns are meant to be read across, and a
column that re-flows as its locomotive gets into trouble breaks exactly when it
is needed.

**A stale locomotive does not report a current-looking anything.** Quorum reads
`UNKNOWN`, not its last value. The figures are kept but dimmed — true once, not
true now.

**ENLISTED and RUNNING stay separate.** P4: `ENLISTED` is `autoEnrolled` from
`state/auto`; `RUNNING` is `autoRunning` from the 1 Hz alert. PAUSE moves
RUNNING → ENLISTED and deliberately keeps the locomotive enrolled, so a single
"AUTO" state would make a PAUSE invisible on screen.

**Columns are discovered, not declared.** No locomotive roster. A column
appears when a locomotive is heard. See the retained-message rule below.

## Dashboard

Operating controls and figures first; the once-per-session setup moved to the
bottom under a `STARTUP` rule.

Order: name + online badge, status line, motion bar, DIRECTION (`FOR` / `REV`
with a NEUTRAL indicator), THROTTLE, BRAKE, E-STOP, MM / KPH / PWM, Voltage /
Current / Power, POLARITY AGREEMENT — then STARTUP: Session Orientation, Set
Location, AUTO, Packet Log.

- **Throttle shows commanded and actual side by side**, both large:
  `148 / 255 COMMANDED` beside `146 PWM`.
- **`AUTO` only — no MANUAL button.** Manual is the default and cannot be
  chosen while auto is in force.
- **Agreement is a rolling percentage of the last ten verdicts**, not the
  session total. The session total is retained underneath in small text. Field
  case that motivated it: 4015 / 30 reads 99% while eight of the last ten
  markers disagreed.
- **Gaps in the verdict ticks are labelled** (`065 066 067 [8 silent] 076 …`).
  A gap means markers passed without a verdict — nav was in `NAV_EVALUATING` or
  `NAV_NO_QUORUM`, or position was re-declared. It is the scar of an incident
  and was previously thrown away by butting the ticks together.
- **Dropped**: CAL RECORDING (continuous runlog supersedes it), DNA
  placeholder, Low V and pKPH tiles.

## Implementation notes

1. **Discovery must not resurrect ghosts.** Wildcard subscription delivers the
   broker's whole retained backlog on connect, including locomotives switched
   off for months. **Only a non-retained message may create a column.**
   Retained messages may update a column that exists. `retained =
   bool(msg.retain)` is already computed at `ngr_app_v1_10_11.py:569`. Without
   this the console fills with ghosts on every restart — the v1.10.0 stale-tile
   and v1.10.11 zombie-LWT failure, returning by a new door.
2. **A column never disappears mid-session.** It goes stale. A locomotive
   dropping off the air must not vanish from the dispatcher.
3. **Stable column order**, not arrival order. BEGIN must be where it was
   yesterday.
4. **Names are a lookup, not a gate.** An unknown id still gets a column,
   labelled with its raw number.
5. **`/dispatcher/state` must widen.** It returns Otto and Toby only, with no
   MM / KPH / PWM / agreement / voltage. All of it is already in `loco_state`.
6. **Fix the Hans release gap.** `dispatcher_endcto()` releases Otto and Toby
   by hardcoded id but fans `stop/` across all of `LOCO_IDS`. Hans is stopped
   and never released. Loop the discovered set for both.
7. **Delete the stale INA219 comment** at `ngr_app_v1_10_11.py:1488` and the
   related note near line 221 — see corrections below.

## Parked

- **Per-locomotive END AO**, for pausing one locomotive and driving the other
  by hand. Consistent with P9 (release is a console act). Blocked on two
  questions: does an enlisted locomotive actually brake for a peer's occupancy
  bound, or merely receive it? And what does that bound mean once a
  hand-driven locomotive has lost position?
- **Bubbles.** Four locomotives in two lead/trailing pairs. Nothing publishes
  bubble assignment; that is CTO/Bubble v2.
- **Separation readout.** Removed at the operator's instruction. If it ever
  returns: **test envelope overlap before any gap arithmetic.** On a closed
  loop the two gaps sum to 171, so the smaller is the meaningful one — but once
  the envelopes intersect, both gaps run nearly the whole loop and the minimum
  is a large, reassuring, false number. Testing produced *165 markers of
  apparent clearance* for two locomotives occupying the same track. The
  overlap test is in `mockup_v7.html`, unused, for whenever that is.
- **Five locomotives.** Otto, Toby, Hans, Franz, Oscar were explored in v4.
  Franz and Oscar have no ids in the repo. Discovery makes ids unnecessary in
  advance, which is why it was adopted.

## Corrections of record

- **INA219 telemetry is live.** It was claimed during this session that no
  firmware publishes voltage. That was wrong, taken from a stale comment in the
  Flask app. QUORUM v1.7 restored it under decision 0012: `telem/voltage`,
  `telem/current`, `telem/power` every 5 s, retained. Voltage stays on both
  pages. The comment is what needs deleting.
- **RUNNING does not mean manual.** It is `autoRunning` — the locomotive under
  dispatcher control and executing. MANUAL is its own state.

## Next

Build `ngr_app_v1_11_0.py`, then a decision record in `docs/decisions/` (next
number 0018) covering the authority-display choices: discovery, the
retained-message rule, quorum outranking link, and the removal of CAL
recording.
