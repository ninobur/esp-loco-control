# How far does Otto's resting Hall level move in an hour?

**Date:** 2026-09-14
**Build:** `NAVI_ONE_1_0X17_FIXED_BASELINE_FIELDTEST`
**Source:** every X17 run of 2026-09-14, 10:25 → 13:11, `shadow_baseline` at 1 Hz
**Method:** samples above the tractive floor only (the shadow median does not
update below PWM 24). Boots separated by `uptime_ms` resets. Laps segmented on
the MM170 → MM0 wrap. Laps containing a stop (SD > 6) excluded from lap statistics.

## The answer depends entirely on whether Otto has warmed up

```
COLD START — boot 12:04:19, baseline primed at 1905

 min since boot    2    4    6    8   10   12   14   16   18   20   22   24   26   34   44
 shadow         1905 1906 1916 1919 1921 1921 1927 1928 1929 1931 1931 1935 1936 1954 1950
 moved            +0   +1  +11  +14  +16  +16  +22  +22  +24  +26  +26  +30  +31  +50  +45
```

```
WARM START — boot 12:50:25, baseline primed at 1950, run still in progress

 min since boot    2    4    6    8   10   12   14   16   18   20
 shadow         1948 1948 1950 1950 1950 1950 1951 1950 1956 1953
 moved            -2   -2   +0   +0   +0   +0   +1   +0   +6   +3
```

**From cold, about 45–50 counts in the first three quarters of an hour, most of
it inside the first 25 minutes. From a warm start, about 3 counts in 20 minutes.**

Otto's equilibrium this afternoon was near 1950. The 12:50 boot primed there and
has not moved since. The 12:04 boot primed at 1905 and spent 40 minutes
travelling to the same place.

The warm run has 40 minutes left before it answers the hour directly. Until it
does, "3 counts in 20 minutes" is what is measured, not an hour's figure.

## Per-lap statistics

A lap is about 3.2 minutes at PWM 90, roughly 190 one-second baseline reports.

```
COLD BOOT 12:04, fixed baseline 1905
 lap    start    n  median   mean   sd   min   max  range  Δcentre  vs base
   1 12:05:18  156    1905 1905.4  1.3  1903  1909      6              +0
   2 12:07:54  197    1912 1911.6  4.9  1904  1919     15       +7     +7
   3 12:11:11  195    1919 1918.8  1.5  1914  1922      8       +7    +14
   4 12:14:26  193    1921 1921.7  1.5  1919  1927      8       +2    +16
   5 12:17:39  192    1927 1927.3  2.0  1922  1931      9       +6    +22
   6 12:20:52  190    1930 1929.1  2.0  1923  1933     10       +3    +25
   7 12:24:02  191    1931 1930.7  1.7  1927  1934      7       +1    +26
   8 12:27:13  266    1936 1942.6 10.1  1930  1972     42       +5    +31   ← MM140 stop
   9 12:47:05   54    1950 1949.8  1.9  1947  1958     11      +14    +45

WARM BOOT 12:50, fixed baseline 1950
   1 12:51:32  213    1949 1949.1  2.2  1940  1959     19              -1
   2 12:57:45  196    1950 1950.0  1.4  1946  1953      7       +1     +0
   3 13:01:01  194    1951 1950.4  1.4  1945  1954      9       +1     +1
   4 13:04:15  193    1951 1950.4  1.5  1942  1953     11       +0     +1
   5 13:07:28  192    1954 1955.4  4.0  1949  1968     19       +3     +4
   6 13:10:41   68    1953 1952.9  1.1  1951  1955      4       -1     +3
```

Two things stand out.

**Within a lap the level is very quiet: SD 1.1–2.2 counts, range 4–11 counts.**
That holds on the cold boot as much as the warm one. Whatever moves the level
does not make it noisy; it moves the whole lap together.

**Lap-to-lap movement is the entire story.** Cold: +7, +7, +2, +6, +3, +1, +5
per lap. Warm: +1, +1, 0, +3, −1. The same railway, the same speed, the same
sensor — the difference is thermal state, nothing else in the data distinguishes
them.

## The within-lap geography is much smaller than we thought

Each lap's own centre and its own linear trend were removed, and what was left
was averaged by position. Without removing the trend, a rising boot leaks a
false ramp into the profile — early MM low, late MM high — which is what an
uncorrected version showed.

```
detrended, 13 clean laps       detrended, 5 settled laps
    MM band     mean            MM band     mean
    0–14       +0.5              0–14       +0.6
   15–29       +0.3             15–29       +0.2
   30–44       +0.1             30–44       −0.2
   45–59        0.0             45–59       −0.2
   60–74       −0.3             60–74       −0.4
   75–89       −0.9             75–89       −1.1
   90–104      −0.1             90–104      −0.2
  105–119      −0.4            105–119      −0.5
  120–134       0.0            120–134       0.0
  135–149       0.0            135–149      +0.3
  150–164      +0.4            150–164      +0.6
  165–179      +0.8            165–179      +0.8
  swing 1.7 counts              swing 1.9 counts
```

**The repeatable, position-locked component is 1.7–1.9 counts.** The same shape
appears independently in both sets: lowest around MM75–89, highest around
MM165–179.

The working model's "roughly 10-count within-lap variation corresponding to the
hot/cool geography of the railway" is not supported. Total within-lap spread is
about 7 counts peak to peak (SD 2.4, p5–p95 of −4 to +3), and only about 2 of
those repeat by location. The rest is unstructured.

Worth noting without claiming it: the lowest band, MM75–89, sits inside the
MM65–98 bar-magnet stretch. A two-count depression there may be magnetic rather
than thermal. It is too small to matter either way at present.

## What this means for a baseline estimator

1. **The phenomenon a production estimator must track is slow and large:
   ~50 counts, over ~40 minutes, once per cold start.** Nothing else in the
   measurement is close to that size.
2. **The phenomenon it must reject is small: SD about 1.5 counts within a lap.**
   There is a factor of thirty between the two. That is a comfortable
   separation, and it is what makes a whole-lap estimator plausible where the
   one-second median is not.
3. **A settled locomotive barely moves at all.** Three counts in twenty minutes
   is well inside any sane margin. If Otto is allowed to reach equilibrium
   before the baseline is fixed, a fixed baseline may be entirely adequate — the
   X17 failures today all trace to priming at 1905 while the equilibrium was 1950.
4. **The operational number from a cold start:** the reference is 25 counts
   stale by about minute 20, and the 82 ms floor began discarding genuine South
   passages by about minute 25. See the MM140 record.

## Open

- The warm run needs another 40 minutes to give a true one-hour figure.
- Equilibrium was 1950 *today*. Nothing here says what it is on a cold morning
  or a hot afternoon, so 1950 is not a constant to hard-code.
- Nothing here identifies which component moves. It is the resting ADC reading
  of the whole chain, and the enclosure gradients measured this afternoon say
  the exterior temperature is not a proxy for it.

## References

- `field-records/20260914_MM140_FLOOR_REJECTION.md` — the event-level account
  establishing that the MM140 stop was caused by this displacement
- `docs/NAVI_X17_FIXED_BASELINE_PREFLASH_AUDIT_20260914.md`
