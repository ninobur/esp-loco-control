# 2026-08-16 — QUORUM 1.16R, second Bubble session, and the migrating magnet

**Session:** 12:18 → 15:08, Otto and Toby, CCW, both on `QUORUM_1_16R`,
supervised. Firmware identity confirmed on the broker (`state/bootid`).

## Verdict: three hours, zero NO_QUORUM — through a fault on every lap

| | agree | disagree | lost | terminals | final |
|---|---|---|---|---|---|
| Otto | 2769 | 161 (5.5%) | 0 | **0** | NORMAL |
| Toby | 2631 | 158 (5.7%) | 0 | **0** | NORMAL |

Not one terminal, while a physical fault recurred on essentially every lap.
The elevated disagreement rate was **caused by that fault**, not by the
navigator, and the navigator absorbed it and kept running.

**Quarantine fired in the field for the first time.** Otto: 20 held, 14
discarded, **6 committed**. The committed six are the reversibility path —
events held on suspicion, then vouched for by their successor and committed
back — working on real track rather than in a fixture. Toby quarantined
nothing.

## The fault: MM008's magnet migrated onto MM007

Operator inspection, after the log narrowed the search: **the magnet from
MM008 was found sitting on top of MM007's magnet.** MM008's position was
empty.

Operator hypothesis, and it fits everything: the magnet came loose, was
picked up magnetically by a passing locomotive or car, carried, and then
attracted onto the next magnet it passed near. **A loose magnet does not
merely fall out — it hitches a ride and reattaches.**

That converts one loose fastening into **two faults at two locations**:

| symptom | cause | handled by | count |
|---|---|---|---|
| odometer runs +1 from MM008 onward | the gap it left | QUORUM adoption `old_mm 159 → new_mm 158, offset +1` | 30× |
| spurious weak read 208 ms after MM007 | the stack it landed on | quarantine, `BELOW_PHYSICAL_FLOOR` | 6× |

The doubled magnet's fingerprint in the log:

```
MM008  peak 47  dur 74 ms  dt 208 ms  BELOW_PHYSICAL_FLOOR
```

208 ms is impossible travel over the route's shortest spacing, and peak 47
against a healthy 130–200 is the fringe field of the second magnet in the
stack rather than a full pass.

**Doubled magnets are phantom generators.** This is a repeat of the class the
earlier phantom investigation closed on (`agent/phantom-verdict-20260812`),
with a transport mechanism now attached to it.

## What this proves about 1.16R, precisely

On the pre-quarantine firmware these two errors would have **partially
cancelled**: a missed marker runs the odometer one behind, an admitted
phantom runs it one ahead. The fault would have presented as intermittent,
self-masking, and much harder to locate.

Quarantine's refusal to admit the phantom left a **clean, repeatable +1
count error** for QUORUM to adopt — visibly, thirty times, at the same place.
The machinery did not merely survive the fault; **it made the fault
diagnosable.** That is a stronger claim than the replay evidence could
support, and it was earned in the field.

## Diagnostic rules learned, both worth keeping

**1. A quarantine cluster at one marker means go and look at that magnet.**
Six `BELOW_PHYSICAL_FLOOR` verdicts at a single mm, with weak peaks and
sub-350 ms intervals, is the signature of a doubled magnet. This is now an
early-warning signal for a fault class that used to be found only by its
downstream damage.

**2. A count error cannot be located by its first disagreement.** An off-by-one
only *shows* where the map's polarity changes between adjacent markers; where
neighbours share a polarity it agrees by coincidence and stays invisible.

This session is the worked example. The first visible failure was at label
MM007, and the analysis concluded MM007 was the missing magnet. It was
wrong by one: label MM008 had read *clean* only because MM007 and MM008 are
both south, so the stacked pair detected at MM007's position was labelled
"8" and S matched S.

Worse, **MM007–MM010 are four consecutive south magnets**. A count error
beginning anywhere in that run stays masked until MM011, the first polarity
change. From the log alone the fault could be narrowed to "somewhere in
008–010" and no further. Inspection was required, and the operator's eyes
resolved what the telemetry structurally could not.

Any long same-polarity run on the route is a blind spot of exactly this kind.
The correct procedure on finding an off-by-one is to take the first
disagreeing label and **walk backwards through every same-polarity run** —
that whole span is the candidate set.

**3. A doubled magnet implies a missing one upwind.** The landing site is
always another magnet. Finding a stack should always trigger a search for the
gap it came from, and vice versa.

## Also confirmed

- **MM012 is correct**: 33 passes, 33 agreements, after the operator re-set
  it earlier the same day. The polarity given (north) was right.
- **Total adoptions 42**, of which 30 were the single recurring correction
  above. The remainder are ordinary.
- **0037's field evidence is in this log**: Otto and Toby paired exactly once
  (Otto LEADER, Toby FOLLOWER) and never dissolved, including through the lap
  that put Otto 12 markers *behind* the locomotive he was leading. See
  `docs/decisions/0037-a-pairing-dissolves-when-nose-tail-order-inverts.md`.

## Still unresolved: the CTO radio

**17 STALE fleet stops** (Otto 7, Toby 10) plus one NO_POSITION each, across
the session — the same failure as 2026-08-15 and still undiagnosed, because
nothing on 1.16R reports the WiFi channel. `QUORUM_1_16Rb` carries that
instrumentation and is not flashed.

The known field signature remains: **a locomotive healthy on MQTT while its
partner starves is a channel problem, not a range problem**, and a reset
clears it.

## Expected on the next run

With MM008 restored to its own position:

- MM007 returns to clean;
- the `159 → 158` adoption stops firing;
- the six MM008 quarantines disappear;
- disagreement rate should fall from ~5.5% to well under 1%.

If MM007 stays dirty, the magnet is seated but marginal and the next thing to
check is depth and lateral alignment — it read correctly in CW on the morning
of 2026-08-15 and failed in CCW, which points to an approach-dependent
detection margin rather than a dead magnet.
