# QUORUM TRACE — passive Hall-waveform + navigation-decision recorder

**Status: INVESTIGATORY / UNAPPROVED.** Built, compiled (both trace OFF and
trace ON, both locomotive profiles), and host-tested. **Never flashed, never
run.** No firmware on either locomotive has been changed by this work until
an operator explicitly flashes a build compiled from it. This document
covers what the capability is, its boundaries, and how to use it once
flashing is approved — it is not itself that approval.

---

## Purpose

Establish the reliable control case (Toby) before comparing Otto's
ghost-driven navigation failures. QUORUM already runs the Hall detector and
the full navigation decision path (`detectorSample()`, `navOnMarker()`,
`acceptEvent()`, the QUORUM evidence-ring/scoring/adoption machinery); this
adds a passive tap that records, synchronized on one timeline, exactly what
QUORUM saw and exactly what it decided — without adding a second sampler,
without touching detection or navigation logic, and without giving the
recorded data any way to influence either.

## Evidence classes

Four kinds of record, on the wire and in the decoded CSV, kept apart the
same way `HALL_WAVEFORM_TEST`'s `CAPTURE_FORMAT.md` keeps its three apart:

| class | column prefix | what it is |
|---|---|---|
| physical measurement | `phys_*` | QUORUM's own averaged Hall reading (`readAveragedADC()`), its adaptive baseline — the exact values `detectorSample()` computed, copied outward, never re-measured |
| detector interpretation | `det_*` | event-open state, opening pole, running peak — `detectorSample()`'s own state, not re-derived |
| navigation decision | `dec_*` | one record per decision point: event opened/closed/floor-rejected, `navOnMarker()` entry, the timing-gate result, `acceptEvent()`, AGREE/DISAGREE, every QUORUM evaluation/adoption/no-quorum event |
| motor context | `ctl_*` | PWM (actual+commanded), direction, E-stop — rides with both samples and decisions |
| operator anchor | `op_*` | **format defined, no producer yet** — see "Anchor mechanism" below |

Full field-by-field wire layout: `QuorumTrace.h`'s own doc comments are the
authoritative definition; `tools/qt_format.py` mirrors it, and the host
tests (`firmware/QUORUM/tests/test_quorum_trace.cpp`,
`firmware/QUORUM/tests/test_qt_decoder.py`) assert struct sizes and CRC
agreement between them, so a drift fails the test run rather than
corrupting a capture — same discipline as HWT1.

Thresholds (north/south enter/exit) are **not** repeated on every sample:
under this firmware they are always `baseline ± a compile-time constant`
(`recomputeThresholds()`), so a `STATUS` record carries
`HALL_DEADBAND_COUNTS`/`HALL_ENTRY_MARGIN_COUNTS` once and a decoder
reconstructs every threshold exactly, not approximately. That is the "as
can be preserved compactly" in the spec, not a missing field.

## Passive / no-authority boundary

Every trace function is called **from** the Hall/navigation path; none of
them is ever called **by** it, and nothing in `detectorSample()`,
`navOnMarker()`, `acceptEvent()`, or any other control-path function ever
reads a value this file writes. Concretely:

- Every value carried into a trace record is a **copy** read after QUORUM
  already decided it — never a value QUORUM reads back.
- The Hall task (`hallTask()`, core 0, priority 2 — must never be late)
  only ever appends a fixed-size record to a preallocated, bounded ring
  (`QtSampleRing`/`QtDecisionRing` in `QuorumTrace.h`). It performs no UDP,
  no MQTT, no `Serial` formatting, no `analogRead()` beyond the one QUORUM
  already made, no dynamic allocation, no blocking call. Verified by static
  audit (`test_quorum_trace_authority.py`, requirement 3) and by the ring
  engine's own host tests.
- **Only `networkTask()`** (already the sole owner of the radio for MQTT)
  opens the trace UDP socket and sends. No new task was created.
