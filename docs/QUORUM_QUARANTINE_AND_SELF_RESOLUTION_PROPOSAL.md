# Quarantine the doubtful, and never stop learning

**Status: PROPOSAL. No firmware change. Owed: harness proof, then a decision
record superseding parts of 0023 and 0024.**

Origin: operator, 2026-08-14 — *"we are deliberately making the locos ignore
what they know"* — with CODEX concurring and supplying the failure trace.

Two changes, independent, both removing a place where the firmware discards
information it already has.

---

## 1. The record should come from the next reading with good credentials

### What happens today

Every accepted event is **committed immediately**: the odometer advances, the
polarity enters the evidence ring, and the last-accepted interval updates. If
that event was spurious, every honest marker afterwards is judged against a
record it corrupted. One phantom becomes twenty disagreements and then a
terminal NO_QUORUM.

The only defence is the conservation gate, and it asks **PWM** what interval is
physically plausible — a model decision 0024 established is wrong by 1.56–1.78×
on grades and under load. On 2026-08-14 at 17:21 it failed exactly there.

### The triggering evidence, 2026-08-14 17:21 (Toby, MANUAL, struggling at PWM 61)

| time | mm | obs | peak | duration | interval | ratio | gate |
|---|---|---|---|---|---|---|---|
| 17:21:20 | 23 | S | **56** | **2940 ms** | **420 ms** | 1.58 | ACTIVE — admitted |
| 17:21:21 | 22 | N | 143 | 361 ms | 3008 ms | 1.58 | admitted |
| 17:21:26 | 21 | S | 239 | **5212 ms** | **463 ms** | 1.58 | ACTIVE — admitted |
| 17:21:26 | 20 | N | 131 | 348 ms | 5261 ms | 2.52 | admitted |
| 17:21:33 | 19 | S | 235 | **6233 ms** | **493 ms** | 2.53 | ACTIVE — admitted |

Ordinary events in that stretch: peaks 131–239, durations 296–458 ms.

**Every conservation ratio landed outside the [0.7, 1.3] rejection band**, so
nothing was refused. The model expected ~2.1 s; the locomotive was crawling
far slower, and the gate was measuring the wrong thing.

Note that **two of the three offenders are strong** (239, 235). A flux
threshold alone would have passed them. What condemns them is a five- to
six-second duration against a 460 ms interval — a locomotive dwelling on one
magnet long enough for it to be read twice. Same family as the 2026-07-27
Grillers crawl that corrupted the baseline and produced 55% polarity errors.

### Proposal — provisional acceptance

An event that fails its credentials is **quarantined, not committed**. Nothing
is published as navigation truth until the *next* event arbitrates between two
hypotheses:

- **H-genuine** — the pending event was real, so this new event is the marker
  after it.
- **H-phantom** — the pending event was spurious, so this new event is the next
  genuine marker after the last committed one.

Whichever hypothesis the new event's polarity fits against the map wins. On a
decisive phantom verdict the pending event is discarded from the navigation
record — retained in telemetry for forensics, per the operator's dig policy —
and the credible event is committed in its place.

### Credentials (all self-calibrating — no velocity model anywhere)

| signal | measured against | rationale |
|---|---|---|
| **low flux** | running median of recent accepted peaks | return-flux companions ran 23–33% of median |
| **improbably short interval** | last **accepted** interval | not PWM. This is 0024's replacement direction, finally earning its place |
| **abnormal duration** | running median of accepted durations | catches the crawl artefacts a flux test misses (239 and 235 above) |
| **opposite pole to predecessor** | last accepted polarity | the return-flux fingerprint, 52 of 56 events (decision 0025) |
| **map consistency of the next event** | `NGR_DNA1` | the arbiter, not a heuristic |
| *(later)* IR displacement | independent wheel sensor | when M2 lands |

### Measured on real captures

Applying flux + interval + opposite-pole as a strict conjunction across
10,205 markers of 2026-08-13/14 traffic:

| | Otto | Toby |
|---|---|---|
| markers | 7297 | 2908 |
| rejected | **32** (0.44%) | **1** |
| rejected peak/median, worst | 0.33 | 0.23 |
| **accepted** peak/median, minimum | **0.29** | **0.19** |

**No single criterion separates the populations** — there are genuine markers
dimmer than rejected ones, and genuine intervals shorter. The conjunction is
doing all the work, and that is the design, not a happy accident. A weak
reading is ordinary; a short interval is ordinary; a polarity flip is ordinary.
All three on one event is the fingerprint and essentially nothing else.

