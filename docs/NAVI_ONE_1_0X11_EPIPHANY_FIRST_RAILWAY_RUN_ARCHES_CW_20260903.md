# NAVI_ONE 1.0 X11 "Epiphany" — first railway run, Lowline CW, 2026-09-03

**Status:** field verdict, written from the telemetry mirror and the operator's preserved copy of the run
([field_runs/20260903_x11_arches_cw_shutdown/9950012_20260903_094727.log](/Users/davidbrown/NGR/NGR-Files/field_runs/20260903_x11_arches_cw_shutdown/9950012_20260903_094727.log)).
Nothing in this document is a decision. The four questions it raises were handed to an independent
code trace whose findings are appended in the last section as they were verified.

## Verdict in four lines

1. **Acquisition held.** 86 magnets accepted, 0 disagreements, `floor_rej` 0 for the whole run. Yesterday's
   X9 railway run closed 4613 passages under the 40 ms floor for 1123 accepted magnets, about four per magnet.
   X11 closed none in 86. The bench result (0 bad reads in 5113) carried to the railway.
2. **The archaeology worked in the field, first time.** Toby came to rest on MM109 for the Arches dwell. The
   passage was paused 41.255 s, stitched on departure, judged two-sided on the trunk (`trunk 2`,
   `why2 "one arc, restored"`), residual 0.0128, and MM109 advanced. Yesterday the same geography produced
   STITCHED_REFUSED at 0.1967, 0.1818 and 0.181.
3. **The strike was on a record with its rising flank cut off, not on a knife-edge fit.** The refused passage
   toward MM111 shows an 80-count step between the last pre-roll sample (37) and the first retained sample (117).
   About 25 ms of rise is missing. The accepted MM110 before it has the same defect, smaller (38-count step,
   residual 0.1139). MM111 in CW normally judges at 0.072 to 0.079 over 21 prior passages, including five
   earlier Arches CW departures at PWM 90 where it was the first magnet out of the station.
4. **The stop itself was by design, the label was not.** A refusal on a passage that opened while the stop
   machinery was armed is routed to the stitched-refusal handler on purpose (Bamboo 12:29 rule). The event
   name `STITCHED_REFUSED` and the withdraw text ("paused by a stop, resumed on Hall morphology, stitched")
   describe a passage that was never paused or stitched.

## The run

| Time (local) | Event |
|---|---|
| 09:44:08 | Boot, `NAVI_ONE_1_0X11_FIELDTEST` "Epiphany", entry 38, exit 25, floor 40 ms, resid ceiling 0.13 |
| 09:47:27 | Declared at MM24, CW |
| 09:47:37 | First magnet, MM25, agreed |
| 09:48:37 | Grillers: ZERO_RAMP at MM62 (off −1); dwell 09:48:49 to 09:49:19; DEPART PWM 110; DEPARTED 09:49:27 at MM65 |
| 09:49:26 to 09:49:27 | `discards` 0 → 14 → 20 during the Grillers departure, moving, PWM 110 |
| 09:50:27.892 | MM108 (Arches) agreed, resid 0.0706 |
| 09:50:27.911 | Arches: ZERO_RAMP at MM108 (off 0); DWELL_BEGIN 09:50:39; DEPART 09:51:09 PWM 90 |
| 09:51:13.897 | MM109 agreed: stitched 1, paused 41255 ms, two_sided 1, trunk 2, "one arc, restored", resid 0.0128, gap 3443 ms |
| 09:51:16.419 | MM110 agreed: resid 0.1139, ratio 0.839, gap 2385 ms, 241 samples, 228 ms |
| 09:51:16 to 09:51:18 | `discards` 20 → 47 → 75 |
| 09:51:18.137 | Passage toward MM111 refused: NOT_A_MAGNET / WRONG_SHAPE, peak 218, ratio 1.019, resid 0.1301, gap 1572 ms, 154 samples, 141 ms, stitched 0, paused 0 |
| 09:51:18.196 | STITCHED_REFUSED / INTERRUPTION_UNRESOLVED, nav STRUCK, auto 0, running 0. Toby stopped. No DEPARTED event had fired for Arches. |

Final counters: agree 86, disagree 1, notmag 1, floor_rej 0, discards 75, baseline 1827, pub_drop 0.

Accepted residuals for the run, excluding the two post-Arches passages: minimum 0.0128 (the stitched one),
median 0.070, 90th percentile 0.078, maximum 0.0853 (MM64, first magnet after the Grillers departure).

## The refused record is amputated

