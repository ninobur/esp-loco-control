# NAVI_ONE — is baseline-dependent Hall closure a navigation requirement?

**Date:** 2026-09-15
**Build under analysis:** `NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST` (working tree,
`firmware/test-programs/NAVI_ONE/`), Otto 9950011, entry 70 / exit 25 / floor 82,
`fixedAfterPrime = true`.
**Corpus:** `OTTO_X18_ALL_20260915.tar.gz` — 15 logger segments, 83,915 messages,
8 boots (one X17, seven X18), 10:54 to 13:48.
**Status:** Analysis only. No firmware changed, no X19 created, no decision
record proposed. Nothing here is ratified.

---

## Answer

**No. Returning to within ±25 counts of a global baseline is not a navigation
requirement.** It is one implementation of "the magnet is over", and on
September 15 it was the implementation that failed.

Two claims, kept separate because the evidence for them differs:

1. **Closure is not what counts magnets.** The 500 ms re-read guard counts
   magnets. Closure only has to re-arm the detector in time for the next one.
   This is demonstrated below on the day's three merge failures: with a
   reference-free close rule, the guard produces exactly one advance per
   physical magnet without any other change.

2. **Eliminating baseline-dependent closure fixes three of the day's four
   in-motion strikes, not four.** The fourth (13:21) was lost on the *entry*
   side — a starved passage under the 82 ms floor — and no closure rule reaches
   it. Closure is not the only place the absolute reference has authority, and
   this report does not claim otherwise.

---

## 1. What closure currently accomplishes in the code

`HallCapture::close()` (`HallCapture.h:249-310`) is the sole producer of a
`Passage`, and a `Passage` is the sole input to `MagnetRecognizer` and
`Navigator`. Ten things happen at, or because of, close:

| # | At close | Site |
|---|---|---|
| a | `sum_` stops accumulating; its **sign becomes the polarity** | `HallCapture.h:282` |
| b | the buffer is **oriented** (negated if S) | `:283-284` |
| c | the **median-of-three judgement copy** is built | `:290` |
| d | **peak** is read from that copy | `:291-293` |
| e | **duration** is measured and the 82 ms floor applied | `:251-275` |
| f | the `Passage` is **emitted** to recognition | `:295-309` |
| g | `open_ = false` — the detector **re-arms** | `:250` |
| h | `closedAtMs` becomes the **anchor for the next 500 ms guard** | `MagnetRecognizer.h:263` |
| i | `closedAtMs` feeds the **speed estimate** (display only) | `NAVI_ONE.ino:1278-1281` |
| j | `baseline()`/`shadowBaseline()` sampled at close feed the **X18 lap estimator** | `NAVI_ONE.ino:1283-1284` |

There is an eleventh, implicit: while a passage is open *beyond* `openMigrateMs`,
`updateBaseline()` resumes taking median samples from inside the magnet
(`HallCapture.h:353-358`). Closure therefore also bounds shadow contamination.
That path was harmless under X17 and is not under X18 — already recorded as
finding (a) in `field-records/20260915_OTTO_HALL_ZERO_SHIFT.md` §4.

## 2. Which of those functions are truly necessary

Separating the requirement from the technique:

| | Needed for navigation? | Does it need *return to baseline*? |
|---|---|---|
| a polarity | **yes** | **no** — the sign of `sum_` is decided long before the tail. The artifacts decision 0064 guards against (+41 / −43 single samples) sit against sums of −12,691 and +19,742. What polarity needs is *enough of the arc*, not the end of it. |
| c,d peak | **yes** | **no** — the peak is fixed once the apex has passed. |
| e duration floor | **yes** | **coupled, and this is the real constraint.** 82 ms is operator-approved (decision 0085) and was derived against *this* definition of duration. See §5. |
| g re-arm | **yes** | **no** — any rule that ends the event re-arms. |
| h guard anchor | **yes** | **no** — it needs *an* anchor, not a baseline-derived one. |
| b, f | mechanical | — |
| i | display only | — |
| j | X18-specific; already identified as a contamination path | — |

**Nothing in the list requires the signal to come back to a remembered absolute
level.** Every genuine requirement is satisfied by "the arc is over".

The one thing that is genuinely bound to the current mechanism is the **duration
floor**, because duration is measured open-to-close and the tail is exactly the
part a different rule would discard.

