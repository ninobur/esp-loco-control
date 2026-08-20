# 2026-08-20 — Circuit Express returns; and a radio hunt that came up short

Session: 2026-08-19 evening into 01:27 on the 20th. One continuous night.

## The headline, in the operator's proportion

**Circuit Express ran, and ran well.** Assignment from roles, severance that
sticks, EXPRESS and LOCAL missions held, the rotating skip, and the mission
ending on re-pairing rather than on a gap number. Across the evening's cycles
it did what decision 0038 says it should do.

**Navigation was quiet.** A paucity of navigation failures — no cluster of
phantoms, no repeat of the terminal-snapshot trouble that dominated earlier
sessions. Recorded as the operator's observation from a night of watching it,
which is the evidence that matters for a verdict of this kind.

This is the second capability in the CTO lineage to go from specification to
working railway, and the first that changes what a *service* looks like rather
than what a locomotive does.

*The NGR Trainspotter is impressed.*

## The rest of the night, in its actual place

The ESP-NOW investigation consumed most of the session's effort and produced a
negative result and a retraction, both worth having:

1. **A third receiver exists.** `ESPNOW_REPEATER` (decision 0039), flashed to
   the spare ESP32, is the only instrument in the system that measures
   delivery correctly — from sequence numbers rather than a ratio of rates.
   Modes `listen` / `shadow` / `repeat`, switchable over MQTT, relay gated on
   freshness and novelty because `ctoAcceptPeer()` has no monotonicity test.
2. **Every delivery percentage in the investigation was withdrawn.** The
   estimator divided two rates measured over two independently-timed windows,
   and had reported 100.2% — impossible for a delivery ratio, and the proof.
   See the withdrawal notice in `20260819_CE3_FIRST_CIRCUIT_EXPRESS.md`.
3. **The failure did not reproduce.** 5.5 minutes running under sound
   measurement: median 100% both directions, worst interval 70%, including the
   stretches that collapsed a few hours earlier the same night.
4. **Line of sight bought nothing.** With clear sight of both locomotives the
   third node was the worst receiver of the three. No geometry model proposed
   so far survives that.
5. **One collapse got away.** Otto's counters show a 4.5 s gap and an 8-frame
   miss run after the watch window closed — no timestamp, no position, no idea
   what Toby heard across it.

What survives of the radio picture: real multi-second dropouts, a real RSSI
spread with position, and no trustworthy measurement yet of what fraction of
packets get through during a failure.

## The next thing to build

Open item 1: per-peer receive-gap instrumentation inside QUORUM — current gap,
max gap, consecutive-miss run length, onset and recovery stamps, on the
existing `state/cto`. The same sequence-based accounting the third node does,
moved to where the failure actually happens, so the next collapse is caught
whether or not anyone is standing outside at 01:27 with a laptop.

## Still outstanding from 0038's implementation

- `SKIPPED` reports `station:"NONE"` — published before `stIndex` is assigned.
- Mission changes ramp over 700 ms regardless of size (`QUORUM.ino:2914`), so a
  56-PWM change becomes ~12 ms/step. Not CE-specific; CE is the first thing
  large enough to expose it.
- Skipped-platform speed policy — full cruise through the platform, which fell
  out of the implementation and is recorded neither way in 0038.
- Track sections for physical inspection: Otto mm 49–70, Toby mm 92–103.