All six records published at the withdraw were decoded (header `<8B6H2f3I`, then int16 samples, 12-sample
pre-roll first). The pre-roll is filled only while no passage is open; on opening, the record is the pre-roll
followed by the samples from the entry sample onward, so a healthy record continues from the pre-roll without
a break.

| Record | Samples | Pre-roll ends | First retained | Step | Rise slope after | Missing rise | Residual |
|---|---|---|---|---|---|---|---|
| MM106 | 261 | 37 | 39 | 2 | 0.8/ms | none | 0.0648 |
| MM107 | 269 | 36 | 38 | 2 | 0.7/ms | none | 0.0632 |
| MM108 | 292 | 36 | 39 | 3 | 0.6/ms | none | 0.0706 |
| MM109 stitched (dec 4) | 501 | 32 | 38 | 6 | 0.4/sample | none; a 21-count step at sample 157 is the stitch join | 0.0128 |
| MM110 | 241 | 34 | 72 | **38** | 1.8/ms | ~21 ms | 0.1139 |
| MM111 refused | 154 | 37 | 117 | **80** | 3.2/ms | ~25 ms | **0.1301** |

The three healthy records and the stitched one resume at 38 or 39, which is the entry margin: the first
retained sample is the entry sample. The two post-departure records resume at 72 and 117. No magnet at
departure speed rises 80 counts in a millisecond; the healthy flanks run under one count per millisecond.
The missing samples are the start of the passage. There are no single-sample deviations over 20 counts from
the local median anywhere in the six records; X11's median is not implicated.

## The family says these two magnets are not marginal

CW passages before today, from the mirror (accepted only):

| Magnet | n | Median resid | Max resid |
|---|---|---|---|
| MM109 | 22 | 0.0596 | 0.0842 |
| MM110 | 20 | 0.0868 | 0.1223 (a stitched passage on 2026-09-02 11:03); 0.1007 unstitched |
| MM111 | 21 | 0.0753 | 0.0790 |

Five earlier Arches CW departures (2026-09-01 16:45 to 18:16, PWM 90) each met MM111 as the first magnet after
the dwell and judged it at 0.0731, 0.0753, 0.0724, 0.0767, 0.0763. Departure acceleration by itself does
not raise MM111's residual. On those runs the stop lay past MM110; today it lay at MM108 with the rest on
MM109, so today's departure crossed MM110 and MM111 while still inside the departure phase.

Yesterday's X9 run, CCW, 18:34 to 20:17: 1602 accepted, residual median 0.0701, 99th percentile 0.0892,
maximum 0.1046, none at or above 0.11. First magnets after 36 station departures ran 0.070 to 0.101.

## The discards

`discards` counts `HallCapture::discard()`: a passage "un-opened" because the field plateaued below the
entry margin with the stop machinery armed ([HallCapture.h:862](../firmware/test-programs/NAVI_ONE/HallCapture.h)).
Departing arms it too ("for the locomotive that stalls halfway out of a magnet",
[HallCapture.h:331](../firmware/test-programs/NAVI_ONE/HallCapture.h)). `discard()` resets the record
pointer but does not refill the pre-roll ring, which is only fed while no passage is open.

Today: 20 discards inside the Grillers departure, 55 inside the two seconds that held MM110's tail and the
approach to MM111. Yesterday's X9 run counted 2340 discards, and they were not new: 1020 in dwell, 788 on
zero ramps, 281 in departure, 251 on open track, with bursts of 10 to 29 in a second while moving at PWM 90
(for example +15 at 18:44:41 on MM105, the first magnet after an Arches CCW departure, which still judged
at 0.0661). So repeated discarding while moving is X9 behaviour as well. What is new today, or at least
newly visible, is that the record that finally stayed open began 21 to 25 ms into the flank. Whether X11's
quieter samples let the plateau test trip on a slow flank where X9's noise did not is the open question the
code trace was asked to settle; it is not established here.

## The strike routing

`stopEpisode_` is cleared when a passage opens ([HallCapture.h:292](../firmware/test-programs/NAVI_ONE/HallCapture.h))
and set on every sample of that passage while the stop arming is anything but `None`
([HallCapture.h:334](../firmware/test-programs/NAVI_ONE/HallCapture.h)); it reaches the judgement as
`j.stopEpisode`. In the NotAMagnet case,
`if (j.kind == 1 || j.stopEpisode) refusedStitched(j);` ([NAVI_ONE.ino:1367](../firmware/test-programs/NAVI_ONE/NAVI_ONE.ino)),
with the comment that a refusal "AROUND A STOP" stops the locomotive because conditioning on the stitched
kind alone "is what let Bamboo run on" (2026-09-02 12:29:07). Arches had not reached DEPARTED when the
MM111 passage opened, so the passage carried the flag, and the refusal stopped Toby as that comment intends.

