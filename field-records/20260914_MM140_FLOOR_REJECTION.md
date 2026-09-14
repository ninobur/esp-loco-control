# 2026-09-14 — Otto stops at MM140: the 82 ms floor swallows a real magnet

**Locomotive:** Otto, 9950011
**Build:** `NAVI_ONE_1_0X17_FIXED_BASELINE_FIELDTEST`
**Stop:** 12:29:50.356, CW, `WRONG MAGNET at MM140: expected S at MM141, read N`
**Source:** `9950011_20260914_120505.log` (12:05:05 → 12:31:50, 1,299 markers)

The operator reported this as "around 12:15". The run contains exactly one
stop and it is at 12:29:50. Otto was running normally at 12:15.

## What happened, in order

```
12:29:45.020  MM137 S   peak 112  dur  85 ms   ADVANCED
12:29:46.123  MM138 S   peak 114  dur  94 ms   ADVANCED
12:29:47.132  MM139 S   peak 118  dur  90 ms   ADVANCED
12:29:48.165  ——— DURATION_FLOOR: dur 79 ms, floor 82, peak 99, sum −5983 (South)
12:29:49.221  "MM140" S peak  96  dur  89 ms   ADVANCED   gap_ms 1988
12:29:50.321  DISAGREE  obs N, expected S at MM141 → WRONG_MAGNET → one-strike stop
```

Route polarity: MM137 S, MM138 S, MM139 S, **MM140 S, MM141 S, MM142 N**.

The 79 ms event at 12:29:48 arrived exactly one marker interval after MM139 and
one before the next accept, and its signed sum is negative. It was the real
**MM140**. The duration floor discarded it before recognition, so navigation
never saw it. The passage accepted 1,056 ms later as "MM140" was physically
**MM141** — also South, so it agreed and the error was invisible for one step.
MM142 is North. It contradicted the expectation and struck correctly.

Navigation did nothing wrong. Every layer above acquisition behaved exactly as
designed, including the stop.

## Why a genuine magnet measured only 79 ms

`shadow_delta` was **+31** at that moment. The startup baseline was frozen at
1905; the quiet Hall line had risen to 1936.

The entry test is `|raw − 1905| ≥ 70`. With the true line 31 counts above the
frozen reference, a **South** magnet — which pulls the reading down — must now
develop 101 counts of field to open a passage instead of 70, and falls back
below that threshold sooner. A **North** magnet needs only 39. The passage
geometry therefore splits by polarity, and the split grows with the drift:

```
shadow_delta |  SOUTH: n   peak   dur   min dur |  NORTH: n   peak   dur
   −8 … −1   |       18    174   132       114  |      29    177   142
    0 …  7   |       89    178   133       115  |      78    178   138
    8 … 15   |      140    162   119       101  |     152    186   149
   16 … 23   |      154    155   112        87  |     161    192   157
   24 … 31   |      244    150   108        84  |     232    197   168
```

At `shadow_delta = 0` the two poles are indistinguishable: 178/178 counts,
133/138 ms. By +31 they have separated by 47 counts of peak and 60 ms of
duration, in opposite directions, which is the signature of a fixed reference
under a rising line and nothing else.

South passages within 10 ms of the floor appear only once the drift passes ~16
counts: 0 of 247 below that, 16 of 398 above it. One of those 16 crossed under.

## The drift itself

Uptime shows Otto was **not** power-cycled between the day's recorder sessions:

```
12:05  uptime    48 s   baseline 1905   shadow 1905   delta   0
12:31  uptime  1653 s   baseline 1905   shadow 1937   delta +32
12:40  uptime  2153 s   baseline 1905   shadow 1957   delta +52
12:48  uptime  2655 s   baseline 1905   shadow 1950   delta +45
12:50  uptime   281 s   baseline 1949   shadow 1948   delta  −1   ← power cycle
```

Roughly **+1.2 counts per minute** over the first 45 minutes, in one direction.
That matches the 2026-09-13 bench anchor independently: +56 counts in 38 minutes,
same sign, same magnitude, measured with a different method. Whatever moves this
line is real, monotonic over tens of minutes, and resets on a power cycle — which
is consistent with warming, though nothing here establishes that.

## The failure escalates with the drift

Two later stops the same afternoon, on the same uninterrupted boot:

```
12:47:45  MM035  DISAGREE  dur 4069 ms  delta +44
12:50:02  MM050  DISAGREE  dur 1047 ms and 229 ms fragments, delta +45
```

Those are latched passages — the Northpoint symptom. So the fault has two
regimes on the same cause:

- **delta ≈ 16–35:** South passages shorten; occasionally one falls under the
  82 ms floor and a magnet is silently dropped. This is the MM140 stop.
- **delta ≈ 44+:** the quiet line no longer reaches within the 25-count exit
  band reliably, passages stop closing, and magnets are merged.

## A prediction of mine that was wrong

The 2026-09-14 pre-flash audit predicted a hard cliff at `|shadow_delta| ≥ 25`,
on the reasoning that the quiet line could then never satisfy the exit test.
That did not happen. This run reached +32 and closed all 1,299 markers, longest
passage 435 ms, none over 500 ms. The simulation assumed a flat line at the
shifted level; the real line evidently still dips inside the exit band often
enough to satisfy the 8 ms hold. The latch did appear, but only in the mid-40s.

The audit's mechanism was right and its threshold was not. What it missed
entirely is the failure that actually occurred first: the drift reaching the
**duration floor** through the entry threshold, well before it reaches the exit
threshold.

## What this run established

1. The fixed baseline held perfectly. `base_open == base_close == 1905` on every
   one of 1,299 markers. The mechanism works as specified.
2. The line moves — **+52 counts in 45 minutes**, confirmed against an
   independent bench measurement. The field test's decisive question is
   answered, and the answer is yes.
3. A fixed reference does not merely risk a latch. It biases passage
   *geometry* by polarity long before that, and the 82 ms floor is the first
   thing that bias breaks.
4. The 82 ms floor and the entry margin are not independent settings. Anything
   that offsets the reference converts directly into duration, and the floor
   has 2 ms of measured margin on this railway.

## References

- `docs/NAVI_X17_FIXED_BASELINE_PREFLASH_AUDIT_20260914.md`
- `docs/NAVI_OTTO_20260913_INVESTIGATION_SUMMARY.md`
- `field-records/20260913_NORTHPOINT_ACQUISITION_LATCH.md`
- Decision 0085 — the 82 ms floor and its 82–95 ms warning band
