# NAVI_RVM: 70-count excursion gate

2026-10-20 · Claude (Opus 5.5) · Branch `agent/navi-rvm-gated-peer-20261020`

**Operator instruction (2026-10-20).** Add the 70-count requirement and the
polarity map, plus any other data sources the model needs to run reliably.
Then test it at 4 station stops and in two-train operation using minimal NAVI
peer spacing.

**Not flashed. No locomotive was touched.**

## Change

`RvmShape.h` and its Python reference `rvm_local.py` now accept a window as a
candidate only if:

- its shape LLR > 0, **and**
- its principal excursion is **≥ 70 ADC counts** from the local baseline level.

The excursion is measured on the same 20 ms tracing points that the shape is
built from.

- **Applied before holding.** A small wander can neither be accepted nor
  displace a real magnet.
- **Polarity map.** It was already enforced by `RvmNavigator.h`: a recognized
  ensemble advances the count only if its polarity is the route polarity of
  the expected MM. A wrong-polarity ensemble of 70 counts or more remains a
  CONTRADICTION.
- **IR.** It was not added. No recorded failure needs it.

The NAVI_SIMPLIFIED form of the rule used two consecutive 1 kHz samples. The
20 ms-mean form used here is stricter, because a 20 ms mean of 70 counts or
more needs a sustained departure.

## Evidence

**The field record decides the one real failure.** In Run 12 (Otto, CCW,
2026-10-19), the false ensemble that produced the MM101 CONTRADICTION had a
logged `amplitude` of **12.4**. That is the quantity the gate tests, so the
gated recognizer cannot accept that window. The same log gives the amplitudes
of all real claims:

| Run | Locomotive | Real MM claims | Smallest amplitude |
|---|---|---|---|
| Run 12 | Otto | 623 | **104.0** |
| Run 12 | Toby | 18 | **179.3** |

None would be lost.

**Replay on the 11 recorded Otto boots** (Oct 15 boot 1 and the Oct 18–19
runs, through `u10_firmware_parity.py`):

- C++/Python parity is exact on every boot: same block ids, same polarity,
  same navigator events.
- Every decision is **identical with and without the gate**. The gate
  removed no recognition the replays depended on.
- The replays reach CONTRADICTION earlier than the field firmware did on the
  same runs. Run 10/11 contradicts almost immediately against 272/272 in the
  field, and Run 12 at MM103 against 623/623 in the field. The replay "misses"
  coincide with native samples that the Pi relay lost (`nsr_native_drop`).
  The firmware reads the ADC locally and never lost them. **These replays
  therefore understate the firmware**, and they cannot by themselves score
  field reliability.
- A replay of Run 12 declared at MM102 (`last` 102, `next` 101, CCW) gives
  79 consecutive correct MM claims with both builds. The borderline LLR 3.4
  false window did not recur in replay, because its acceptance depended on
  live recognizer state.

## Not established

- Toby's continuity baseline still fails on his motor-shifted Hall (Run 12
  record). The gate does not address it.
- No boot-2 (Oct 15) holdout run yet.
- Station stops and peer spacing are not built.
