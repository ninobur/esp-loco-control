# NAVI_TRACER_0_2 — Stage B extension: physical waveform boundaries, MM045→MM044

**Status: offline research result. Not a decision record. No firmware
changed, flashed, or substituted. No new recognition mechanism is proposed.
This is the physical-waveform investigation the operator asked for, as an
extension of
`docs/navi/NAVI_TRACER_0_2_STAGE_B_MM045_MM044_20261022.md`. Stopping here
for review, per instruction.**

Date: 2026-10-22. Same recording, same read-only access, same raw native
Hall samples already pulled from the Pi for the prior Stage B pass (no new
data was fetched for this extension — reused in full).

## Summary finding, stated first

**No physical magnetic waveform matching "MM045" exists in the recorded
Hall signal.** The software's `OPEN_CANDIDATE`/`ACQUIRED`/
`CLOSE_AND_ADVANCE` sequence for "MM045" (15:33:23.715–15:33:25.303) was
not triggered by a real magnet. It appears to have been triggered by the
same baseline-extrapolation defect already identified for MM044 — just at
a much milder slope (23.84 ADC/s here vs. 213.80 ADC/s for MM044) — crossing
threshold at a moment that coincides with the very early, still-small
leading edge of MM044's own real approach. The questions below are answered
against that finding, not against an assumed MM045 waveform.

## Method

Used the raw native-ADC samples already extracted for the prior Stage B
pass (`mm044_tight.json`, 210,000 native samples, bad_frames=0, serial
contiguous — see the prior report for provenance). Established the full
declared-event timeline for this run directly from
`/home/david/NGR/telemetry/runs/9950011_20261022_153206.log` (not assumed —
read in full for every `OPEN_CANDIDATE`/`ACQUIRED`/`CLOSE_AND_ADVANCE` event
from MM051 down through MM044):

| Wall clock | Event | mm | target | departure | baseline_level | baseline_slope |
|---|---|---:|---:|---:|---:|---:|
| 15:33:17.510 | CLOSE_AND_ADVANCE | 47 | 46 | 69.96 | 1878.65 | 151.3786 |
| 15:33:20.039 | OPEN_CANDIDATE | 47 | 46 | 103.25 | 1904.41 | −0.2191 |
| 15:33:20.619 | CLOSE_AND_ADVANCE | 46 | 45 | 2.56 | 1904.41 | −0.2191 |
| 15:33:23.715 | OPEN_CANDIDATE | 46 | 45 | 69.60 | 1912.27 | 23.8375 |
| 15:33:23.738 | ACQUIRED | 46 | 45 | 69.45 | 1912.27 | 23.8375 |
| 15:33:25.303 | CLOSE_AND_ADVANCE | 45 | 44 | 4.30 | 1912.27 | 23.8375 |
| 15:33:25.952 | OPEN_CANDIDATE | 45 | 44 | 90.65 | 2098.31 | 213.7989 |

Each wall-clock timestamp was mapped to the recording's on-device clock via
the Pi's own `recv_us` (nearest-match against the already-extracted native
samples, same method as the prior report, typically accurate to single-digit
milliseconds). "Physical" boundaries below are found directly from the raw
waveform's own shape — amplitude, persistence, smoothness — never from a
declared event.

Background reference for this stretch, independently established from a
visibly flat, low-noise span (device t_us [165348540, 169308540), n=199):
**median 1903.66, std 1.52 ADC**, no significant trend (fitted slope
−1.16 ADC/s — noise, not a real drift). A "tight" physical band of 3σ
(≈4.57 ADC) is used below to say a sample is "ordinary track," since the
70-ADC/half-70-ADC bands in the firmware are software departure-test
artifacts, not a description of the sensor's own noise floor.

## Q1 — Where does the MM045 magnetic waveform physically end?