- Losing every trace record, at any time — ring overflow, a dropped UDP
  packet, the receiver never running — changes nothing about what QUORUM
  does. The ring-overflow counters (`cumSampleRingDrops`,
  `cumDecisionRingDrops`) are diagnostic-only, exactly like QUORUM's own
  pre-existing `queueDrops`/`floorRejects`/`pubDrops`.
- Proven by static audit (`test_quorum_trace_authority.py`'s requirement-8
  checks): no trace function writes `commandedPwm`, `actualPwm`,
  `estopped`, `navMm`, `navState`, `navDir`, `motorDirection`,
  `sessionDir`, `autoRunning`, or `autoEnrolled`, calls a motor-pin write,
  or calls back into any control-path function (`acceptEvent()`,
  `navOnMarker()`, `applyDirection()`, `requestPwm()`, `navDeclare()`).
- One trace-only exception, and it is deliberately narrow: `navDir` is not
  `volatile` today (nothing else reads it cross-core), so a trace-only
  `volatile int8_t qtNavDirMirror` shadows it, written only at `navDir`'s
  own one assignment point inside `applyDirection()`. The mirror is
  read-only from the Hall task's side; `navDir` itself is never touched by
  any trace code.

## Compile flags

Gated by `QUORUM_TRACE`, undefined (OFF) by default. Every `QT_*` call
site in `QUORUM.ino` reads as an unconditional-looking single line and
expands to nothing at all when the flag is off — compile-time elimination,
not a runtime branch (see `QuorumTrace.h`'s macro layer, and requirement-1
of the static audit).

```bash
# trace OFF (default — this is what ships today)
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/QUORUM

# trace ON
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-property "compiler.cpp.extra_flags=-DQUORUM_TRACE" firmware/QUORUM
```

Both compiled clean (`--warnings all`) for both locomotive profiles this
session — see "Compile results and resource cost" below.

## Toby build selection

`firmware/QUORUM/LocoConfig.h`'s active `#include` currently selects
**Otto** (`LL_LocoConfig_9950011.h`) — this is the committed, tracked state
of the repository (matches `git log`, not a stray local edit), and this
task did not change it. Its own comment block is stale in an unrelated way
(says "TARGET: Toby (9950012)" and references a pre-QUORUM sketch name,
`LL_Auto_r22.ino` — worth a separate cleanup, not touched here).

To build for Toby, change the one active line:

```c
// firmware/QUORUM/LocoConfig.h
#include "LL_LocoConfig_9950012.h"     // Toby
```

This session verified a Toby build (temporarily, compiled, then reverted
`LocoConfig.h` back to its exact committed state via `git checkout`) —
confirmed via the compiled `.elf`: the string `9950012` is present,
`9950011` is absent, `HALL_POLARITY_INVERTED` resolves to Toby's `false`
(`LL_LocoConfig_9950012.h:16`), and Toby's `HALL_DEADBAND_COUNTS=25` /
`HALL_ENTRY_MARGIN_COUNTS=13` (identical numeric values to Otto's, per
that file's own comment: measured with the NGR Hall Probe 2026-06-26 and
unchanged since) are what the trace's `STATUS` record will carry when
flashed. **This task did not alter either profile header** — confirmed by
`git diff` and by a dedicated regression test
(`test_quorum_trace_authority.py`, requirement 10).

### The dead `HALL_POLARITY_INVERTED` symbol