## 3. What September 15 shows

### 3.1 The passage population is tight, and the guard population is far away

4,229 accepted passages:

```
duration   p0 85   p25 133   p50 145   p90 174   p99 226   p99.9 650   max 22053 ms
gap        p0 551  p1 868    p50 1087  p95 1384  p99 1585  max 46541 ms
```

Only 42 accepted passages (0.99%) exceed 226 ms; only 20 (0.47%) exceed 300 ms.
The gap population — close-of-previous to open-of-this — has **exactly one**
member below 600 ms and its first percentile is 868 ms. The 500 ms guard sits
roughly 350 ms below the genuine population and ~8× above the surveyed rebound
maximum of 64 ms. **It has room to do the counting.**

### 3.2 The failures are two mechanisms, and only one of them is closure

Four in-motion strikes. Three share one signature — a long passage whose
polarity came out wrong, immediately followed by a short passage with the
*correct* polarity:

```
11:05:20  DISAGREE  dur=1852  obs=N exp=S   then 128 ms obs=S (correct)
11:12:02  DISAGREE  dur=3652  obs=N exp=S   then  97 ms obs=S (correct)
12:06:24  DISAGREE  dur=1443  obs=S exp=N
13:21:03  DISAGREE  dur=1348  obs=S exp=N   -- NOT a merge; see below
```

The fourth, 13:21, is a **floor starvation**, not a merge: the lost magnet is
the 76 ms rejection at 13:20:55 (`raw_peak=101` against a typical accepted peak
of ~175). All 24 floor rejections of the day have `raw_peak` between 70 and 129
— starved passages that barely cleared the entry margin. That is an *entry-side*
amplitude failure caused by a high reference, and **no closure rule reaches it.**

### 3.3 The merges are visibly two magnets, and the separation is large

79 `diag/waveform` messages decode to 55 passages, 52 with every chunk present.
Rendered, the merges are unambiguous — two well-separated humps with the signal
falling to a flat plateau between them:

```
11:05  dur=1852  peak 206   inter-hump plateau 17..50    trough/peak 0.08
11:12  dur=3652  peak 224   inter-hump plateau 30..68    trough/peak 0.13
12:06  dur=1443  peak 223   inter-hump plateau 17..~40   trough/peak 0.08
```

against clean single passages whose only sub-50% excursion is their own tail
(trough/peak 0.43–0.60 by the same measure, which is the degenerate answer for
a unimodal arc).

**The signal does depart the magnet between the two magnets. It simply does not
return to 1935.** The plateau *is* the offset: at 11:05 the reference was 33
counts low, and the plateau sits at 47–50.

### 3.4 The X18 controller is not merely slow; it saturated on most laps

23 lap updates, all `valid`, all `coverage 171/171`. Requested corrections:

```
-29 -21 -1 0 0 1 1 1 1 1 1 2 3 4 5 5 6 7 8 12 12 13 19
```

**13 of 23 laps (57%) requested more than the ±2 cap.** The estimator was
right; the authority was two orders of magnitude short of the drift rate, as
§3 of the field record already established from a different direction.

### 3.5 An offset above the entry margin is an outage, not a stall

With `fixedAfterPrime = true`, `baseline_` moves only via `adjustBaseline(±2)`,
which requires 171 accepted advances, which require closed passages. When the
zero wanders past 70 counts the detector opens on ordinary track and cannot
close; boot 3 recorded passages of 111,304 / 78,595 / 51,981 / 28,158 ms.
These ended when the *sensor zero wandered back*, not through any recovery
mechanism. Boot 3 produced 143 agreements and 87 `NO_POSITION`; boot 4 produced
**zero** agreements against 23 `NO_POSITION` after priming at 2006.

## 4. The simplest alternative the data supports

**It already exists and it already has a replay.** The operator's peak-relative
close rule of 2026-09-13 (`docs/NAVI_PEAK_CLOSE_REPLAY_20260913.md`):

> close when the signal has spent **N consecutive ms more than H counts below
> its own running peak**, with `peakCloseLockout` — after such a close, do not
> re-open until the signal has fallen below the entry margin once.

Measured against `entryBaseline_`, which is frozen at open. **No live reference
participates in the decision to close.** That replay established, over 3,132
Otto and 187 Toby magnets, zero polarity differences and zero peak differences
at N ≥ 20, and that the lockout is what prevents the naive rule from shredding
arcs.