`refusedStitched()` publishes `STITCHED_REFUSED` and withdraws with a message that says the passage was
paused, resumed on Hall morphology and stitched. For this passage (kind 0, paused 0, stitched 0) that text is
false. The routing and the wording are two different things.

## The mechanism, from the code

This is a code reading, not yet independently verified; the trace in the last section is checking it.

The discard branch ([HallCapture.h:363](../firmware/test-programs/NAVI_ONE/HallCapture.h)) fires on a
sample when the stop machinery is armed, `plateau_` is true, the passage has not yet seen progress
(`sawProgress_` false) and the settled level is below the entry margin. Its own comment records the
first form of this fault, Arches CCW 2026-09-02 14:14:40: discard on every sample, reopen on the next,
"with the pre-roll frozen the whole time because a pre-roll does not fill while a passage is open",
residual 0.2706. The guard added then, "only if there is no field there", tests the *settled* level.

`plateau_` is not computed per sample. `updateProgress()`
([HallCapture.h:513](../firmware/test-programs/NAVI_ONE/HallCapture.h)) takes one raw sample every
25 ms into a 16-sample window (400 ms), sorts it, drops the single highest and single lowest, and calls
the field flat when the remaining span is 20 counts or less. Between updates the flag holds.

Put those together on a departure flank:

1. Between magnets the window holds 16 samples near zero. `plateau_` is true and `plateauLevel_` is near
   zero, well below 38, so the "no field there" guard passes.
2. The flank arrives. Within one 25 ms step it crosses the entry margin. At most one window sample shows
   the rise, and the trim discards the single highest sample, so the window still reads flat.
3. The passage opens on the entry sample with `plateau_` true and `sawProgress_` false, and is discarded on
   that sample. The field is still over the threshold, so it reopens on the next sample and is discarded
   again. The pre-roll ring is not fed while a passage is open, so it keeps the twelve samples from before
   the first opening.
4. At the next 25 ms update a second flank sample enters the window, the trimmed span exceeds 20, `plateau_`
   clears, and the passage stays open from wherever the flank has reached, spliced onto the stale pre-roll.

The amputation is therefore bounded at one progress step, 25 ms, and its size in counts is the flank slope
times the time lost. Measured against the records: MM111 lost ~25 ms at 3.2 counts/ms, an 80-count step;
MM110 lost ~21 ms at 1.8 counts/ms, a 38-count step. The 55 discards in those two seconds are one per
sample of cycling, 46 of them in the two flanks. Each discard fires only while armed, so cruise passages
are untouched, and a slow flank loses little. The exposure is a strong magnet crossed at departure speed
inside the armed window. MM111 is that magnet, ratio 1.02, and it was inside the window today because
DEPARTED had not fired; on the five earlier CW departures the stop lay past MM110 and MM111 was met later
in the departure at a shallower slope.

**None of this code changed between X9 and X11.** `git diff e010846..HEAD` on HallCapture.h is empty; X10
and X11 touched only NAVI_ONE.ino. The 20 discards at Grillers today and yesterday's 2340 on X9 are the
same branch. What made today's instance visible is geography: the Arches CW stop fell at MM108 with the
rest on MM109, where earlier CW runs stopped past MM110. Whether X11's quieter samples also lengthen the
flat verdict is not established and is left to the trace.

## Side finding: the boot line is truncated

The `state/bootid` payload arrives as exactly 399 characters ending in `"resume`, cut inside a key. The
buffer is `char b[400]` ([NAVI_ONE.ino:1186](../firmware/test-programs/NAVI_ONE/NAVI_ONE.ino)) and the
X-series fields outgrew it. Any consumer that parses the boot JSON discards it.

## What this run does not settle

- Whether X11 changes how often the 400 ms window reads flat between magnets. No X9 post-departure record
  was ever published (accepted passages publish only in a withdraw window), so the X9 flanks have not
  been seen, and yesterday's post-departure residuals of 0.070 to 0.101 are consistent with smaller steps
  on shallower flanks.
- Why the Arches CW stop landed at MM108 today and past MM110 on 2026-09-01. Not looked at here.
- The stitched MM109 residual of 0.0128 is far below any ordinary passage; the fit ran on a decimation-4
  record. Worth understanding before it is treated as a quality signal.

## Verified findings from the code trace

(Appended below when the trace reports.)
