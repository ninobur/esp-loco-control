# Stage A field verdict — baseline poisoning reproduced on demand

Date: 2026-08-07
Firmware under test: QUORUM_1_7 (the un-gated firmware — required, since
1.8 makes the fault undemonstrable)
Protocol: `QUORUM_1_8_FIELD_TEST_PROTOCOL.md` Stage A
Evidence: `field-records/logs/20260807_A1_prefix-magnet-park.log`
Operator: David. Locomotive: Otto (9950011), Lowline, CCW.

## Verdict: **PASS.** Decision 0017 ratified; QUORUM 1.8 cleared to flash.

The falsifier was stated in advance: *if the parked-on-magnet baseline
holds steady, the diagnosis is wrong and 1.8 is not built.* It did not
hold. It collapsed, on cue, within the predicted time constant.

---

## The reproduction

Control leg first: ~9 minutes of ordinary running, baseline **2017–2024**,
188 markers, 182 AGREE / 3 DISAGREE — including a complete
`QUORUM_OPEN → TIED×4 → ADOPTED → CLOSED` self-recovery. Healthy machine,
healthy reference.

Then parked on an S magnet at 11:54:32, throttle 0:

```
11:54:31  base=2019  raw=2026  d=  +7   rolling, healthy
11:54:32  base=2019  raw=1816  d= -203  stopped ON the magnet
11:54:44  base=2018  raw=1889  d= -129  12 s parked — reference holding
11:55:00  base=2016  raw=1889  d= -127  28 s — still holding
11:55:01  base=2005  raw=1890  d= -115  ← MEDIAN CROSSES OVER
11:55:02  base=2001
11:55:03  base=1970
11:55:03  base=1911
11:55:05  base=1892  raw=1889  d=   -3  reference has BECOME the magnet
```

**2019 → 1892 in four seconds, after a 29-second dwell, locomotive
stationary at PWM 0.** The step shape is the signature of a median rather
than an average: nothing moves until magnet samples take the middle of the
128-slot window, then it tips.

## Predictions vs. observations

| Prediction (spec §1, §5) | Observed | |
|---|---|---|
| Migration ≥ 40 counts (beyond ±38) | **129 counts** | PASS, 3.4× |
| Crossover ~32–64 s | began 29 s, complete 33 s | PASS |
| Long open event during dwell | **`ms=32010`**, obs=S | PASS |
| Inverse-polarity phantom at departure | **obs=N, `ms=8910`**, dt saturated 65535 | PASS |
| Departure stream corrupted | 56 DISAGREE, 33 PHANTOM_REJECTED, 4 QUORUM_OPEN, 3 adoptions | PASS |
| Evidence self-erases | baseline back to **2019** by inspection | PASS |
| Net navigational damage | **45 markers**: dashboard 163, locomotive physically at 117–118 | — |

## One prediction corrected by the data

Spec §1 (self-review R2) predicted the departure stream would be
*polarity-inverted* — every real N delivered as S, every S swallowed. **The
field data does not support that characterization.** Post-departure
polarity was roughly even (83 N / 75 S).

What actually degraded was **event rate**:

| | markers | duration | rate |
|---|---:|---:|---:|
| Healthy leg | 188 | 485 s | 0.39 /s |
| Corrupted leg | 158 | 174 s | **0.91 /s** |

A **2.3× flood** of spurious and misjudged events, not a clean inversion.
The conservation gate caught 33 as phantoms — working as designed — but
enough survived to drive three separate adoptions and 45 markers of
odometer error. The R2 reasoning about `EVENT_EXIT_HOLD_MS` re-opening
events was mechanically plausible and is not disproven as *a* pathway, but
it is not the dominant one. **Corrected in the record rather than
defended.** The core claim — a displaced reference produces coherently
wrong evidence that QUORUM cannot absorb — stands, strengthened.

## Notes for the acceptance stage

- The 32 s open event closed with `gate=LOW_PWM` (PWM 0 at detect), so it
  bypassed the conservation test — consistent with, and unaffected by, the
  1.8 gate.
- The departure phantom carried `dt=65535` (saturated), confirming the u16
  saturation path CODEX asked to have tested is reachable in the field, not
  merely in theory.
- Post-fix rows C3/C4 must show this same park producing a **frozen**
  baseline with a large steady `delta` — that is both the acceptance
  criterion and the genuine-1.8 discriminator.

## Unrelated fault, still open

Throughout the session Otto cycled online/stale, became briefly
unresponsive to controls, and rebooted twice (11:44:04, 11:53:31 by
uptime reset). Earlier the same day a fractured Hall harness wire produced
a decaying quiescent level (2016 → 1509 → 1069 → 0) and phantom markers on
the bench at 46 pKPH while stationary — repaired by rewiring, confirmed by
a steady 2018–2020 afterwards. The connectivity/reboot fault is **not**
explained by the baseline mechanism and is **not** addressed by 1.8. It is
the next investigation. See
`field-records/logs/20260807_otto_hall-signal-collapse.log`.

The bench phantom incident is also the strongest argument yet for decision
0005's motion witness: a stationary locomotive on a patio table generated a
clean stream of markers and a plausible 46 pKPH, and nothing in the stack
could contradict it.
