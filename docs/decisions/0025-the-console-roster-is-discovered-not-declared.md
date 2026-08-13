# 0025 — The console roster is discovered, and only a live message may create it

Status: **Accepted (2026-08-12).** Implemented in
`server/ngr_app_v1_11_0.py`. Not yet run against locomotives; verified
against synthetic telemetry only (`server/tests/test_ngr_app_v1_11_0.py`,
50 assertions).

## Decision

The dashboard has **no roster**. A locomotive appears on the console when it
is **heard**, not because its id was typed into the source. `LOCO_NAMES` is
a display lookup and nothing more: an id nobody has named still gets a
column, labelled with its own number.

Three clauses make that safe, and they are the decision as much as the
discovery is:

1. **Only a NON-RETAINED message may create a locomotive.** Retained
   messages may update one that already exists; they may never bring one
   into being. A `cmd/` echo — this dashboard's own voice — may not either.
2. **A column is never removed.** Nothing is pruned from `loco_state`. A
   locomotive that goes quiet keeps its column and greys.
3. **Order is stable**: known names in their listed order, then anything
   else by id. Never arrival order.

The dispatcher's fan-out sets — `BEGIN`/`PAUSE` with no target, `END AO` —
are derived from the discovered set rather than a literal tuple.

## Context

The console was built for two locomotives and had them written into it in
at least four places: `LOCO_IDS`, three copy-pasted route blocks, a
hardcoded two-column status grid, and `dispatcher_endcto()`. The operator
now expects four running at once and named five (Otto, Toby, Hans, Franz,
Oscar); two of those have no id in the repository at all.

The hardcoding had already produced a live bug. `dispatcher_endcto()`
published `dispatcher_release` to Otto and Toby by literal id, then fanned
`stop/` across all of `LOCO_IDS` — which included Hans. **END AO stopped
Hans and never released it.** Harmless while Hans had no console presence;
wrong the moment it got one. A fan-out list derived from who is actually
present cannot drift from the list of who can be commanded, so this class
of bug is closed rather than fixed.

Clause 1 is the one that matters. A wildcard subscription (`ngr/loco/+/…`)
receives the broker's entire retained backlog on connect, including every
`state/*` topic of every locomotive ever powered on. Creating a column on
"first message seen" would repopulate the console with ghosts at every
restart. That is precisely the failure v1.10.0 killed (stale tiles read as
live data) and v1.10.11 killed again (zombie last-wills flipping `online`),
arriving by a new door. The flag needed to prevent it was already being
computed — `retained = bool(msg.retain)` — and already governed rendering;
it now governs existence too.

Clause 2 is not tidiness. A locomotive dropping off the air is the one you
most need on screen; a console that quietly loses the column is worse than
one showing a greyed column with UNKNOWN in it.

## Alternatives considered

- **Extend the tuple to five.** Rejected: it needs ids for Franz and Oscar
  that do not exist, and six columns crush E-STOP ALL to a four-line button
  at phone width. It also leaves the drift between "who is listed" and "who
  is commanded" that produced the Hans bug.
- **One row per locomotive instead of columns.** Explored
  (`docs/dashboard-redesign/mockup_v4.html`) and legible at five, but the
  operator ruled for columns at two locomotives.
- **Create on any message, prune the ones that never go live.** Rejected:
  the ghost is visible in the window before it is pruned, and the whole
  point of the drop-retained rule is that a remembered value never gets
  rendered as a current one, even briefly.
- **Discover the loco PAGES too, from the same set.** Adopted — the routes
  are now one parametrised `/loco/<ref>` family. Keeping the console
  dynamic while the pages stayed hardcoded would have given a discovered
  locomotive a nav button pointing at a 404.

## Consequences

- Franz, Oscar or anything else appears by being switched on. Naming it in
  `LOCO_NAMES` only buys a nicer label and a friendlier URL.
- Three copy-pasted route blocks collapse to one parametrised set. `/otto`,
  `/toby` and `/hans` still work, as redirects.
- A locomotive that has never been heard but IS named answers with a blank
  state rather than a 404, so its page can say "silent this session" —
  the same blank-until-proven answer P8 gives.
- ~~The console shows nothing at all until something speaks.~~ **Amended
  2026-08-12, same day, on the operator's objection.** A blank console until
  you switch a locomotive on is a real regression from a console that always
  drew Otto and Toby greyed — it reads as broken, and leaves nowhere to
  reach for a locomotive you are about to power up. A locomotive named in
  `LOCO_NAMES` now always has a column; discovery is unchanged for anything
  unnamed. Naming means "I expect to see this one", not "only these may
  appear". A listed-but-unheard locomotive renders from `_fresh_state()` and
  is never written into `loco_state`, so being listed is not being heard
  anywhere else in the app, and the retained backlog still creates nothing.
- The commandable set is the set on screen, for the per-locomotive buttons
  and the fan-outs alike — including a column that has not been heard, since
  END AO must not miss a locomotive that is enlisted but momentarily between
  telemetry.
- `/dispatcher/state` returns a list rather than `otto_*`/`toby_*` keys, and
  carries MM, KPH, PWM, voltage and rolling agreement per locomotive.
- **No authority rule changes.** P2–P14, the drop-retained rule, the P8
  provisional seed, the never-fabricate rule (v1.10.9) and the E-STOP
  ordering (v1.10.8) are carried over intact. This record covers who may
  appear on the console, not who may command what.

## References

- `server/ngr_app_v1_11_0.py` — the discovery gate is in `on_mqtt_message()`
  and `_ensure_loco()`.
- `server/tests/test_ngr_app_v1_11_0.py` — the gate is tested three ways
  (retained, `cmd/` echo, live) plus order stability and the release set.
- `docs/dashboard-redesign/README.md` — the full layout design record.
- Decision 0012 — INA219 restored; v1.10.11 carried a stale comment claiming
  no firmware published `telem/voltage`, which was wrong and is now deleted.
- `firmware/QUORUM/QUORUM.ino` `acceptEvent()` — why markers pass without a
  verdict, which is what the "N silent" gaps in the agreement row report.
