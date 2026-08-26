# IR distance vs. surveyed track truth — four laps, silent undercount of 1% to 39%

**Date:** 2026-08-26
**Firmware:** `IR_SCOPE_ESPNOW_TX_1_0` / `IR_SCOPE_ESPNOW_RX_1_0` (raw 1 kHz
waveform capture, GPIO34, ESP-NOW ch11 → USB receiver on the Pi). Wheel
constant: 7 spokes, 87.34 mm circumference = **12.477 mm/pulse**.
**Tow loco:** Toby (`9950012`), QUORUM.
**Supersedes:** the "17% gap, cause unresolved" framing in
`2026-08-26_IR_CW_CCW_LAP_COMPARISON.md`. That gap is now explained, and one
of its premises was wrong — see *Correction* below.

## Ground truth exists in the firmware

`firmware/QUORUM/QUORUM.ino:522` carries `spacingMm[DNA_N]` — the surveyed
physical spacing, in real millimetres, between every adjacent pair of the 171
mile markers. Summed:

> **True Lowline circuit = 52,150 mm = 52.15 m**

This is the arbiter the previous record said it lacked. Note also
`QUORUM.ino:2703`: *"mm" fields carry marker indices — "mm" in this codebase
means MILE MARKER, not millimetres.*

## The four laps

All four are full circuits, Toby towing the IR car. Laps 1–2 ran QUORUM Auto
Solo with station stops (~0.16 m/s while moving); laps 3–4 ran straight
through at 120 PWM (~0.30 m/s), back to back, reversing the car between them.

| Lap | Dir | Sensor mount | Moving | IR pulses | IR distance | **vs 52.15 m truth** |
|---|---|---|---:|---:|---:|---:|
| 1 | CCW | right | 384 s | 3515 | 43.86 m | **−15.9%** |
| 2 | CW | left | 373 s | 4144 | 51.71 m | **−0.9%** |
| 3 | CCW | left / inside-facing | 187 s | 3377 | 42.14 m | **−19.2%** |
| 4 | CCW | right / outside-facing | 167 s | 2539 | 31.68 m | **−39.3%** |

Lap 2 is the important positive result: **within 1% of surveyed truth**, which
also independently validates the configured wheel geometry (lap 2 implies an
88.1 mm rolling circumference against the 87.34 mm configured — 0.9%). The
wheel constant is right and the detector can be accurate. The other three laps
are not.

## The failure is optical/threshold, not mechanical

Two candidate explanations for undercount: the wheel isn't turning (slip,
lift, bounce — worse at speed) or the detector isn't seeing spokes that are
there. The raw waveform separates them. Classifying every 96 ms packet:

| Lap | median intra-packet raw p2p | flat (wheel not modulating) | **modulated but zero rises** | error |
|---|---:|---:|---:|---:|
| 2 | 1017 | 27.2%\* | **16.9%** | −0.9% |
| 1 | 321 | 26.6%\* | **25.8%** | −15.9% |
| 3 | 1369 | 12.3% | **29.4%** | −19.2% |
| 4 | 434 | 5.0% | **39.2%** | −39.3% |

\* laps 1–2 include station dwell, which is legitimately flat.

The "modulated but zero rises" column — the waveform is plainly showing spoke
structure and the detector emitted nothing — rank-orders **identically** to the
distance error across all four laps. The wheel was turning. The detector was
missing it.

## Mechanism: merged spokes, exactly the IR_SCOPE hypothesis

`2026-08-09_IR_SCOPE_BUILD.md` predicted this failure: the signal enters a
spoke normally, but the inter-spoke trough never falls below `thrLow`, so the
detector never re-arms and swallows the next spoke. In-pulse duty cycle is the
direct test:

| Lap | in-pulse duty | mean pulse width | contrast-valid | error |
|---|---:|---:|---:|---:|
| 2 | **29.0%** | 25.2 ms | 76.6%\* | −0.9% |
| 1 | 41.2% | 45.0 ms | 76.7%\* | −15.9% |
| 3 | 41.9% | 24.3 ms | 97.1% | −19.2% |
| 4 | **54.9%** | 35.3 ms | 96.0% | −39.3% |

\* again depressed by station dwell.