Rejections cluster at recurring sites — Otto mm 89 (×4), mm 87 (×3), plus 86,
76, 79, 100, 60. Those are magnets to dig at, exactly as decision 0025 intends
the phantom to be used.

### Why a false rejection is cheap — the operator's argument

*"QUORUM can still recover if it was an accurate reading (which is soooo
unlikely)."* This is the safety case and it is asymmetric:

- **Wrongly discard a genuine marker** → the odometer runs one short. That is
  an offset of −1, inside `QUORUM_OFFSETS`, and the navigator adopts and closes
  it routinely — it did so twice on 2026-08-13 within 23 markers.
- **Wrongly admit a phantom** → the record is corrupted, every later marker is
  judged against it, and the failure mode is a terminal NO_QUORUM the
  locomotive cannot leave.

The costs differ by an order of magnitude, so the bias should be toward
rejection. Today's code is biased the other way.

---

## 2. NO_QUORUM must keep learning

### What happens today

`NAV_NO_QUORUM` is terminal. The odometer keeps advancing and the ring keeps
filling, but **candidate scoring stops entirely**. The published candidate set
freezes at the values it held when the hard bound fired. There is no exit
except an operator declaration.

The HARD_BOUND advisory (decision 0023) runs **once**, inside
`buildNoQuorumSnapshot()`, on the twelve-event ring as it stood at the moment
of collapse — which is to say, on the corrupted evidence that *caused* the
collapse. It correctly finds nothing, and is never asked again.

### The measurement that settles it

Toby, 2026-08-14, after entering NO_QUORUM at believed MM012: he read **141
further markers** in MANUAL. Matching that polarity sequence against
`NGR_DNA1`, route-wide, both directions:

| window | exact matches route-wide | implied position |
|---|---|---|
| last 12 markers | **1** | mm 157 |
| last 20 | **1** | mm 157 |
| last 30 | **1** | mm 157 |
| all 141 | **1** | mm 157 |

One answer. Every window length. Unambiguous. The locomotive had the evidence
to place itself within twelve markers of the failure and sat lost for over an
hour, while Otto — correctly, under decision 0031 — refused to move because its
peer reported unusable position.

### Proposal

Continue scoring in NO_QUORUM. The candidate offsets do not change as the
locomotive advances, so **every later marker is evidence**. When one hypothesis
earns a decisive lead over a long window and is then confirmed by subsequent
markers, return to NORMAL without an operator declaration.

Separately and importantly: **motor policy and knowledge recovery are different
decisions.** Whether AUTO stops during uncertainty is one question; whether the
navigator keeps learning is another. Stopping is right. Refusing to think is
not.

This does not reopen what decision 0023 rejected. That record refused a
*ranking prior* during evaluation — a wide search on doubtful evidence, which is
what killed CTO2. Here the prior is already formally discredited: NO_QUORUM
means we have declared the position untrustworthy. Constraining the search to a
fence around a discredited belief is the one case where the caution buys
nothing. The predecessor knew this — r12's header reserves whole-map W12 search
for *"startup or genuine loss"*. QUORUM kept the startup half and dropped the
other.

---

## What is owed before any firmware

1. Harness fixtures (below) for both changes, run statefully over the complete
   2026-08-13/14 captures, enumerating **every** changed acceptance, rejection,
   adoption and terminal outcome — the same bar decision 0024 set for itself.
2. Threshold selection from measured distributions, not chosen numbers.
3. A decision record superseding 0023 in part (advisory may act, in NO_QUORUM
   only) and 0024 in part (its replacement direction, finally justified — by
   quarantine rather than by the phantom, which decision 0025 had removed as a
   justification).
4. Adversarial cases: maximum genuine acceleration with route-spacing variation;
   a missed marker followed by acceleration; consecutive phantoms; a correctly
   rejected phantom followed by the genuine remainder of that interval; and
   recovery from a short poisoned predecessor.

## References

- `docs/decisions/0023-*` (advisory, diagnostic-only), `0024-*` (the gate
  measures rather than predicts), `0025-*` (phantom as maintenance artefact
  and as diagnostic signal), `0031-*` (fleet stop — why Otto stopped)
- `docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md` — `expectedDt = previousAcceptedDt`
- `field-records/20260813_TOBY_QUORUM_1_13_VERIFICATION.md`
- Capture: `all_20260814.log`, NO_QUORUM 17:28:04, triggering events 17:21:20–33
