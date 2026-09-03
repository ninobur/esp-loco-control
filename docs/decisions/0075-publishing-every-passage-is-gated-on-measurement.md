# 0075 — Publishing every passage is gated on measurement

**Date:** 2026-09-03
**Status:** PROPOSED, and deliberately **not** ready to build. Not
authoritative until the operator reviews and approves it.
**Numbering:** written as 0074 and renumbered to 0075 on landing — decision
0074 ("Shape is recorded, not refused") was committed to this branch while
this was being written. Decisions are numbered sequentially and never
renumbered once landed, so this took the next free number.
**Adjacent to 0074**, which proposes that the Gaussian residual stop being
able to refuse a passage. The two are independent: this record is about
whether a passage's shape reaches the archive at all, 0074 about whether it
may veto. If both were approved, the residual would be recorded here and
advisory there, and the archive becomes the only place it does any work —
which raises the value of this record and lowers the urgency of nothing.
**Split from:** 0072, which now covers only the Pi-side database. Sam/CODEX's
review of 2026-09-03 recommended the separation: one is a reader of logs that
already exist, the other changes what a running locomotive puts on the radio,
and they should not stand or fall together.
**Evidence:** the operator's `NAVI_ONE_STATION_CURVES_0_1` field run of
2026-09-03, `docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260903.md`,
`docs/CURVES_DATABASE_REVIEW_RESPONSE_20260903.md`.
**Builds:** none proposed here. `NAVI_ONE_STATION_CURVES_0_1` exists and ran;
it is a diagnostic, not the permanent protocol.

---

## The question

For the database of 0072 to hold the railway's magnets rather than only its
refusals, the locomotive has to publish every passage it judges. The operator's
concern about that is stated precisely, and it is not about bytes:

> The fielded firmware shares the 2.4 GHz radio with ESP-NOW for inter-loco
> CTO.

## What the operator already measured

He did not wait for a proposal. On 2026-09-03 he built
`NAVI_ONE_STATION_CURVES_0_1` from X11 — Hall acquisition, median-of-five
reads, recognizer, navigation, station ramps, dwell, departure and stopping
all unchanged — and added one thing: every completed passage is published at
once, the binary waveform as before plus a companion JSON on
`diag/wave_meta` carrying the passage's own sequence number, its station phase
at open and close, station and marker offset, commanded and actual PWM, stop
episode, paused duration, `stitchAt`, `restLevel`, `preSamples`, decimation
and verdict.

The run, 15:08 to 15:52, ingested into the curve database and measured there:

| | |
|---|---|
| passages published | 1,177, sequence 1–1177, **no gaps**; `pub_drop` 0, `cmd_drop` 0 |
| rate | 26.7 a minute mean; busiest minute 50 |
| samples per passage | median 148, p95 270, max 494 |
| bytes on the wire | median 336, mean 375, max 1,028 |
| fitting one 704-byte payload | 1,135 of 1,177 (96.4%) |
| archive traffic | **167 B/s** mean, **319 B/s** in the busiest minute |
| station-associated passages | 344, **every one accepted** |
| refusals | 5, all `TOO_SOON`, all off-station near MM001–002 |
| paused or stitched passages | none in the run |

Residual by station phase, which is what a recognizer reader wants from it:

| phase | n | median | max | refused |
|---|---:|---:|---:|---:|
| cruise / IDLE | 833 | 0.0691 | 0.0990 | 5 `TOO_SOON` |
| approach | 135 | 0.0735 | 0.0885 | 0 |
| zone | 128 | 0.0735 | 0.0897 | 0 |
| zero ramp | 54 | 0.0820 | 0.1150 | 0 |
| departure | 27 | 0.0741 | 0.1180 | 0 |

Two things in that table matter beyond the gate. The zero ramp and the
departure carry the **highest residuals and the longest records** — zero-ramp
passages run to a median of 331 samples against 145 at cruise, right at the
one-frame boundary. The largest archive records are produced exactly where the
locomotive is moving slowest and where the geometry is most delicate. And
0.1180 against a 0.13 ceiling is the closest an accepted passage came all day.

## What this settles, and what it does not

