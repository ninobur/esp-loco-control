# QUORUM replay suite

Deterministic regression for QUORUM's adjudication, driven by the 2026-08-10
Otto-tow capture. Nothing here is ever flashed.

```bash
python3 run_suite.py
```

## What it actually runs

`harness.cpp` `#include`s **QUORUM.ino** and compiles it for the development
host against the stubs in `shim/`. The suite therefore exercises the shipped
navigator — `scoreEntry()`, `decideEvaluation()`, the timing gate, adoption and
its validation, `enterNoQuorum()`, `buildNoQuorumSnapshot()` — not a
transcription of it. Publications are read by draining the firmware's own
`pubQueue` / `markerPubQueue`.

Two things in the shim are deliberately real: the FreeRTOS queues are genuine
FIFOs, and `millis()` is host-driven so events replay at their captured timing.

## What it cannot test

The replay begins at `navOnMarker()`. Everything upstream — the Hall ISR,
detector thresholds, baseline tracking, queue-drop behaviour — is out of reach
and must be covered on the bench or in the field. WiFi and MQTT never connect,
by design. Task scheduling and real-time interleaving are not modelled.

Do not read a passing suite as evidence about the detector.

## Fidelity, and its one limit

The full-session replay reproduces the capture exactly:

- **1890 / 1890** marker events land on the same odometer value
- **40 / 40** adjudication decisions match, field for field
- **1885 / 1890** timing-gate verdicts match exactly

The five exceptions are pinned by capture line in `verify_replay.py`.
`pwmCommandedAtDetect` is sampled at event *open* and is never published, so the
fixture recovers it as `publish_ts - durationMs` against the `state/throttle`
timeline. That is exact for ordinary markers, but five events carry multi-second
pulses — all inside the defective mm 66–82 stretch where the detector latches —
whose open instant straddles a throttle change and cannot be recovered from the
capture at all. All five land on the same odometer value and change no decision.
Any *new* divergence fails the suite.

## Layout

| file | role |
|---|---|
| `harness.cpp` | includes QUORUM.ino; command protocol on stdin, JSON on stdout |
| `shim/` | smallest Arduino/ESP32/WiFi/MQTT stubs that let the sketch link |
| `extract_fixture.py` | capture → `full_run.replay` + `.expected` |
| `make_synthetic.py` | cases the capture lacks, with expectations as data |
| `verify_replay.py` | fidelity + decision-sequence comparison |
| `run_suite.py` | the entry point |
| `qrun.py` | shared run/summarise helper |

The capture at `field-records/logs/20260810_IR_SPEED_LOCAL_1_2_otto.log` is the
authority and is never modified.

## Why one capture fixture and not per-incident slices

A slice has to open with a synthetic declaration, and `navDeclare()` resets
last-confirmed, the evidence ring, `markersSinceConfirmed` and the incident
state — so it cannot reproduce the entry conditions of the incident it is named
for. Incident C's evaluation opened at mm 14 carrying ring content and an
adoption history from thirty markers earlier. The full run reproduces all three
incidents with their true entry conditions, and `run_suite.py` asserts each one
individually by capture line and published score vector.

## The synthetic cases

Start positions are **not** arbitrary. Whether a displacement provokes an
incident depends on the DNA under the wheels — an offset only disagrees where
the sequence differs at that lag. Each start was found by sweeping all 171 route
positions through this harness; the sweep counts are recorded in
`make_synthetic.py` so the choice is auditable. For the outside-the-fence cases
only ~40% of starts provoke NO_QUORUM at all; at the rest the displacement
passes unnoticed. That is worth knowing, and is why the position is stated.

`syn_ordinary_recovery` is the control the capture lacks. The run's only
adoption was wrong, so without a case that *must* adopt, a navigator that never
adopts would pass everything else.

## Evidence properties

`run_suite.py` also checks facts about the map itself, independent of firmware,
because the exact-or-silent advisory depends on them:

- every DNA window of length ≥ 10 is unique route-wide (W=9 still has 4
  collisions) — this is what makes an exact 12-window match unambiguous
- incident A's ring matches marker 108 route-wide, and **nothing** within the
  ±5 advisory window
- incident B's ring matches **nothing**, at any width
- incident C's ring matches marker 18, both route-wide and within ±5

A map change that broke any of these would invalidate the advisory, and fails
here rather than in the garden.