Confirmed, independent of this task (matches `docs/CLAUDE.md`'s own note):
`HALL_POLARITY_INVERTED` is defined in both loco profiles (`true` for
Otto, `false` for Toby) but **no code in `QUORUM.ino` reads it** — QUORUM
decides polarity purely from which threshold was crossed
(`evOpenPole=(raw>=northEnter)?1:0`). Grepped fresh this session; still
dead. The trace does not treat it as authoritative for anything — it isn't
recorded at all, because it has no run-time effect to record.

**Actual polarity convention** (the thing that matters): opening pole is
whichever threshold the averaged reading crossed first —
`raw >= northEnter` → `N` (1), `raw <= southEnter` → `S` (0). This is a
measured fact about the code, not a config value, and it is the same for
both locomotives regardless of `HALL_POLARITY_INVERTED`.

## Receiver and decoder commands

```bash
python3 tools/qt_receiver.py --outdir ~/NGR/qt_logs --port 47700
python3 tools/qt_decode.py ~/NGR/qt_logs/qt_20260824_193000.qtcap -o run.csv
python3 tools/qt_plot.py run.csv --save run.png
python3 tools/qt_plot.py run.csv --start 12 --duration 5
```

The receiver is **listen-only** — there is no command channel, because
there is no anchor mechanism to command yet (see below). It writes every
datagram verbatim, in arrival order, to a capture file exactly like
`hwt_receiver.py` does, so a bug in decoding can never damage the evidence.

## Anchor mechanism (not implemented — design surfaced per instruction)

Operator anchors are the only ground-truth position statement HWT1's own
design relies on, and QUORUM TRACE's format reserves a record type for
them (`QT_REC_ANCHOR`, shape mirrors HWT1's: id, the sample it names,
timestamp, motor context, free text) so wiring a producer in later needs
no wire-format version bump. **No producer exists in this build.** Both
plausible mechanisms add an external interface, which the task instructed
be surfaced rather than silently implemented:

- **Option A — new MQTT command topic**, e.g. `ngr/loco/<id>/cmd/trace_anchor`.
  Symmetric with the existing `cmd/force_lost` diagnostic-fixture topic:
  goes through the same `cmdQueue`/`serviceCommands()` path already proven
  safe for a diagnostic command that must not race locomotive state.
  Cheapest to operate (same MQTT session the operator already has open on
  the Pi console) but is one more permanent topic on the shared broker,
  live even when `QUORUM_TRACE` is off (the topic would need its own
  `#ifdef` guard around the subscribe, or it always exists and only does
  something when tracing is compiled in).
- **Option B — a separate UDP command port on the locomotive**, mirroring
  HWT1's own `--loco`/`--loco-port` pair (receiver forwards operator lines
  to a small listener on the locomotive). Fully outside MQTT/Flask — never
  touches the broker or the console — but is a new listening socket on the
  locomotive that has to be reasoned about the same way the trace-send
  socket was (who owns it, what thread services it, what happens if a
  malformed datagram arrives).

Recommendation for a future task, not a decision made here: **Option A**,
gated by `#ifdef QUORUM_TRACE` around the subscribe/handler so it does not
exist at all in a trace-off build, keeping it symmetric with how every
other diagnostic-adjacent command already works. Do not implement either
without operator sign-off — this section exists to make the choice
visible, not to pick for you.

## Capture procedure

1. Confirm the receiver is running on the Pi (or a laptop on the railway
   network) before power-up: `python3 tools/qt_receiver.py --outdir ...`.
2. Power up, enlist AUTO, declare start position, GO — exactly the
   existing operator workflow. Tracing adds no new step here.
3. Run the lap(s) per the Toby test protocol below.
4. Stop capture (Ctrl-C on the receiver) after the locomotive is stopped
   and safe.
5. Decode and review before drawing any conclusion:
   `qt_decode.py`'s printed report first (completeness, ring-drop counts,
   transport gaps), then the CSV / plot.

## Integrity interpretation

`qt_decode.py`'s report separates, exactly like HWT1's:

- **`cum_sample_ring_drops` / `cum_decision_ring_drops`** — this trace's
  own ring overflowed because `networkTask` could not drain it fast enough
  (weak WiFi, MQTT congestion). Diagnostic-only; does not mean QUORUM
  missed anything, only that the trace of it did.
- **`cum_hall_queue_drops` / `cum_floor_rejects`** — QUORUM's own
  pre-existing counters, carried in `STATUS` for correlation. These
  describe QUORUM's detector, not the trace.
- **transport gaps** — reported **per record-type stream** (`SAMPLE`,
  `DECISION`, `STATUS`, `ANCHOR` each have their own `batchSeq` space; see
  `QuorumTrace.h`'s `QtHeader.batchSeq` comment for why a single shared
  counter was not used here, unlike HWT1). A `SAMPLE`-stream gap does not
  imply a `DECISION` was lost, and vice versa — check each independently.
