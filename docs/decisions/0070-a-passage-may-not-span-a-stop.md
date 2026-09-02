# 0070 — A passage may not span a stop

**Status:** PROPOSED. Not ratified. **No firmware has been modified.**
**Date:** 2026-09-01
**Supersedes nothing. Touches:** 0057 (no duration ceiling), 0064, 0065.
**Evidence:** findings 11 and 13; replay harness `tools/interrupted_replay/`.

---

## The operator's requirement, verbatim

> My intent is not to relax Gaussian recognition for slow movement generally.
> Normal moving passages continue to receive the complete recognizer at every
> PWM. The only special case is a Hall passage physically interrupted because
> Toby stops while the passage is open. In that specific case, Gaussian shape is
> unavailable because the traversal is incomplete. Evaluate only the Hall
> evidence collected while the field was changing before the stop. Do not
> include stationary dwell samples. If that pre-stop evidence confidently
> establishes a magnet, advance exactly once and suppress all further counting
> from that same occupied field until it clears after departure. If the pre-stop
> evidence is insufficient, advance zero. Preserve a pending state so departure
> Hall evidence may establish the magnet, still with at most one advance for the
> entire stop-and-departure episode.
>
> Station identity, route expectation, commanded PWM, and the fact that a stop
> was requested may select the evaluation mode but may not create a magnet or
> authorize an advance. Hall evidence remains the sole ticket.

---

## The problem

A passage runs from a threshold crossing to a threshold return. That model
assumes the sensor is *moving past* the magnet. When the locomotive stops
mid-field the model has no way to end, and everything before the stop merges
with everything after it into one object which is then fitted to a Gaussian and
thrown away.

Twice on 2026-09-01, twenty-one minutes apart, at two platforms:

| | Grillers CCW (finding 11) | Arches CCW (finding 13) |
|---|---|---|
| the real magnet is | at the HEAD, on arrival | at the TAIL, on departure |
| plateau | 163 counts, on the magnet | 41 counts, a fringe |
| open for | 37,733 ms | 36,689 ms |
| residual | 0.2773 | 0.3825 |
| outcome | MM59 lost, strike | MM106 lost, strike |

A marker can be lost at either end. Stop-offset tuning relocates the landing; it
cannot guarantee the landing is clear of every fringe field, and both incidents
happened with the offsets doing exactly what they were set to do.

---

## The decision

### 1. A stop is detected from the Hall signal alone

The field has settled when it stays within **±8 counts** of a reference for
**400 ms**. No PWM, no station state, no route expectation participates — not
even in selecting the mode. This is stronger than the requirement, which permits
those to select the mode; it costs nothing to refuse them entirely.

### 2. States

```
TRAVERSING          normal. Passages open and close by the existing rules and
                    receive the COMPLETE recognizer at every speed. Unchanged.
      |  passage open AND field settled
      v
INTERRUPTED         judge the pre-stop segment -- the samples from the passage
                    opening to the START of the settle window. Stationary dwell
                    samples are excluded.
      |                                        |
      | evidence sufficient                    | insufficient
      v                                        v
OCCUPIED_COUNTED                          OCCUPIED_PENDING
  advance exactly once                      advance zero
  suppress everything until                 judge the departure segment when
  the field clears                          the field clears; at most one
      |                                        |  advance for the episode
      +------------ field clears --------------+
                        v
                   TRAVERSING
```

### 3. The interrupted-traversal rule

Applied to a segment collected **while the field was changing**. Shape is not
tested, because a half traversal is not evidence either way — the identical
reasoning the recognizer already applies to truncated and clipped curves.

| test | threshold | why |
|---|---|---|
| entry into the field | segment peak exceeds its own starting level by ≥ `entryMargin` (38) | the operator's "do not count a stationary offset that did not have a changing pre-stop entry". This is what rejects a DC offset, which has no entry. |
| amplitude | ≥ `amplitudeFloor` (0.34) | unchanged |
| single polarity | no opposite-sign excursion above `exitMargin` (25) | "no contradictory excursion" |
| guard | ≥ `guardMs` (200) since the last acceptance | unchanged |
| no clipping | | unchanged |

Verdict published with reason `INTERRUPTED_TRAVERSAL` and `shapeTested: 0`.

### 4. What does not change

