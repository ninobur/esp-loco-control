# NAVI proposal — native IR observation when IR becomes local

Date: 2026-10-01
Status: **Architectural proposal for a future build. Not a decision.**
Proposed by: David and Sam.
Analysis and recording: Claude, at David's request.

This document authorizes no firmware change, test change, change to current IR
behavior, flashing, or deployment (`/AGENTS.md` section 2). It does not amend
any decision record. In particular it does **not** reopen
[0116](decisions/0116-ewo-ir-distance-is-authoritative-and-hall-only-navigation-is-withdrawn.md)
(IR authority, Missed Magnet, Hall-only navigation, IR health) or
[0111](decisions/0111-ir-is-a-normal-navigation-sensor-navi-decides-applicability-degraded-operation-stays-inside-navi.md).
The present separate IR car, with TX → ESP-NOW → locomotive, is unchanged.

Companion proposal, independently reviewable:
[`NAVI_EWO_HALL_ORDINARY_TRACK_PROPOSAL_20261001.md`](NAVI_EWO_HALL_ORDINARY_TRACK_PROPOSAL_20261001.md).

Naming note: the request suggested `docs/proposals/`. The repository has no
such directory; existing proposals sit in `docs/` as `NAVI_*_PROPOSAL_<date>.md`,
so this file follows that convention.

---

## 1. The proposal (David and Sam)

EWO's governing principle is **observation → NAVI judgment**. The Hall path
follows it: native Hall ADC observations reach NAVI and NAVI owns their
meaning. The IR path does not, because the sensor sits on a separate ESP32
which converts the optical signal into conclusions (transitions, completed
pulses, cumulative count, nominal distance, optical reason, health counters)
before the locomotive sees it.

When the IR sensor is physically integrated with the locomotive and read by the
same ESP32 that runs NAVI, reconsider the boundary as:

> **IR acquisition observes. NAVI interprets.**

```text
IR sensor
    ↓
native ADC observations + factual acquisition metadata
    ↓
NAVI
    ↓
optical interpretation
    ↓
movement/distance evidence
    ↓
navigation judgment
```

which makes IR consistent with the other inputs:

```text
Hall sensor   → native observations → NAVI interpretation
IR sensor     → native observations → NAVI interpretation
Map           → physical facts      → NAVI interpretation
PWM/direction → locomotive facts    → NAVI interpretation
```

**Acquisition may measure and preserve. NAVI decides what the measurement
means.** Acquisition may still perform what is technically necessary: ADC
reads, accurate timestamps, deterministic sampling, LED sequencing if adopted,
buffering, serial numbers, and normalization or framing that assigns no
physical meaning.

**Why it matters.** An apparent ~20% IR distance discrepancy was first
investigated through the processed pulse/distance stream. Understanding it
required flashing IR_DIAG and inspecting near-native optical behavior (raw ADC,
envelope, span, thresholds, pulse timing, headroom, phase, marginal events,
contrast loss). Physically manipulating the sensor wiring was then shown to
produce large raw ADC excursions and pulse-like behavior with the wheel
stationary. Important evidence can disappear when an upstream subsystem
converts observations into conclusions before NAVI sees them.

**Do not overcorrect.** This is not "send every ADC sample everywhere forever."
With no network boundary between acquisition and judgment, design an efficient
local interface. Diagnostic recording is separate from navigation authority.

**Code modularity is not authority separation.** An `IrDetector` module may
remain useful. The question is whether it gives NAVI evidence and tools, or
independently decides what NAVI is allowed to know.

> Provenance note (`/AGENTS.md` section 7): I found no repository record of
> the ~20% discrepancy investigation or the wiring-manipulation result. They
> are cited here from David and Sam's account. They should be recorded as
> field evidence in their own right; this proposal should not be their only
> record.

---

## 2. Inventory of the present IR processing boundary

Source of truth for this inventory: the repository copy of the current IR
transmitter, `firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`
(**IR_SCOPE_ESPNOW_ACTIVE_TX_1_6_R2**), the shared detector
`firmware/common/IrMovementDetector.h`, `IrMovementContract.h`,
`IrMovementWire.h`, and the consumer in
`firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/` on branch
`codex/ewo-ir-authoritative` at `d0185be`.

**Caveat:** the firmware catalog records 1.6_R2 as flashed to the IR car on
2026-09-23 with bench acceptance pending, and notes "the older local Arduino TX
copy has not been replaced." IR_DIAG was flashed more recently for the
discrepancy work. Which sender was installed for each EWO run should be
confirmed before this inventory is relied on for any specific run.

### 2.1 Two corrections to the premise

