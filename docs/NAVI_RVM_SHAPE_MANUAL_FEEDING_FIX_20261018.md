# NAVI_RVM_SHAPE_MANUAL: bounded feeding at GO (field fix)

2026-10-18 · Claude (Opus 5.5) · Approved by David: "fix the block-queue overflow
at GO by feeding released blocks to the recognizer in bounded portions between
incoming blocks; verify offline and report before reflashing." **Not reflashed.**

## Defect

Field run, 2026-10-18 evening
([`NAVI_RVM_SHAPE_MANUAL_FIELD_RUN_20261018.md`](NAVI_RVM_SHAPE_MANUAL_FIELD_RUN_20261018.md)):
at GO the arrival hold-back released 240 and 203 blocks to the recognizer in one
step.

- Re-scoring made single RVM steps last 2.7 s and 3.3 s on the ESP32.
- The 128-block Hall queue (2.56 s) overflowed, and 44 blocks were dropped.
- No magnet was affected that evening.

## Change

| File | Change |
|---|---|
| `RvmPause.h` | Released blocks are **not** handed over at once: they wait, in order, at the front of the same 512-block ring. A block that joins the waveform while a backlog exists goes behind it. `drainOne()` feeds the oldest waiting block. A declaration still feeds everything first. If the ring is full, its oldest block is fed at once. No new memory |
| `NAVI_RVM_SHAPE_MANUAL.ino` | `rvmTask` takes every queued Hall block first, without waiting while a backlog exists. It feeds **one** waiting block per pass, and only while no new Hall block is queued. `state/loopstat` adds `rvm_backlog` and `rvm_backlog_max` |
| Tests | `unit.cpp` adds backlog order, partial draining, a full ring and the declaration flush. The STOP/GO unit cases now feed one waiting block per incoming block. `replay.cpp` and `blocks_replay.cpp` take a drain rate |

Not changed: recognizer, model, local baseline, pause/resumption/arrival rules,
navigator, and the motor and safety paths.

## Verification (offline)

| Check | Result |
|---|---|
| Boot 1, firmware path, released blocks fed at once (`s21_feeding_parity.py`) | 1,092 = Python 1,092, identical centres and polarity, identical navigation events |
| Boot 1, firmware path, **one waiting block per incoming block** (worst schedule; backlog reached 511) | **identical** to the above |
| Required STOP/GO cases (`s18`) | 864/864 counted once, MM sequence correct, 0 CONTRADICTION; C++ = Python in all 1,260 runs |
| Unit checks, authority check | pass |
| Build (real credentials) | **982,499 B flash (74 %)**, **121,292 B static RAM (37 %)**: memory unchanged |

The recognizer sees exactly the same blocks in exactly the same order. Only
*when* it processes them changes, so recognition results cannot differ.

## What to expect on Otto

- **Queue safety.** The Hall queue no longer has to absorb a release burst.
  `block_q_drop` should stay 0.
- **Latency.** After a GO with a large hold-back, recognition runs behind real
  time until the backlog is fed. `rvm_backlog` shows it shrinking.
- **CPU limit.** If the ESP32 cannot keep up with the waveform on average, the
  backlog would grow to the ring size, and then the oldest blocks would be fed
  synchronously again. `rvm_backlog_max` would show that.
- **What is not fixed.** A *single* recognizer step can still be long during
  slow running: 0.16–0.49 s observed, from re-scoring wide windows after an
  acceptance. That is within the 2.56 s queue. Reducing the per-step cost is a
  separate change and was not made.