Thresholds, baseline behaviour, station offsets, the normal moving recognizer,
decisions 0064 and 0065, the waveform dump, and the navigation contract. An
interrupted segment never enters the moving-passage gain median, so a partial or
stationary peak cannot drift later amplitude ratios.

---

## Replay results

`tools/interrupted_replay/`. Real captures are reconstructed from the decoded
waveform CSVs; synthetic cases are generated Gaussians.

### The real captures

```
finding 09 A  DC offset, stationary throughout ....... 0 advances (wanted 0)  PASS
finding 09 B  DC offset held 9m15s ................... 0 advances (wanted 0)  PASS
finding 11    parked ON MM59 ......................... 1 advance  (wanted 1)  PASS
              pre-stop: start 65 peak 192 growth 127 ratio 0.941 -> ACCEPT
              departure: SUPPRESSED, this field already counted
finding 13    parked in a fringe, MM106 on departure .. 1 advance  (wanted 1)  PASS
              pre-stop: no changing field to judge     -> PENDING
              departure: start 39 peak 189 growth 150 ratio 0.887 -> ACCEPT
```

### Stops at every point on the arc — peak 220, gain 213, 1400 ms crossing

```
stop on the RISING edge at 15% (33 counts, below the entry margin) .. 1  PASS
stop on the RISING edge at 25% (55 counts) ......................... 1  PASS
stop on the RISING edge at 40% (88 counts) ......................... 1  PASS
stop at the PEAK ................................................... 1  PASS
stop on the FALLING edge at 40% .................................... 1  PASS
stop on the FALLING edge at 15% .................................... 1  PASS
parked in a 41-count fringe, real magnet on departure .............. 1  PASS
stationary DC offset, no entry, decays away ........................ 0  PASS
clean crossing then a stop clear of every magnet ................... 1  PASS
```

**13 of 13.** Note the 15%-rising case: the field never reaches the entry margin,
so no passage is open, no interruption occurs, and the departure is judged by the
UNCHANGED recognizer — residual 0.0801. The missing foot of the arc lies below
the 20%-of-peak fit window, so nothing is lost.

The count moves between the pre-stop and the departure as the stopping point
crosses the amplitude floor — 25% counts on departure, 40% counts at the stop —
but it is exactly one in every case. Where `navMm` updates changes; the marker
sequence does not.

### Slow crossings with NO stop

The settle detector could in principle mistake the apex of a very slow arc for a
stop. Measured against the ±8-count band:

```
  700 ms crossing   apex moves 147.0 counts in 400 ms   cannot settle    1 PASS
 1400 ms            53.0                                cannot settle    1 PASS
 2500 ms            18.2                                cannot settle    1 PASS
 4000 ms             7.3                                CAN settle       1 PASS
 6000 ms             3.3                                CAN settle       1 PASS
 9000 ms             1.5                                CAN settle       1 PASS
```

**Above about four seconds the apex does look flat and the settle fires
spuriously — and the answer is still exactly one advance**, because the rising
half alone establishes the magnet and the falling half is then suppressed. The
settle constants are therefore a tuning parameter, not a correctness-critical
one. A false settle costs the shape test on that one passage; it cannot cause a
miscount.

---

## What this proposal does not prove

- **Real noise against the ±8 band is untested.** The reconstructions are built
  from decimated records and are smoother than a live 1 kHz line. If real noise
  exceeds ±8 counts the settle never fires and behaviour reverts to today's,
  which is a safe failure but not a fixed one. This wants a bench measurement of
  the stationary noise floor before the constants are frozen.
- **Finding 09 B is rejected by the polarity rule, not the entry rule.** Its
  record is decimated 2048:1, the transition falls inside one sample, and the
  reconstruction is coarse. The verdict is right; the reasoning path is an
  artefact of the reconstruction and should not be read as evidence for the
  entry test.
- **Finding 13 emits one extra `NOT_A_MAGNET`** as the fringe passage clears and
  the machine cycles. Harmless to the count, visible in telemetry.
- **A magnet entered slowly from a pre-existing offset** — where the growth from
  the offset to the peak is under 38 counts — is not covered by any capture.

---

## Ratification

Nothing here is authoritative. The operator's instruction was explicit: *"Do not
modify firmware until I ratify that design."* No firmware file has been touched.
