# The Epiphany bench test — X9 against X11 on the kitchen table

**Date:** 2026-09-02 23:48 to 2026-09-03 00:49
**Locomotive:** Toby (9950012), power car on the table, loco on blocks, MANUAL
**Builds:** NAVI_ONE 1.0X9 (one ADC read per sample) then 1.0X11 "Epiphany"
(one sample is the median of five reads), flashed by the operator at 00:36
**Instrument:** a marker magnet on a ruler, slid under the power car and held
still beneath the sensor. In MANUAL a held field keeps the passage open until
the magnet leaves, the flat top fails the shape test, and every refusal
publishes its complete record on `diag/waveform`.
**Scorer:** `tools/curves/score_bench.py` (36947fe, 30e33eb): a reading more
than 20 counts from the five-wide median of its own record is a bad read;
held records are scored on their flat top, less the first second and last
half second where the ruler is still settling.

## Verdict

**The bad reads are single conversions, and the median of five removes them.**

| build | condition | flat-top samples | bad reads | rate |
|---|---|---|---|---|
| X9 | still, four holds of 13–15 s | 1527 | 2 | 1.3 / 1000 |
| X9 | still, tapped repeatedly during one hold | 466 | 0 | 0 |
| X9 | motor at 69–90 PWM, six holds of 2.5 s | 1738 | 2 | 1.2 / 1000 |
| **X9 total** | | **3731** | **4** | **1.1 / 1000** |
| X11 | still, eight holds of 13–19 s | 2620 | 0 | 0 |
| X11 | still, eight more holds of 14–20 s | 2493 | 0 | 0 |
| **X11 total** | | **5113** | **0** | **0** |

At the X9 rate, 5113 samples should have held five or six bad reads. Zero
happens by chance about four times in a thousand. Counting the hand passes
of the first round as well (4593 samples at full rate, 8 bad, 1.7 per 1000),
the X9 rate is 1.4 per 1000, seven expected, and zero is under one in a
thousand.

Every bad read on X9 was one stored sample, 22 to 58 counts off, gone the
next sample, in both directions: 113 in a run of 171–177, 317 in a run of
258–266, 26 in a run of 60–63, 234 in a run of 174–181, 142 among 187s.
On X11 the largest single excursion on any flat top was 14 counts.

## What the fault needs

**A field on the sensor.** A bad read of 38 counts or more opens a passage
by itself, and three did tonight, each a single sample of 39, 53 or 62 in a
line of 21s. In the quiet 17 minutes between 23:52 and 00:09, about a
million conversions with nothing near the sensor, none did. At the off-idle
rate there should have been hundreds.

**Not motion.** The loco and the magnet were both still for every hold.
**Not the motor.** 1.2 per 1000 at 69–90 PWM, 1.3 per 1000 at rest.
**Not the mount or a connector.** Repeated taps beside the sensor during a
held field: 466 samples, nothing over 14 counts.
**Not the IR channel.** Toby has no IR fitted; `irService` returns before
it reads; GPIO33 is the only channel this locomotive ever converts.
**Not a stale or settling conversion.** Mid-hold, every prior read for six
seconds sat at the held level, so a retained value would BE the held level.
The excursions go both ways and never toward idle in particular.

What survives: one wrong conversion, or a glitch on the sensor output shorter
than the five reads (about 100 µs), either only while the sensor is in a
field. This test cannot tell those two apart. It does show that a median of
five at the read is a complete containment for the single-sample kind.

## What it does not cover

The MM157 burst that stopped X9 on the railway (decision 0071) was
`-2 -13 108 109 174 68 48`: adjacent stored samples at 1 ms, a fault lasting
several milliseconds. Five reads inside 100 µs cannot touch that. Decision
0071's five-wide judgement median stays for it; the two medians are for two
different durations of fault, and tonight measured only the short one.

## Registered, not investigated

- **Periodic dips of 11 to 14 counts** at roughly half-second spacing in two
  X11 records: 00:41:00, nine dips to exactly 215 from a level of 229; and
  00:46:25, ten dips of 11 to 13. They survive the read median, so they last
  longer than the five reads. Under the 20-count line, invisible to the
  recognizer. The spacing is of the order of the once-a-second publishes; a
  ratiometric Hall output on a rail the radio pulls on would look like this.
- **The ordinary noise floor is unchanged** by the median: the 90th
  percentile deviation is 4 to 5 counts on either build. The everyday ±5 is
  not per-conversion.
- **A hand pass is a marker.** The first round's passes read as textbook
  Gaussians (residual 0.05–0.09, peaks 235–314) and were accepted: two
  advances, then a polarity strike at MM75, which withdrew the position and
  cut the throttle at 90 PWM in MANUAL at 00:23:38. Every accepted passage
  after the strike was lost, because an accepted shape is published only on
  a withdraw. Decision 0072 is the answer to that.
- **A held field with the motor running closes after 2.5 s.** Above the
  tractive floor the live reference is allowed to migrate under an open
  passage after `openMigrateMs`; the held field closed on the migrated
  reference and the magnet's removal opened as the opposite pole. As
  designed; noted because it halved the record length in that round.
- **The magnet parks past the sensor unless placed with care.** The second
  round's 15-second holds produced ten clean arcs and no held record at all:
  the sensor read nothing while the magnet sat.

## Sequence, for the record

| time | build | what | result |
|---|---|---|---|
| 23:48–23:50 | X9 | hand passes, strong and weak | 14 records, 8 bad in 4593 |
| 00:09–00:11 | X9 | "holds" that parked past the sensor | 10 arcs, nothing held |
| 00:16–00:18 | X9 | four still holds | 2 bad in 1527 |
| 00:21 | X9 | one tapped hold | 0 in 466 |
| 00:23 | X9 | six holds at 69–90 PWM, then a strike | 2 bad in 1738 |
| 00:36 | X11 | flashed, boot line confirmed | |
| 00:39–00:42 | X11 | eight still holds | 0 in 2620 |
| 00:45–00:49 | X11 | eight still holds | 0 in 2493 |

## What is proposed

Decision 0073 (PROPOSED): one reading is the median of five conversions.
X11 is on Toby now. Whether it goes to the railway is the operator's call.
If the outlier's position among the five is wanted, an X12 that tallies every
five-read set whose spread exceeds 20 counts, by position and sign, into the
status alert, would collect it on every lap.