- A `SAMPLE`/`DECISION` completeness percentage is **not** a claim about
  QUORUM's detection accuracy, exactly as `hwt_decode.py`'s own report
  states for HWT1: it describes what the *trace* recorded, not whether
  QUORUM read the track correctly. **QUORUM's own position and event
  counts are never treated as ground truth by this tooling** — see
  `docs/QUORUM_NAV_AUDIT_AND_GATE_PROTOTYPE.md` §E for the same principle
  applied to the earlier gate-replay prototype.

## Compile results and resource cost

All four combinations compiled clean this session (`arduino-cli
--warnings all --clean`; toolchain: `esp32:esp32 3.3.11`, confirmed
available).

| build | flash | flash % | RAM (globals) | RAM % | new warnings |
|---|---|---|---|---|---|
| Otto, trace OFF | 982,847 B | 74% | 52,452 B | 16% | 0 |
| Otto, trace ON | 990,247 B | 75% | 77,532 B | 23% | 1 (see below) |
| Toby, trace OFF | 982,783 B | 74% | 52,452 B | 16% | 0 |
| Toby, trace ON | 990,191 B | 75% | 77,532 B | 23% | 1 (see below) |

Trace-OFF byte counts are **identical** whether or not the trace-only
local-variable captures are present in source — confirmed by comparing a
build before and after wrapping those captures in `#ifdef QUORUM_TRACE`;
both produced exactly 982,847 bytes for Otto. That is compile-time
elimination demonstrated, not asserted.

**Delta (ON − OFF):** +7,400 B flash (Otto) / +7,408 B (Toby) — about 0.7
percentage points. **+25,080 B RAM**, both profiles — about 7.7 percentage
points, still leaving 250,148 B free for locals (down from 275,228 B).
Dominated by the two rings: `QT_SAMPLE_RING_BATCHES` (16) ×
`QT_SAMPLE_BATCH` (90) × 16 B/sample ≈ 23 KB, plus the 64-slot decision
ring (~2.6 KB) and small fixed overhead. Both are `#define`s in
`QuorumTrace.h` and can be tuned down if RAM pressure ever matters more
than buffering depth (currently ~1.6 s of sample buffering at 1 kHz, ample
against `networkTask`'s ~200 Hz drain cadence).

**New warnings, both explained:**
1. One instance of `'++' expression of 'volatile'-qualified type is
   deprecated` on `qtUdpSendFailures++` — the *same, already-accepted*
   warning class this codebase already carries on 8 other `volatile`
   counters (`queueDrops`, `floorRejects`, `pubWindowCount`, `cmdDrops`,
   `actualPwm`). Not a new class of issue; left consistent with existing
   style rather than "fixed" in only the new code.
2. A multi-line-comment warning from an early draft of this document's
   compile-command example inside a `//` comment block — fixed (reformatted
   to avoid a trailing backslash on a comment line).

Timing: not independently measured this session (would need a flashed,
running locomotive). What IS established: `hallTask()`'s only addition is
one `QT_SAMPLE_TICK()` call per existing 1 ms tick, which does arithmetic
and a `memcpy`-scale struct write into a preallocated slot — no ring push
on most ticks (only every 90th, when a batch completes). `networkTask`'s
drain is bounded (≤2 sample batches, ≤8 decisions, ≤1 status per ~5 ms
pass) so it cannot starve `mqtt.loop()` any more than the existing
marker/status drains already bound themselves.

## Known limitations, honestly

- **Never flashed, never run.** No claim about real cadence, ADC timing,
  WiFi/UDP behaviour, or actual RAM headroom under a live WiFi+MQTT+trace
  load is made here — that is exactly what a flashed, running capture
  would measure and report.
- **`navDir`'s mirror has the same latency as `navDir` itself** — both are
  updated at the same point, but a sample tick between that update and the
  next could theoretically read either the old or new value on real
  hardware, exactly as `commandedPwm`/`actualPwm`'s existing volatile
  cross-core reads already accept. Immaterial at 1 kHz against
  direction changes that happen at most a few times per session.
- **Sample/decision cross-stream alignment** (spec requirement 6) is
  verified structurally (both use `millis()`, both stamp `loco`/`session`
  identically) but not against a real capture — that needs a flashed run.
- **The anchor mechanism is not implemented** (by instruction — see above).
  Without it, a trace capture has the same "cannot attribute a waveform to
  a specific magnet without an operator anchor" limitation HWT1's own
  README states, and for the same reason.
- **Requirement 7's end-to-end proof is partial.** `QUORUM.ino` has no
  host-testable navigation core (confirmed again this session — still no
  `.py`/`.sh`/`.cpp` runner for `firmware/QUORUM/tests/fixtures/*.replay`
  anywhere in the repository), so a wrong-polarity accepted event's trace
  output cannot be produced by actually running QUORUM's real
  `acceptEvent()`/`navPublishState()` on the host. What IS proven: (a) a
  host test constructing the exact record a wrong-polarity accept would
  produce and confirming it survives the ring/wire format intact
  (`test_quorum_trace.cpp`), and (b) a static audit confirming the two
  real call sites (`qtDecisionAccept()`, `qtDecisionNavState()`)
  unconditionally assign every required field
  (`test_quorum_trace_authority.py`). Building a host-runnable QUORUM
  navigation core would resolve this fully; it is a materially larger,
  separate undertaking, explicitly out of this task's scope ("this task
  observes existing QUORUM behavior; it does not change it").
- **RAM/timing numbers above are compile-time facts, not field
  measurements.** Report them as such.

## Explicit prohibition

**Nothing in this document, this capability, or its compile-clean status
is flashing or operating approval.** Flashing either locomotive, or
running the Toby test protocol below, requires the operator's explicit
go-ahead, session by session, exactly like every other firmware change in
this repository.

---

## Toby test protocol (prepared, not executed)

For operator review and approval before any flash or field run.

- **Build:** QUORUM (this branch), Toby profile
  (`LL_LocoConfig_9950012.h`), `QUORUM_TRACE` **ON**.
- **Mode:** QUORUM Auto (enlist, GO) — normal operating procedure,
  unchanged.
- **Extent:** one short, continuous run.
- **Direction:** one direction only for this run (repeat separately for the
  other direction on a later, separately-approved session if wanted).
- **Speed:** Toby's reliable normal operating speed (`CRUISE_PWM`, 90 —
  unchanged by this task).