This report adds the September 15 corpus, which is a *different failure
population* from the Northpoint latches that rule was tested against: Northpoint
was a single plateau; September 15 is two clean arcs separated by an offset
plateau.

I also swept a fraction-of-peak variant (close below `k × peak`). **It is worse
and should not be pursued:** at k = 0.25 it leaves 19 of 21 clean singles
bit-identical but fails to separate the 11:12 merge at all (that plateau sits at
0.28–0.30 of peak); at k ≥ 0.35 it separates that merge but truncates a genuine
weak magnet from 153 ms to 53 ms. The offset and a weak magnet's body are the
same order of magnitude (up to 68 counts against peaks of 98–112), so a single
scalar fraction cannot separate them. Recorded so it is not tried again.

I also tested a **quiescence** rule (close when a trailing window varies by less
than F counts). On its own terms it discriminates beautifully — at an 80 ms
window, magnet apex variation is min 52 / median 82, against inter-hump plateau
variation of median 8–16 — but it **shatters every low-speed passage**: all five
pwm-42 passages in the corpus split into 2–7 events at every setting tried.
Falsified; do not pursue.

## 5. Replay results, and what they cost

Replaying the operator's rule over the recovered waveforms, then passing the
resulting events through the **existing, unchanged** 82 ms floor, 500 ms guard
and 0.34 amplitude test:

```
N=80 H=8, peakCloseLockout, production absolute test retained

11:05 MERGE   1852 ms, 1 event  ->  4 events: 176(pk204) 108(pk95) 228(pk205) 28
                                     advances: 2   CORRECT   (108 refused TOO_SOON, 28 refused FLOOR)
12:06 MERGE   1443 ms, 1 event  ->  2 events: 184(pk159) 176(pk222)
                                     advances: 2   CORRECT
11:12 MERGE   3652 ms, 1 event  ->  6 events, 3 refused TOO_SOON
                                     advances: 3   (3652 ms at pwm 90 ~ 3 marker intervals; consistent)
13:21 SINGLE  1348 ms, 1 event  ->  1 event: 196(pk220)      advances: 1   CORRECT
13:21 SINGLE  1294 ms, 1 event  ->  1 event: 176(pk227)      advances: 1   CORRECT

14 clean single passages: 0 split, 0 pushed under the 82 ms floor
  durations 87->87  91->91  102->101  108->105  119->119  121->121  124->124  130->130
5 low-speed (pwm 42) passages:    1 event each, no shredding
```

**This is the load-bearing result: the 500 ms guard performs the
disambiguation.** The split does not have to be surgically correct. It has to
re-arm the detector; the guard then refuses the fragments and advances once per
magnet. Closure's navigation-critical job is to be *timely*, not *accurate* —
and timeliness does not need a correct reference.

### The costs, stated plainly

- **The floor and the close rule are coupled.** The 09-13 replay measured
  durations ~23% shorter at N=40 H=8, with 5 of 3,132 falling under 82 ms. On
  this corpus N=80 H=8 leaves durations essentially untouched (0 of 14 under the
  floor) and N=40 H=8 puts 3 of 14 under it. **N is not a free parameter; it is
  the parameter that decides whether decision 0085 survives.** If the close rule
  changes, the operator's 82 ms must be re-derived against the new definition,
  not carried across.
- **N = 80 is not derived from anything.** It is the corner of a small sweep
  that holds durations up on 14 passages. It has no more standing than N=40 did.
- **Over-fragmentation is noisier on `diag/acquisition`.** The 11:12 merge
  produces six events where one appeared before. Under decision 0080 a refused
  fragment has no navigation authority, so this is telemetry noise, not a
  hazard — but the operator will see it.
- **It does not fix 13:21.** Entry-side starvation is untouched.

## 6. What this data cannot validate

Stated separately from the above, because these are not weak results — they are
absences.

- **Low speed is effectively untested.** 4,204 of 4,229 accepted passages
  (99.4%) were at PWM ≥ 80. There were 21 at 60–79, 4 at 40–59, none below. Every
  low-speed passage for which raw samples exist comes from boot 3's struck,
  badly-offset period — the failure regime itself. **The low-speed sample is
  drawn entirely from the population the rule is meant to fix**, so it cannot
  serve as a control. This is survivorship bias with the sign reversed, and it
  is the single biggest gap.