**Settled.** The size and rate of the traffic, from a representative run rather
than from the 215 refusal-and-window records that were all the archive had
before. The earlier estimate — 45 markers a minute, 494 bytes mean, 370 to 600
B/s — was biased upward by a sample selected for being abnormal. The real
figure is 167 B/s, about 8% of Toby's own 2.1 KB/s at cruise. Publishing every
passage does not flood the link, and the recognizer behaves through real
station stops.

**Not settled, and these are the gates.**

1. **CTO contention is untested and its impact is unbounded.** NAVI_ONE has no
   ESP-NOW at all, so this run says nothing about it. The earlier proposal
   claimed the worst an archive frame could do to a CTO beacon was one frame's
   airtime. That was wrong, and the review named why: it accounts only for a
   frame already on the air, and not for driver contention between TCP/MQTT
   and ESP-NOW, retransmission and backoff, rate fallback on a weak link, time
   spent inside a blocking `mqtt.publish()`, CPU scheduling between the
   network task and the loop-side CTO service, or several locomotives bursting
   at the same station. The honest statement is: **the queue rule limits each
   scheduling decision to one publish; end-to-end CTO impact is unbounded
   until it is measured.** QUORUM beacons every 500 ms and peer truth goes
   stale at 3 s, and fleet separation depends on it.
2. **No burst, whatever the state.** The first design drained one frame per
   network-task pass while `stationMachine.holding()` or `!autoRunning`. At a
   10 ms task period that is 100 publishes a second, which at the 1 Mbps floor
   is most of the channel — and `!autoRunning` is not "stationary", because a
   locomotive under manual control is moving and its commands matter. Any
   build must gate on `actualPwm == 0 && commandedPwm == 0`, rate-limit in
   **every** state including a dwell, and suspend entirely while CTO is
   paired, traffic is holding, peer freshness is degrading, or ordinary
   outbound traffic is backed up.
3. **Delivery is best-effort and must be described that way.** A RAM queue
   drops its oldest record when full; MQTT QoS 0 acknowledges nothing; a
   reboot loses the queue. Counting drops is observability, not retention. The
   contract is **every judged passage is offered to the archive, and loss is
   detectable** — by the sequence gap and by the drop counter. Literal
   completeness needs durable buffering or an acknowledged protocol, and
   neither is proposed.
4. **Identity must be on the wire.** `seq` today restarts at every boot, lives
   only in `wave_meta`, and appears in neither the binary waveform nor
   `mm/marker`. The permanent protocol carries a generated `boot_id` and
   `passage_seq` in all three, and that pair becomes the deterministic join;
   `opened_ms` and `closed_ms` stay as evidence. Until then the Pi derives the
   boot and matches heuristically, with provenance and ambiguity recorded
   (0072).
5. **Free heap has never been reported** on this lineage. No queue is sized
   until `state/loopstat` carries `ESP.getFreeHeap()` and
   `ESP.getMinFreeHeap()` and the operator has read them.
6. **Rollback must be immediate**: disabling archive production must change
   nothing about recognition, navigation or how the locomotive runs.

## The unintended consequence, stated now

Archive telemetry is the first traffic this railway would carry that **no
operational decision depends on**. That is what makes it safe to delay, drop
and rate-limit — and it is also what makes it dangerous, because it is the
first traffic whose loss nobody will notice at the time. Two consequences
follow and should be designed for rather than discovered:

- **It must never acquire authority.** The moment anything on the locomotive
  or the dashboard waits for, retries, or blocks on an archive publish, a
  diagnostic has become a dependency, and a full queue becomes an operational
  fault. It has no authority, and yields to commands, ordinary telemetry, WiFi
  maintenance and CTO.
- **Passages will arrive late and in clumps** if they are held for dwells,
  minutes after their marker events. A console reader should not read that as
  a fault, and the Pi must not either — which is why the decoder matches over
  a thirty-minute window and records how it matched.

## Attribution

The operator asked for the database and named radio airtime as the concern. He
built and ran `NAVI_ONE_STATION_CURVES_0_1` himself and produced every field
number above. The corrections in "not settled" — the overstated CTO bound, the
dwell burst, the `!autoRunning` gate, the delivery contract, and the demand
that identity go on the wire — are Sam/CODEX's review findings of 2026-09-03.
The queue discipline is an agent's proposed technique and is not his ruling.