- **Starting interval:** an exact, operator-confirmed marker interval
  (e.g. via `cmd/start_interval`, `AAA-BBB` at a marker the operator is
  physically standing at) — not inferred from a filename or a prior
  session, per the standing evidentiary requirement from
  `docs/QUORUM_NAV_AUDIT_AND_GATE_PROTOTYPE.md`.
- **Next marker:** noted explicitly before departure, so the first
  `DECISION` record in the capture can be checked against a stated
  expectation, not just QUORUM's own map.
- **Physical direction convention:** operator states which physical
  direction (by landmark, e.g. "toward Grillers") corresponds to the
  session's `CW`/`CCW` selection, in the operator's own words, recorded
  alongside the capture (not inferred).
- **Landmark anchors:** several operator-confirmed landmarks called out
  during the run (verbally / written down with a timestamp) even without
  a wired anchor mechanism — cross-reference against the decoded CSV's
  `t_ms` afterward. This is a manual substitute until Option A/B above is
  implemented and approved.
- **No deliberate stalls, reversals, or assistance.** The point is the
  reliable case; introducing an artificial failure defeats it.
- **Any unplanned stop, hesitation, or uncertainty is recorded
  immediately** — what happened, where, and the wall-clock time, so it can
  be found in the capture afterward.
- **Stop condition:** operator's own judgement, as always; E-stop remains
  fully available and unaffected by tracing (confirmed by the requirement-8
  audit: no trace function reads or writes `estopped`).

This protocol is ready for the operator to approve, adjust, or schedule.
It is not scheduled, and nothing above authorizes running it.