- **Opening cannot be replayed at all.** There is no continuous raw stream. The
  only samples on record are the 52 recovered passages (which begin at the
  existing detector's open) and 155 departure-diagnostic records at 10 Hz —
  too coarse to resolve a 150 ms passage, and covering 5 episodes in one boot.
- **Floor rejections have no waveforms.** `publishWaveformSlot` is called only
  on a refused *Passage*, and a floor rejection is deliberately not a Passage
  (`HallCapture.h:73-81`). So the 24 rejections of the day are known only by
  their scalars. The 13:21 starvation cannot be replayed.
- **Sub-event polarity is inherited, not measured.** The stored samples are
  already oriented by the *parent* passage's polarity, so the split events'
  poles are not an independent measurement. The field record establishes merges
  occur inside same-pole runs (an opposite-pole neighbour swings the signal
  through zero and self-closes), which is consistent — but it is not proof.
- **Station dwell and stopped-in-field are untested.** A locomotive parked in a
  fringe field currently holds a passage open for the whole dwell, deliberately,
  so that no guard window runs (`MagnetRecognizer.h:81-86`). A peak-relative
  rule would close that passage ~N ms after the apex and start the guard
  running. **The consequences of that were not examined here and are not
  obviously benign.**
- **The route premise, corrected.** Markers are 280–355 mm apart, median 300
  (`ROUTE_SPACING_MM`, 171 markers, 52.15 m circuit) — not ~1 m. The margin
  between magnet footprint and spacing is ~10:1, not ~30:1.

## 7. The smallest experiment that would falsify or support this

**Not a new closure policy.** A measurement aperture.

A build that runs the proposed close rule **in shadow** — computing, for every
passage, where the peak-relative rule *would* have closed and what the resulting
event sequence *would* have been — and publishes that alongside the real
outcome. Acquisition, recognition and navigation behave exactly as X18 does.
Nothing the locomotive does changes.

That yields, in one session of the size already achieved (3,436 advances),
the two numbers this report is short of:

1. **How often the two rules disagree, across thousands of passages** rather
   than 52 — and in particular whether any *clean* passage would have been split
   into two accepted advances.
2. **The duration distribution under the new rule**, which is what decision
   0085's floor must be re-derived against before any closure change is flashed.

It must be run **with station stops and at least one deliberate slow section**,
because those are precisely the populations this corpus lacks.

The hypothesis is falsified if shadow-rule advances diverge from real advances
on passages where the reference was demonstrably healthy, or if the shadow
duration distribution puts a material fraction of genuine magnets under 82 ms.
It is supported if disagreements occur only where the offset exceeded the exit
margin.

One caveat on interpretation, per the standing correction of 2026-09-02: a
shadow replay over stored passages is **segmentation evidence, not acquisition
coverage**. It cannot tell us what the detector would have opened on.

## 8. Provenance

- `OTTO_X18_ALL_20260915.tar.gz`, 15 segments, 8 boots. Boots segmented by
  `alert.uptime_ms` regression.
- Waveforms decoded from `diag/waveform` base64 per `WaveformDump.h` (40-byte
  header, `<BBBBBBBBHHHHHHffIII`), reassembled by `(openedAtMs, closedAtMs)`.
- Replay scripts are scratch, not committed; every number above is reproducible
  from the archive and the decode described.
- Prior work this rests on and does not supersede:
  `docs/NAVI_PEAK_CLOSE_REPLAY_20260913.md` (the rule and its 3,132-magnet
  replay), `field-records/20260915_OTTO_HALL_ZERO_SHIFT.md` (the zero shift,
  the X18 review, the controller arithmetic).

## 9. Corrections made while writing this

Recorded so they are not repeated.

- "The pre-roll gives a local, uncontaminated zero." **False.** At ~300 mm/s the
  12 ms pre-roll sits on the foot of the arc: clean passages enter the pre-roll
  at 40–60 counts and close at 14–25. The pre-roll measures the magnet, not the
  track. This killed the most attractive candidate.
- "A fraction-of-peak close is the simple answer." It is simpler to state and
  worse in practice; see §4.
- "The five pwm-42 passages are a low-speed control." They are not — they are
  from the struck, offset regime. Retained in §5 as evidence against the
  quiescence rule only, where the failure is unambiguous either way.
