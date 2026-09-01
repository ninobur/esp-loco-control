# IR_SCOPE_Replay over the four laps — the replay cannot answer the threshold question from these captures

**Date:** 2026-08-26
**Inputs:** the four lap captures from
`2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md`, converted to IR_SCOPE CSV by the
new `tools/ir_scope_espnow_to_csv.py`.
**Tool:** `firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py`, unmodified.

## Result: validation failed on all four captures

The replay validates itself before any candidate is read: replaying at the
production 1/3 falling fraction must reproduce the firmware's own recorded
rise/fall flags. It did not, on any lap.

| Lap | recorded rises | replay rises | matched | match rate | verdict |
|---|---:|---:|---:|---:|---|
| 1 | 2277 | 2136 | 2107 | 92.5% | WARNING |
| 2 | 2810 | 2725 | 2722 | 96.9% | WARNING |
| 3 | 2089 | 1953 | 1950 | 93.3% | WARNING |
| 4 | 1666 | 1356 | 1349 | 81.0% | WARNING |

The tool's own contract is explicit: *"replay does not reproduce the recorded
detector; treat candidate numbers as suspect."* **The falling-fraction table
it printed is therefore not reportable as an answer, and no threshold change
is recommended from it.** Recording the numbers here without that caveat
would be exactly the survivor-biased reasoning the IR_SCOPE tooling was built
to prevent.

## Why it failed — two causes, both fixable, both already known

**1. ESP-NOW packets carry no mid-batch envelope.** IR_SCOPE's CSV recorded
the envelope with mid-batch recompute offsets. The `IR_SCOPE_ESPNOW_TX`
packet carries a single `runMin`/`runMax`/`thrHigh`/`thrLow` per 96-sample
batch, sampled at batch start, while the firmware recomputes the envelope
every 250 ms. The converter cannot invent the missing offsets, so replayed
thresholds are stale by up to 96 ms and a small number of edges land on the
wrong side. This is a **packet format gap**, not a tool defect.

**2. Transport loss dominates the captures.** Broadcast loss ran 20–27%,
producing 210–531 transport gaps per lap. Every gap opens a `resync_unknown`
span that the replay must exclude from statistics, correctly refusing to
guess the candidate's state after a gap. The excluded-span counts (`unkn`)
were 431 / 382 / 219 / 118 — a large fraction of each capture is
unjudgeable. This is precisely the item already standing open on the
pre-capture punch list from the original IR handoff (duplicate transmission
with Pi-side dedup, or unicast ACK/retry, plus packet sequence accounting).

That punch list is no longer a nice-to-have. It is the blocker on threshold
work.

## What survives: the structure pass

The replay's waveform-structure pass counts physical peaks by prominence
(≥ 0.25 × span) directly on the raw ADC trace. It involves no detector
semantics and no candidate threshold, so it is unaffected by the failed
validation. Comparing peaks against the pulses the firmware actually
completed, within the same received packets:

| Lap | physical peaks | completed pulses | peaks converted | peaks missed | distance error |
|---|---:|---:|---:|---:|---:|
| 2 (CW left, slow) | 3417 | 2949 | 86.3% | 13.7% | −0.9% |
| 1 (CCW right, slow) | 3429 | 2539 | 74.0% | 26.0% | −15.9% |
| 3 (CCW left, fast) | 2859 | 2243 | 78.5% | 21.5% | −19.2% |
| 4 (CCW right, fast) | 3033 | 1888 | 62.2% | 37.8% | −39.3% |

Lap 4 is the clean corroboration: 37.8% of visible spoke peaks never became
pulses, against an independently measured 39.3% distance shortfall, from two
methods sharing no machinery — one is prominence peak-finding on raw ADC, the
other is surveyed track length from `spacingMm[]`.

**Caveat against over-reading this table:** lap 2 shows 13.7% of peaks
unconverted while its distance was accurate to 0.9%. Both cannot be true of
real spokes. The likely explanation is the limit IR_SCOPE documents itself —
peak counting assumes one optical peak per spoke, and per-spoke optical
doubling inflates the peak count. That would put a floor of roughly 13% of
spurious or doubled peaks under every row, which is why this table is
directional corroboration of *rank order*, not a calibrated miss rate.
Resolving it needs the hand-turn revolution-marker protocol
(`--rev-marker`), which none of these captures carry.

`mpk` — completed pulses containing two or more physical peaks, the direct
merged-spoke signature — was 15 on lap 2 against 29 on lap 4 despite lap 4
being the shorter capture, consistent with the merging mechanism established
in the distance-truth record.

## Also missing: revolution markers

Every capture reported `p/rev-mk: ---` — no operator revolution markers, so
the only absolute pulses-per-revolution figure is unavailable. The
`IR_SCOPE_ESPNOW` firmware has no marker mechanism at all (the earlier
`IR_SCOPE` took them over MQTT). Without markers, `p/7pk` stays structural
evidence and cannot settle whether the wheel is optically doubling.

## What is needed before the threshold question can be answered

1. **Transport**: duplicate transmission with Pi-side dedup, or unicast with
   ACK/retry, plus sequence accounting — the standing punch-list item.
2. **Packet format**: carry the mid-batch envelope recompute offsets, as
   IR_SCOPE's CSV did, or recompute the envelope only at batch boundaries so
   the recorded envelope is exact for every sample it covers.
3. **Markers**: an in-band marker mechanism, minimally a revolution marker
   for the hand-turn protocol and start/end/illumination marks.
4. Only then re-run this replay. A hand-turned bench capture with revolution
   markers and no transport loss would settle both the doubling question and
   the falling fraction, and is far cheaper than another field lap.

## What not to conclude

- Do not read the falling-fraction table from this run, in any form. The
  tool declared it suspect and the reasons are understood.
- Do not conclude the replay tool is at fault. It behaved exactly as
  designed: it refused to certify its own output against an input that does
  not meet its contract.
- Do not conclude a threshold change is or is not warranted. That question
  is untouched by this run.