1. **The TX already transmits native samples.** Every 1 kHz raw ADC sample is
   broadcast in type-1 packets: 96 samples per packet, about 10.4 packets per
   second, each sample carrying the 12-bit value plus four detector flags in
   its top bits, along with the per-batch envelope and thresholds. The EWO
   consumer **drops them**: `serviceIrIngress()` accepts only packets whose
   length equals the 110-byte type-5 `WireSnapshot`, and counts everything
   else as `irPacketInvalid`. So the native evidence does not vanish inside
   the TX. It is sent, then discarded by the locomotive, and it is unacknowledged broadcast
   with no delivery guarantee. The real transport limitation is that the
   remote raw stream is **lossy and unpaced**, not that it is absent.
2. **Some listed items are already NAVI-owned, or are not TX judgments.**
   - *IR speed* is computed in NAVI (`NaviIntegratedCore::addSpeedPoint`, from
     `completedPulses` and `capturedUs`), not on the TX.
   - *IR health/readiness* is classified in NAVI
     (`ngr_nav::classifyIrInstrument`) from the TX counters and reason.
   - *Inferred added/removed* exist as wire fields but the detector never
     sets them; in 1.6_R2 they are always zero. Likewise `distanceValidated`
     is always 0 and `calibrationId` is 0 ("stays zero until the installed
     wheel is confirmed").
   - *pitchUm / nominalUm* are a configured constant (9.652 mm, from the
     96.52 mm ten-spoke wheel, decision 0022) and its product with
     `completedPulses`. NAVI re-checks that product.

What the TX genuinely decides before NAVI sees anything is the **optical
interpretation**: what counts as a transition, whether contrast is adequate,
whether the signal is stationary, and therefore what `completedPulses` is.

### 2.2 Operation-by-operation classification

| # | TX operation (1.6_R2) | Class | Information lost? |
|---|---|---|---|
| 1 | `analogRead(GPIO34)` at 1 kHz via `vTaskDelayUntil`, core 0 | **Acquisition** | — |
| 2 | `esp_timer` timestamp; sample sequence; late (>250 µs) and missed-slot accounting | **Acquisition** | — |
| 3 | Batching 96 samples, CRC, broadcast; latest-only (`xQueueOverwrite`, depth 1) for the type-5 snapshot at 10 Hz | **Transport** | Raw: lossy delivery. Snapshot: none, because counters are cumulative |
| 4 | Sample-gap detection (>1.5 ms between samples) → counter, detector reset | **Factual transform**, but the reset it triggers is a judgment | Gap fact preserved; detector state discarded |
| 5 | Saturation (raw == 0 or ≥ 4095) → counter, detector invalidated | **Factual transform** (the fact) plus **judgment** (invalidate) | Fact preserved in counter |
| 6 | Rolling 512-sample histogram of `raw >> 4` (16-count bins); 5th/95th percentile envelope recomputed every 50 ms | **Transform with chosen parameters** | Yes: quantized to 16 counts, window and percentiles fixed |
| 7 | Contrast adequacy: span ≥ 120 to set thresholds; span < 300 → `INADEQUATE_CONTRAST` | **Judgment** | Only the reason code survives |
| 8 | Thresholds at 1/3 and 2/3 of span | **Judgment** | Sent in type-1 only (dropped by NAVI) |
| 9 | Re-arm requires an observed low | **Judgment** | — |
| 10 | Hysteresis edge detection → `observedRises`, `completedPulses` | **Judgment** (what a wheel transition is) | Timing of each edge is lost; only counts survive |
| 11 | 2.5 s open-pulse timeout → abort | **Judgment** (time-based) | Only counter |
| 12 | Stationary retention: "proven" low/high plateaus, compatibility margin = ¼ span, ≥ 300 span to prove | **Judgment** (is the wheel stationary, is a quiet plateau still a valid phase) | Only reason and abort counter |
| 13 | Reason state machine (`PRIMING`, `INADEQUATE_CONTRAST`, `SATURATION`, `SAMPLE_GAP`, `SIGNAL_STALE` at 2.5 s since last completion, `REACQUIRING`, `TRACKING`) | **Judgment** | Reason sampled at 10 Hz; transitions between reports are invisible except via counters |
| 14 | `unreliableSamples` = samples while reason ≠ `TRACKING` | **Derived from judgment** | — |
| 15 | `nominalUm = completedPulses × pitchUm` | **Arithmetic on a calibration constant** | None |
| 16 | Types 2/3: Toby CTO observation and marker-interval "fusion" packets, with `STRUCTURALLY_VALID`, `HALL_JUMP` etc. flags | **Diagnostic interpretation**; not consumed by EWO | n/a |

Two smaller observations, reported but not acted on: `DEBOUNCE_US` and
`LATCH_MS` are declared in the TX but unused (the detector uses its own
literals), and the fusion packets hard-code Toby's ID.

### 2.3 A field example of why the boundary matters

0116 records it directly: "Otto's normal stops produced
`INADEQUATE_CONTRAST` … The previous health veto made IR unavailable; the
650-ms fallback then confirmed MM13, MM12 and MM11 from one sustained Hall
field." The upstream optical judgment (row 7/13), not the optical observation,
is what entered NAVI's health logic. 0116 fixed that by making health observe
rather than veto. That was the right fix for the current architecture, but it
shows the pattern: NAVI had to decide how much to trust a conclusion it could
not inspect.

---

## 3. Proposed future boundary (local IR only)

### 3.1 Acquisition layer (not NAVI)

- ADC conversion of the IR pin at a fixed rate, with a monotonic serial number
  and `esp_timer` timestamp per sample;
- schedule-miss and lateness counters (as the Hall task already has);
- LED drive and on/off sequencing **if** modulation is adopted, with the LED
  state recorded per sample. This is acquisition metadata, not interpretation;
- nothing else. Saturation and gaps are derivable from (raw, timestamp)
  and need no separate acquisition verdict.

### 3.2 NAVI-owned interpretation

All rows 4–14 above become judgments **NAVI invokes**, over native samples
NAVI can see:

- whether an excursion is a wheel transition, and whether it completed;
- duplicates/artifacts, including electrical disturbance (the wiring result);
- missed transitions and whether any pulse is inferred;
- contrast adequacy, for a stated purpose (adequacy for counting is not the
  same question as adequacy for stationarity);
- stationary vs. moving optical behavior;
- cumulative displacement, speed (already NAVI's), and confidence.

### 3.3 Where the code lives

`IrMovementDetector.h` is already a header-only, host-testable library with no
I/O. It should **remain a module**. What changes is:

- NAVI calls `sample()` itself, on samples it also retains;
- NAVI can read the detector's intermediate state (envelope, thresholds,
  arm/open state), not just its counters;
- the detector's outputs are **evidence** NAVI weighs, not a gate on what
  NAVI may observe. NAVI may run a second interpretation over the same
  samples, for example a disturbance check, without asking permission from
  the first.

That is the concrete meaning of "modularity is not authority": the same file
can serve either architecture. The difference is who owns the samples and
who may look behind the detector's answer.

### 3.4 Minimum native observation NAVI needs

Per IR sample: `{serial, timestampUs, raw12}`, plus LED state if modulated.
If the IR pin is read in the same task and tick as the Hall pin, serial and
timestamp can be **shared** with the Hall observation, so one record carries
both sensors and their relative timing is exact. That is a benefit the remote
architecture cannot offer. Everything else the TX now transmits is derivable.

### 3.5 What is gained, stated narrowly

Moving the detector does **not** by itself make its judgments better; the same
algorithm in a different place makes the same calls. The gains are:

1. **Visibility.** NAVI and replay can see why a pulse was or was not counted.
2. **Per-sample context.** PWM, direction and Hall are known at each IR
   sample. Today NAVI pairs a 10 Hz snapshot with the PWM at packet receipt,
   a known association risk noted in 0116.
3. **No lossy link** between observation and judgment.
4. **One timebase.** There is no cross-ESP clock relationship to infer.

---

## 4. Resource and timing consequences (estimates, not measurements)

| Item | Estimate | Basis / caveat |
|---|---|---|
| ADC | Two ADC1 reads per ms (Hall GPIO33, IR GPIO34). Both are ADC1, so Wi-Fi's ADC2 restriction does not apply | Reads serialize on ADC1; per-read cost on the installed Arduino core should be measured, not assumed |
| Detector RAM | ≈ 1.1 KB (512 B window + 512 B histogram + state) | From `IrMovementDetector.h` |
| Detector CPU | O(1) per sample + a 256-bin scan every 50 ms | Small, but **unmeasured on the locomotive** |
| Queue | If co-sampled, the Hall queue item grows by 2 bytes; otherwise a second 1 kHz queue (the Hall queue is 256 deep) | Co-sampling is cheaper and gives exact relative timing |
| Hall 1 kHz risk | Low if the detector runs in NAVI's consumer loop, **not** inside `hallTask`. The acquisition task should stay acquisition-only, as it is for Hall now | Verify with the existing late/miss counters on a bench build |
| Recorder / telemetry | **The main risk.** Recording native IR at 1 kHz roughly doubles NSR1 native volume | Otto has already needed a diagnostic for Wi-Fi heap pressure from EWO publishing (`codex/otto-wifi-auth-diag`, 2026-09-29) |
| Radio | Removes the IR car's ESP-NOW traffic for movement | Frees airtime; not a locomotive CPU saving |

Sampling rate: the 9.652 mm pitch at 300 mm/s is ≈ 31 transitions/s, so 1 kHz
gives ≈ 30 samples per pulse. 1 kHz looks adequate for unmodulated sensing. LED
modulation, if adopted, would set its own rate and needs separate analysis.

---

## 5. Diagnostic evidence for NSR1 / replay

- **Requirement:** NAVI's IR interpretation must be a deterministic function
  of `(timestamp, raw[, LED])`, so that replay of recorded native IR
  reproduces NAVI's IR judgments exactly. Hall already meets this requirement.
- **Always record (cheap):** each NAVI-accepted transition with its timestamp;
  envelope and thresholds at their 50 ms update cadence; every reason/state
  change with the triggering sample serial; disturbance/artifact rejections;
  schedule-miss counters.
- **Record natively when requested:** full 1 kHz IR samples, either in bounded
  windows (around Hall encounters, reason changes, or rejections) or as an
  explicit recording mode. Which of these is acceptable depends on the
  heap/bandwidth measurement in section 4.
- Keep the NSR1 rule that the recorder observes and never feeds back into
  NAVI.

---

## 6. Reasons this could be inferior to the present preprocessing model

These are real and should be weighed, not treated as objections to be
answered away.

1. **Electrical environment.** The IR car is electrically quiet. A locomotive
   has motor PWM, motor current and a shared supply and ground. The wiring
   result shows the optical front end is sensitive to electrical disturbance.
   Local integration may make the raw signal *worse*. That is an argument for
   native visibility, but also a reason the remote sensor may be the better
   instrument.
2. **Which wheel.** Decision 0022 makes the IR car's plastic ten-spoke wheel the
   target. If a locomotive-mounted sensor observes a **driven** wheel, slip and
   spin produce real optical transitions without equivalent displacement. No
   choice of software boundary fixes that. The target must be an unpowered
   wheel, or slip becomes a new evidence problem.
3. **Fault isolation.** Today a detector bug, overrun or crash on the IR car
   cannot delay Hall acquisition, motor control or E-stop on the locomotive.
   Locally it shares the CPU and failure domain.
4. **Independent instrument workflow.** IR_DIAG and TX revisions can be flashed
   to the IR car without touching locomotive firmware. Locally, every IR
   diagnostic becomes a locomotive flash with its own deployment authority
   (`/AGENTS.md` section 2), and IR experiments couple to NAVI releases.
5. **NAVI complexity and review burden.** NAVI becomes a signal-processing
   system as well as a navigator. Each IR detector change then becomes a NAVI
   change with full review.
6. **Telemetry pressure** (section 4).

None of these argue for keeping the *authority* boundary where it is. Items 1–4
argue for care over *physical* integration and over keeping the detector a
separable, independently testable module.

---

## 7. Unresolved questions

1. Which wheel would a locomotive-mounted IR sensor observe, and is it driven?
2. Electrical noise of the optical front end on a running locomotive: needs a
   bench or track measurement before any boundary design is finalized.
3. LED modulation: adopted or not? It changes the acquisition contract and
   sampling rate.
4. Measured cost of a second ADC1 read and of the detector on the installed
   core, including Hall late/miss counters under Wi-Fi load.
5. NSR1 native-IR recording policy (always, windowed, or on request) given
   Otto's heap pressure.
6. Should NAVI keep a single optical interpretation, or separate purpose-specific
   judgments (counting vs. stationarity vs. disturbance)? The present detector
   fuses them, for example by using a contrast threshold both to count and to
   declare stationarity.
7. Even while IR stays remote, should NAVI consume or record the type-1 raw
   stream it currently discards? This is **not proposed here**. It is noted
   because it would test the proposal's value before any hardware change.
   It is a separate question for David and Sam, and lossy delivery limits it
   to diagnostic use.
8. Should the ~20% discrepancy and the wiring-manipulation result be recorded
   as field evidence (section 1 note)? Claude recommends yes.

---

## 8. Relationship to the Hall ordinary-track proposal

Both proposals apply one principle: when NAVI has observations of the
phenomenon itself, it should judge the phenomenon directly rather than
substitute a proxy or accept another subsystem's interpretation unnecessarily.
For Hall, observe that the field has ended rather than assume it from
distance. For local IR, interpret the optical waveform in NAVI rather than
receiving only a movement conclusion.

There is one practical coupling. The Hall proposal concluded that "sustained
ordinary track" must be measured over travel, which today means IR. A
native-IR NAVI would let that judgment use optical evidence of actual
movement, rather than a pulse count whose stationary/moving classification
was made elsewhere. The two proposals are still independently reviewable and
are kept as separate documents.