No MM045 waveform was found to end. What *is* found, directly in the raw
signal: a real, clean, single magnetic encounter — South polarity, peak
departure 172.8 ADC below background, minimum 1730.2 ADC at device
t_us=164628540 — that recovers to within the tight background band and
stays there (6+ consecutive quiet bins) starting at **device t_us=165348540**
(hall=1907.86). This encounter's own declared close (`CLOSE_AND_ADVANCE
mm:46 target:45`, 15:33:20.619 → device t_us≈165066455) falls inside its
recovery tail, consistent with normal RTB-persistence lag, not a defect.

Given this is the real waveform immediately preceding the "MM045"
declaration, and no other real excursion exists between it and MM044, **this
is the physically relevant boundary** — I am reporting it as the answer
rather than leaving Q1 unanswered, but flagging plainly that it is MM046's
recovery, not MM045's, because no MM045 waveform exists to measure.

## Q2 — Where does the MM044 magnetic waveform physically begin?

**Device t_us ≈ 169,388,540–169,408,540** (one 20 ms bin of resolution).
At t_us=169388540 (hall=1907.73) the signal is still within the tight
background band (+4.07 ADC); at t_us=169408540 (hall=1909.47, +5.81 ADC) it
has exceeded the band and never returns — every subsequent sample climbs
further, smoothly and monotonically, to a peak of 2072.63 ADC. This matches,
within the same margin, the earlier Stage B report's independent estimate
("the real rise beginning ~0.44 s before the recorded closure event") — both
analyses find the same physical onset from the same raw data by different
methods.

## Q3 — Is there an identifiable ordinary-background interval between them?

**Yes — but it is bounded by MM046's real recovery and MM044's real onset,
not by any MM045 event**, because MM045 has no waveform of its own:

- Interval: device t_us [165,348,540, 169,388,540–169,408,540)
- Span: **≈4.04 s**

This interval is clean by every measure checked: no excursion, low and
flat variance, no trend (§"Method" above), and (§Q6 below) ΔBIC does not
mistake it for a magnet.

## Q4 — How many 20 ms observations fall within that interval, and what is their median?

| | value |
|---|---:|
| n (PWM>0, unfiltered) | 202–203 |
| Median | **1903.67 ADC** |
| Mean | 1904.42 |
| Std | 1.51–5.16 (depending on exact boundary rounding) |
| Min | 1900.02 |
| Max | 1908.09 |

This median is, for all practical purposes, identical to the independently-
established background (1903.66/1.52) — unsurprising since the interval
*is* background, almost by construction, but it is the first clean,
representative result either Stage B pass has produced for this encounter.

## Q5 — How do the physical boundaries compare with NAVI_TRACER_0_1's recorded declarations?

| | physical | declared | declared − physical |
|---|---:|---:|---:|
| "MM045" close vs. MM046's real recovery | t_us=165,348,540 | t_us=169,751,224 (MM045 close) | **+4,402.7 ms** |
| MM044 open vs. MM044's real onset | t_us≈169,398,540 | t_us=170,403,582 (MM044 open) | **+1,005.0 ms** |

The first gap is large because it is not really comparing the same event —
the "declared MM045 close" is itself the artifact being investigated, not
a late recognition of MM046's return. The second gap (the declared MM044
open lagging the real onset by ~1 s) is the direct, already-documented
consequence of the 650 ms/8-bin baseline-establishment delay plus its own
contamination (engineering failure report, architectural continuity audit):
the software could not declare the opening until its corrupted 650 ms fit
had run its course, by which point the real signal had been rising for
roughly a second.

## Q6 — Does the line-versus-curve model distinguish the two waveforms and the background?

Evaluated separately, non-gating, exactly as decided (decision 5):

| Window | n | span | ΔBIC |
|---|---:|---:|---:|
| MM046's real waveform (rise + dip + recovery) | 56 | 1.12 s | **219.85** |
| Clean background interval (Q3) | 202 | 4.04 s | **11.80** |
| MM044's real waveform (rise + peak + partial fall) | 75 | 1.50 s | **283.43** |
| The old recognizer's own "MM045" candidate window (its declared open→close) | 80 | 1.58 s | **184.27** |

The two genuine magnets score 220–283; the genuine background scores 11.8 —
a clean, well-separated distinction in this data (ΔBIC correctly treats
"ordinary track" very differently from "a real magnet"). **The old
recognizer's own "MM045" window scores 184.27 — in the same range as the
real magnets, not the background.** This is not a contradiction: that
window's own end boundary (device t_us=169,751,224) falls *after* MM044's
real onset (169,398,540–169,408,540, Q2), so it already contains the first
~340–350 ms of MM044's genuine rise. ΔBIC is correctly detecting a real
curve in that window — it is MM044's curve, mislabeled by the software as
MM045's.

This is, on this evidence, a point in favor of line-versus-curve as a
*diagnostic*: had it been evaluated on the background-only portion of this
same window (Q3's interval, ΔBIC 11.8) versus the declared-but-phantom
"MM045" candidate's own accumulated samples, it would have correctly
indicated "this is not yet a magnet" for most of that span. Per decision 5,
I am reporting this as a diagnostic observation, not proposing to use it as
an admission gate or a new mechanism.

## What this extension does not do

- Does not explain why MM045's own candidate declared at all, beyond
  identifying the same baseline-extrapolation mechanism (milder slope) as
  the likely driver — tracing exactly why that slope (23.84 ADC/s) arose,
  or whether a real MM045 magnet is physically missing, misaligned, or the
  software has silently miscounted a position, is not determined here and
  would need its own investigation.
- Does not touch MM152, MM031, or MM063 — still out of scope.
- Proposes no new recognition mechanism, admission rule, or firmware
  change.
- No firmware was modified, flashed, or substituted.

Stopping here for review, as instructed.