Duty cycle rank-orders exactly with error. Lap 4's detector spent 55% of the
lap latched inside a "pulse". Width confirms it: lap 3 ran at roughly twice
lap 2's speed yet reported a *wider* mean pulse (24.3 ms vs 25.2 ms) where
physics demands about half — i.e. each reported pulse is spanning ~2 spokes.
Lap 4's 35.3 ms at speed spans roughly 3.

## What drives it: speed and per-mount contrast, not inside/outside

- **Speed dominates.** Same sensor side, same detector: lap 2 slow = −0.9%,
  lap 3 fast = −19.2%. Faster spokes give the trough less time to reach
  `thrLow` before the next spoke arrives.
- **Contrast matters independently.** At matched speed and direction (laps 3
  vs 4, run back to back), the mount with 3× less signal (median p2p 434 vs
  1369) doubled its error (−39.3% vs −19.2%).
- **Inside vs outside does not explain it.** Mapping mount side against
  travel direction: lap 1 outside (−15.9%), lap 2 outside (−0.9%), lap 3
  inside (−19.2%), lap 4 outside (−39.3%). No consistent pattern. The
  operator's expectation that the inside-facing mount would face into the
  north-side sun held up optically — lap 3 had the *best* contrast of all four
  laps (p2p 1369) — but it still lost 19% because it was running fast. More
  light helped the signal, consistent with the morning's shade/sun result;
  it did not rescue the threshold logic.

## The safety-relevant finding: the undercount is silent

Laps 3 and 4 reported **96–97% contrast-valid**, 1–2 latch discards, and 0–1
contrast-loss events across the whole circuit. By every health signal the
firmware exposes, those laps looked fine. They were off by 19% and 39%.

There is currently **no counter, flag, or telemetry field that would let a
consumer detect this condition.** An integrator trusting `speed_valid` or the
contrast gate would have accepted a 39% distance error without warning. This
is the false-confidence failure class that decision 0043 (admission favors
recoverable omission over false inclusion) exists to prevent, and it argues
directly against IR admission to any distance or speed authority in its
present form.

Candidate detectors, from this evidence and cheap to add: in-pulse duty
cycle (29% good vs 55% failing), and mean pulse width compared against the
interval between pulses (a pulse occupying more than roughly a third of its
own interval implies merging). Neither is currently published.

## Correction to the previous record

`2026-08-26_IR_CW_CCW_LAP_COMPARISON.md` inferred that Toby's `mm` fields were
"a fixed position-index scale, not physical distance." Half right: they are
**mile marker indices**, which the firmware states outright at
`QUORUM.ino:2703`. The inference was correct; describing it as "worth fixing
the field name" was not — the name is documented in the source and the
convention is deliberate. That record also left the lap-1-vs-lap-2 gap
unresolved and floated latch discards as a candidate cause; latch discards are
now measured to account for only ~1 m of the 7.85 m gap and are not the cause.

## What not to conclude

- Do not conclude the IR wheel sensor is unusable. Lap 2 hit 1% against
  surveyed truth. The hardware and wheel constant are sound.
- Do not fix this by raising the falling fraction blindly. `IR_SCOPE_Replay.py`
  exists to choose that threshold from recorded waveform, and the four
  captures here are exactly its input. Run the replay before changing
  `thrLow`.
- Do not treat 0.30 m/s as the ceiling. The speed at which merging begins was
  not bracketed; only two speeds were tested. The onset could be well below
  0.30 m/s.
- Do not read the lap 4 numbers as a navigation failure on Toby's part. Toby
  went `NO_QUORUM` partway through lap 4 and dead-reckoned the remainder, so
  its own position for that lap is less well attested — but the IR distance
  comparison uses the surveyed `spacingMm` total, not Toby's live position, so
  the −39.3% figure does not depend on it.

## Next

1. Run `IR_SCOPE_Replay.py` over all four captures to find a falling fraction
   that recovers seven pulses per revolution at 0.30 m/s without introducing
   false edges.
2. Bracket the speed onset: laps at ~0.20 and ~0.25 m/s with a fixed mount.
3. Publish duty cycle and pulse-width-vs-interval in the IR telemetry so this
   condition is detectable in the field rather than only in post-hoc replay.
4. Investigate why mount contrast varied 3× between physically similar
   installations (p2p 321 / 434 vs 1017 / 1369) — geometry, alignment, or
   surface condition.
