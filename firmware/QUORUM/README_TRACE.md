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
| operator anchor | `op_*` | ground truth the operator states in the moment (`ngr/loco/<id>/cmd/trace_anchor`) — see "Anchor mechanism" below |

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
  `cumDecisionRingDrops`, `cumAnchorRingDrops`) are diagnostic-only,
  exactly like QUORUM's own pre-existing `queueDrops`/`floorRejects`/`pubDrops`.
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
- The operator-anchor mechanism (`qtSubmitAnchor()`, `qtPublishAnchorAck()`,
  and `handleCommand()`'s `T_CMD_TRACE_ANCHOR` branch) is held to the same
  discipline, extended with one more requirement (static audit
  requirement 12): it also never writes `evRing`/`evRingLen`/`evRingHead`
  (QUORUM's evidence ring). It runs on the loop thread, the same thread
  that owns navigation/motor/command state — but only ever reads three of
  those values (`navDir`, `actualPwm`, `commandedPwm`) to copy them
  outward, exactly like every other trace function; it writes none of
  them. No control-path function ever reads anchor state back
  (requirement 13) — an anchor being late, dropped, or never sent changes
  nothing about what QUORUM does, the same as losing every sample and
  decision record would.

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

Both sessions that touched this capability verified a Toby build the same
way: temporarily switch the one active `#include` above, compile, verify,
then revert `LocoConfig.h` to its exact committed state via `git checkout`
— confirmed via `git diff` producing no output. **The tracked selector
stays Otto**, deliberately: Otto is the field-operating baseline (see
`firmware/README.md`'s catalog), and this capability's own purpose —
preparing evidence ahead of comparing Otto's navigation to Toby's — does
not warrant changing which profile a plain from-source compile targets by
default. The stronger guard against an accidental Otto-onto-Toby flash is
not a header toggle at flash time; it is the preserved, hash-verified Toby
artifact itself (see "Toby trace-ON binary identity" below) — flash that
exact file, not a fresh recompile.

Confirmed via the compiled `.elf` (both sessions): the string `9950012` is
present, `9950011` is absent, `HALL_POLARITY_INVERTED` resolves to Toby's
`false` (`LL_LocoConfig_9950012.h:16`), and Toby's `HALL_DEADBAND_COUNTS=25`
/ `HALL_ENTRY_MARGIN_COUNTS=13` (identical numeric values to Otto's, per
that file's own comment: measured with the NGR Hall Probe 2026-06-26 and
unchanged since) are what the trace's `STATUS` record carries. **Neither
session altered either profile header** — confirmed by `git diff` and by
a dedicated regression test (`test_quorum_trace_authority.py`,
requirement 10).

### Toby trace-ON binary identity

The exact Toby, `QUORUM_TRACE` ON build compiled this session is preserved
at `firmware/QUORUM/build_artifacts/toby_trace_on/` (gitignored — covered
by the repo's existing `*.bin`/`*.elf` patterns; local to this machine,
not uploaded anywhere). Flash `QUORUM.ino.merged.bin` at offset `0x0` (it
already contains the bootloader and partition table combined — that is
what `arduino-cli upload`/`esptool.py write_flash 0x0 ...` expects); the
separate `.bin`/`.bootloader.bin`/`.partitions.bin` are also present for
any flashing method that wants them individually.

SHA-256, so the exact bytes flashed can be checked against the exact bytes
built here, independent of anything else that happens to `LocoConfig.h`
between now and flash time:

```
d2c7c32d839d998a4586a183397fadf7745f02613d0116662b7b5d411c1799d6  QUORUM.ino.merged.bin
bee92793655ad0e2dc9f778357ad0966b7e3c2fc7854b291d9414021f4cf09eb  QUORUM.ino.elf
f29b09445338e12722158163b6dea7e815673f85bd5b2cf424382ec03a56779e  QUORUM.ino.bin
427f96e10c620c4f062dab15da54fc45494d897e8397ae6f3aecc98c42d7e379  QUORUM.ino.bootloader.bin
148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1  QUORUM.ino.partitions.bin
```

Verified directly against this exact `.elf` before it was preserved: the
string `9950012` is present, `9950011` is absent, and
`ngr/loco/%s/cmd/trace_anchor` is present (the format string; the
locomotive substitutes its own id at runtime via `buildTopics()`).

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
mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/trace_anchor -m "Grillers platform"
python3 tools/qt_decode.py ~/NGR/qt_logs/qt_20260824_193000.qtcap -o run.csv
python3 tools/qt_plot.py run.csv --save run.png
python3 tools/qt_plot.py run.csv --start 12 --duration 5
```

The receiver is **listen-only** — it has no command channel of its own,
by design, and does not need one: an operator anchor goes in over MQTT
(`cmd/trace_anchor`, see "Anchor mechanism" below), and the locomotive
turns it into an ordinary `QT_REC_ANCHOR` datagram on the same UDP stream
the receiver already reads, handled like any other record type. The
receiver writes every datagram verbatim, in arrival order, to a capture
file exactly like `hwt_receiver.py` does, so a bug in decoding can never
damage the evidence.

## Anchor mechanism

**Implemented: Option A** from the design surfaced in the prior session —
a trace-only MQTT command topic, gated by `#ifdef QUORUM_TRACE` so it does
not exist at all in a trace-off build (no topic, no subscription, no
handler, no command-queue branch, no buffer — confirmed by static audit
and by grepping the compiled ELF, see "Compile results" below).

**Topic:** `ngr/loco/<id>/cmd/trace_anchor` — e.g. `ngr/loco/9950012/cmd/trace_anchor`
for Toby. Matches this sketch's own existing `ngr/loco/%s/cmd/...` topic
convention exactly (`buildTopics()`); symmetric with the diagnostic-fixture
topic `cmd/force_lost`, which is the closest existing precedent this
mechanism was modelled on.

**Payload:** short operator text (ASCII, plain string, not JSON) — e.g.
`Grillers platform` or `START interval-014-015 next-015 physical-toward Grillers`.

**Text rule** (the ONE place it is enforced: `qtDecideAnchorText()` in
`QuorumTrace.h`, host-tested by `test_quorum_trace.cpp`, not just grepped
for):
1. Leading/trailing ASCII space/tab/CR/LF are trimmed.
2. Empty after trimming → **REJECTED** (matches `cmd/force_lost`'s own
   precedent of rejecting an empty payload outright — an anchor with no
   text carries no evidence).
3. Longer than 40 bytes after trimming → **TRUNCATED** to exactly 40 bytes
   (byte-level, not UTF-8-aware — same as the rest of this sketch's text
   handling), accepted, not rejected.
4. Otherwise → accepted verbatim.

**Pipeline:** the command passes through QUORUM's existing MQTT callback
(`onMqttEnqueue()`) and `cmdQueue` exactly like every other command —
confirmed by static audit that the branch lives in `handleCommand()`
(called only from `serviceCommands()`, the queue-drain loop) and does not
appear in `onMqttEnqueue()`. It executes on the loop thread, the same
thread that owns `navMm`/`navState`/`navDir`/PWM/`estopped` — but reads
none of them for any purpose beyond copying their current value outward
(`navDir`, `actualPwm`, `commandedPwm`), and writes none of them, confirmed
by static audit (no assignment to any navigation/motor/E-stop/Auto/
command-authority name, no motor-pin write, no call back into any
control-path function, no evidence-ring write, in either `qtSubmitAnchor()`
or the `handleCommand()` branch itself).

**What it stamps**, per accepted anchor (`QT_REC_ANCHOR`):
- a monotonically increasing anchor id (reset to 0 at every boot, alongside
  the sample/decision sequences — `qtTraceBegin()`);
- `sampleSeq` — the **most recently completed** Hall trace sample at
  execution time (`QtSampleRing::lastCompletedSeq()`, distinct from the
  forward-looking `sampleSeq()` STATUS itself uses — see that accessor's
  own comment in `QuorumTrace.h` if the distinction matters for your
  analysis);
- `millis()` at execution;
- direction (`navDir`, read directly — this runs on the loop thread that
  owns it, so no cross-core mirror is needed the way `hallTask`'s sample
  tick needs one) and both PWM values, at that instant;
- the trimmed/truncated text, verbatim.

**Acknowledgement:** published on `state/nav` (the same topic every other
one-time decision event — AGREE/DISAGREE/QUORUM_\*/FIXTURE_REJECTED/
FORCED_OFFSET — already rides, via the durable `pubMarker()` queue, so a
lost ack means the same thing a lost AGREE would: queue congestion, not a
design gap) and to `Serial`:

```
{"event":"TRACE_ANCHOR_ACCEPTED","anchor_id":3,"sample_seq":48210,"len":17,"truncated":false,"text":"Grillers platform"}
{"event":"TRACE_ANCHOR_REJECTED","why":"EMPTY"}
```

Deliberately **not** published via `publishQuorumDecision()` (the function
`FIXTURE_REJECTED`/`FORCED_OFFSET` use): that function also fires
`QT_DECISION_QUORUM()`, which would inject a spurious `QUORUM_EVENT/OTHER`
trace record for every anchor and drag in navigation-state JSON fields
(scores, leader, margin, ...) that have nothing to do with an anchor. A
dedicated `qtPublishAnchorAck()` avoids both. One known, accepted, pre-
existing-pattern limitation: the `text` field is embedded in this JSON
ack unescaped, exactly like `FIXTURE_REJECTED`'s own `payload` field
already is — operator text containing `"` or `\` would produce invalid
JSON on this **ack line only**. The wire `QT_REC_ANCHOR` record itself is
not JSON and is unaffected — it preserves the text exactly, byte for byte.

**Transport and integrity:** a dedicated 8-slot `QtAnchorRing` (bounded,
drop-oldest, counted — same discipline as the sample/decision rings),
pushed from the loop thread, popped only by `networkTask` (the sole owner
of both the MQTT connection and the trace UDP socket — no new task).
Overflow is counted in `cumAnchorRingDrops` and reported in every `STATUS`
record; nothing is dropped silently. Anchor and sample/decision records
share the same `locoId`/`sessionId` stamped at `qtTraceBegin()`. The
receiver (`qt_receiver.py`) stays listen-only and needed no change — every
accepted anchor becomes an ordinary `QT_REC_ANCHOR` datagram on the same
UDP stream it already reads, handled by the same generic per-record-type
logic already in `qt_decode.py`. The decoder places each anchor on the
merged timeline by `t_ms` like every other row, and now also carries its
`sample_seq` through to the CSV (`op_sample_seq` column) so it can be
cross-checked directly against the SAMPLE stream. `qt_plot.py` draws each
anchor as a distinct full-height labelled line (id, sample seq, text),
visually separate from `DECISION` lines.

**Commands:**

```bash
# publish an anchor (any MQTT client the operator already uses for cmd/* works)
mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/trace_anchor -m "Grillers platform"

# watch the acknowledgement (and every other state/nav decision event)
mosquitto_sub -h 192.168.68.142 -t ngr/loco/9950012/state/nav
```

## Capture procedure

1. Confirm the receiver is running on the Pi (or a laptop on the railway
   network) before power-up: `python3 tools/qt_receiver.py --outdir ...`.
2. Power up, enlist AUTO, declare start position, GO — exactly the
   existing operator workflow, plus operator anchors at known landmarks
   (`cmd/trace_anchor`, see "Anchor mechanism" above) whenever ground
   truth is worth stating in the moment.
3. Run the lap(s) per the Toby test protocol below (the numbered, staged
   version there is the one to actually follow step by step).
4. Stop capture (Ctrl-C on the receiver) after the locomotive is stopped
   and safe.
5. Decode and review before drawing any conclusion:
   `qt_decode.py`'s printed report first (completeness, ring-drop counts,
   transport gaps), then the CSV / plot.

## Integrity interpretation

`qt_decode.py`'s report separates, exactly like HWT1's:

- **`cum_sample_ring_drops` / `cum_decision_ring_drops` /
  `cum_anchor_ring_drops`** — this trace's own ring overflowed because
  `networkTask` could not drain it fast enough (weak WiFi, MQTT
  congestion). Diagnostic-only; does not mean QUORUM missed anything, only
  that the trace of it did. A non-zero `cum_anchor_ring_drops` is worth
  noticing specifically: it means an operator's stated ground truth never
  reached the capture at all — treat that anchor as absent, not delayed.
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
available) — this is the anchor-mechanism session's numbers; the previous
session's pre-anchor numbers are superseded.

| build | flash | flash % | RAM (globals) | RAM % | new warnings |
|---|---|---|---|---|---|
| Otto, trace OFF | 982,847 B | 74% | 52,452 B | 16% | 0 |
| Otto, trace ON | 991,339 B | 75% | 78,076 B | 23% | 1 (see below) |
| Toby, trace OFF | 982,783 B | 74% | 52,452 B | 16% | 0 |
| Toby, trace ON | 991,311 B | 75% | 78,076 B | 23% | 1 (see below) |

Trace-OFF byte counts are **identical** to a build with no trace code at
all, including the anchor mechanism — confirmed the same way as before:
comparing before/after wrapping the trace-only captures, both still
produce exactly 982,847 bytes for Otto. Compile-time elimination
demonstrated, not asserted, and confirmed to still hold after adding the
anchor mechanism.

**Delta, trace ON, sample/decision only (prior session) → +anchors (this
session):** flash grew a further **+1,092 B** (Otto: 990,247→991,339;
Toby: 990,191→991,311, matching within 28 B — see note below) and RAM a
further **+544 B**, both profiles, still leaving 249,604 B free for
locals. Dominated by the 8-slot `QtAnchorRing` (8 × 56 B = 448 B) plus the
new topic buffer (`T_CMD_TRACE_ANCHOR[64]`) and two small counters. Modest
relative to the sample/decision rings' own ~25 KB, as expected for an
operator-paced, 8-deep ring. **`QT_ANCHOR_RING`** is a `#define` in
`QuorumTrace.h` and can be tuned if a longer buffer is ever wanted.

The ~28 B flash difference between Otto's and Toby's trace-ON deltas
(both otherwise identical code) is not investigated further here — it is
far too small to be the anchor mechanism itself (identical code compiled
for both profiles) and is consistent with ordinary per-profile constant-
folding/alignment differences already visible between the two profiles'
trace-OFF builds before this session's changes (982,847 vs 982,783, a 64 B
difference of the same character).

**New warnings, both explained (both were present before this session's
anchor work and are unrelated to it):**
1. One instance of `'++' expression of 'volatile'-qualified type is
   deprecated` on `qtUdpSendFailures++` — the *same, already-accepted*
   warning class this codebase already carries on 8 other `volatile`
   counters (`queueDrops`, `floorRejects`, `pubWindowCount`, `cmdDrops`,
   `actualPwm`). Not a new class of issue.
2. `enumerated and non-enumerated type in conditional expression` on
   `applyDirection()`'s `derived` ternary (`QUORUM.ino:2030`, this
   session's line numbers) — **pre-existing**, present in trace-OFF
   builds too, in a line neither this nor the prior trace session
   touched. A pedantic `-Wextra` style warning (the enum's underlying
   type already matches `sessionDir`'s `int8_t`, so this is not a
   correctness concern) in core navigation logic this task is explicitly
   prohibited from changing ("this task observes existing QUORUM
   behavior; it does not change it") — flagged here rather than silently
   fixed, per the repo's own "flag anomalies, don't silently fix them"
   convention.

Timing: not independently measured this session (would need a flashed,
running locomotive). What IS established: `hallTask()`'s addition is
unchanged from the prior session (one `QT_SAMPLE_TICK()` call per existing
1 ms tick; no ring push on most ticks). The anchor mechanism adds nothing
to `hallTask()` at all — it is entirely loop-thread/networkTask, reached
only when an operator publishes a command, at most a handful of times per
run. `networkTask`'s drain is now bounded at ≤2 sample batches, ≤8
decisions, **≤4 anchors** per ~5 ms pass, so it still cannot starve
`mqtt.loop()` any more than the existing marker/status drains already
bound themselves — and in practice the anchor drain is almost always
zero-cost (nothing queued).

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
- **The anchor mechanism's ack JSON does not escape operator text**
  (matches `cmd/force_lost`'s pre-existing `FIXTURE_REJECTED` payload,
  which has the same property) — text containing `"` or `\` would break
  the `state/nav` acknowledgement line's JSON, though never the wire
  `QT_REC_ANCHOR` record itself, which is not JSON and carries the bytes
  exactly. Not fixed here: it is an existing pattern in this file, not a
  new gap this task introduced, and adding escaping to only the new code
  would be inconsistent with the rest of it.
- **`qt_plot.py`'s anchor rendering has one direct, non-matplotlib-
  dependent test** (`load()` parses `ANCHOR` CSV rows correctly) plus a
  manual end-to-end smoke render this session (see the delivered
  screenshot/plot); it is not exercised by a pixel-level automated test,
  the same honest limitation any of this repo's other plotting tools have.
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
  (`LL_LocoConfig_9950012.h`), `QUORUM_TRACE` **ON** — the exact preserved,
  hash-verified artifact at `firmware/QUORUM/build_artifacts/toby_trace_on/`
  (see "Toby trace-ON binary identity" above), not a fresh recompile.
- **Mode:** QUORUM Auto (enlist, GO) — normal operating procedure,
  unchanged.
- **Extent:** one short, continuous run, one direction (repeat separately
  for the other direction on a later, separately-approved session if
  wanted).
- **Speed:** Toby's reliable normal operating speed (`CRUISE_PWM`, 90 —
  unchanged by this task).
- **No deliberate stalls, reversals, or assistance.** The point is the
  reliable case; introducing an artificial failure defeats it.
- **Any unplanned stop, hesitation, uncertainty or intervention is
  anchored and reported immediately** (step 11 below) — what happened,
  where, and the wall-clock time, so it is findable in the capture
  afterward, not reconstructed from memory later.
- **Stop condition:** operator's own judgement, as always; E-stop remains
  fully available and unaffected by tracing (confirmed by the
  requirement-8/12 audits: no trace function, including the anchor path,
  reads or writes `estopped`).

Replace `9950012` below if run against a different locomotive; commands
match the implemented topic exactly
(`ngr/loco/9950012/cmd/trace_anchor`) — no placeholder syntax.

1. **Start the receiver** on the Pi or a laptop on the railway network,
   before power-up:
   ```bash
   python3 tools/qt_receiver.py --outdir ~/NGR/qt_logs
   ```
2. **Power Toby while stationary.** Do not enlist AUTO yet.
3. **Verify trace health before doing anything else.** The trace's own
   `STATUS` record travels over the UDP trace stream, not MQTT — read it
   directly from `qt_receiver.py`'s own console output (a `STATUS` line
   every ~2 s): confirm `free_heap` sane, `udp_fail` at or near 0, and
   `sample_ring_drops` / `decision_ring_drops` / `anchor_ring_drops` all 0
   (or explain any non-zero value before proceeding). Separately, over
   MQTT, confirm the locomotive itself is connected:
   ```bash
   mosquitto_sub -h 192.168.68.142 -t ngr/loco/9950012/online
   ```
   should show retained `1`.
4. **Publish a stationary test anchor:**
   ```bash
   mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/trace_anchor -m "STATIONARY_TEST pre-power-check"
   ```
5. **Confirm it appears live, in two independent places** — the
   acknowledgement on `state/nav` (`TRACE_ANCHOR_ACCEPTED`, with an
   `anchor_id` and a `sample_seq`), and `qt_receiver.py`'s own console
   printing `ANCHOR #<id> "STATIONARY_TEST pre-power-check"` as soon as
   the datagram arrives. Do not proceed past this step if the anchor does
   not appear on both — a full check that its `sample_seq` lines up with
   the `SAMPLE` stream at that same moment happens later, at step 15, once
   the whole capture is decoded.
6. **Verify E-stop responsiveness before enlisting AUTO** — trigger and
   clear E-stop once, confirm the motor stays at zero and
   `state/estop`/`ESTOP_CLEARED` behave exactly as they do today (tracing
   is confirmed, by the requirement-8/12 static audits, to have no path
   into this at all).
7. **Declare the exact starting interval and physical direction** the
   normal operator way (`cmd/session_direction`, `cmd/start_interval`) —
   not inferred from a filename or a prior session, per the standing
   evidentiary requirement from `docs/QUORUM_NAV_AUDIT_AND_GATE_PROTOTYPE.md`.
8. **Anchor the declaration itself**, in the documented free-text form,
   before GO:
   ```bash
   mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/trace_anchor \
     -m "START interval-014-015 next-015 physical-toward Grillers"
   ```
   (Substitute the operator's own confirmed interval, next marker, and
   physical-direction words — the example above is illustrative, not a
   prescribed interval.)
9. **Run one short continuous QUORUM Auto test**, one direction, normal
   Toby cruise (enlist, GO — unchanged procedure).
10. **Insert several anchors at independently known landmarks** during the
    run, e.g.:
    ```bash
    mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/trace_anchor -m "Grillers platform"
    ```
    Each is a ground-truth statement the operator is making in the moment,
    not a retrospective guess — say it as the locomotive passes the
    landmark, not before or after.
11. **Anchor and report any unplanned stop, uncertainty or intervention
    immediately**, e.g. `mosquitto_pub ... -m "UNPLANNED STOP -- see operator notes"`,
    then say what happened out loud / in writing at the same time.
12. **Stop Toby safely** (operator's own judgement).
13. **Insert a final stopped-position anchor** once fully stopped:
    ```bash
    mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/trace_anchor -m "STOPPED final position"
    ```
14. **Stop the receiver only after Toby is stationary** and the final
    anchor's acknowledgement has been seen (Ctrl-C on `qt_receiver.py`).
15. **Decode and inspect integrity before analyzing navigation:**
    ```bash
    python3 tools/qt_decode.py ~/NGR/qt_logs/qt_<timestamp>.qtcap -o run.csv
    ```
    Read the printed report first — ring drops, transport gaps, anchor
    count matches what was actually published (step-by-step above) — and
    only then look at `run.csv` / `qt_plot.py`'s rendering for navigation
    conclusions. An integrity problem found after the navigation analysis
    already started is a discarded analysis, not a footnote on it.

This protocol is ready for the operator to approve, adjust, or schedule.
It is not scheduled, and nothing above authorizes running it.
