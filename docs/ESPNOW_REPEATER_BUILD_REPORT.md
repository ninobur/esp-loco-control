# ESPNOW_REPEATER 1.0 — build and bench report

Date: 2026-08-20
Artifact: `firmware/test-programs/ESPNOW_REPEATER/ESPNOW_REPEATER.ino`
Decision: 0039
Status: **Built, flashed to the spare ESP32, bench-verified. Not deployed at a
survey position; no field verdict.**

## What was asked for, and what changed on the way

The session's agreed next step was a *passive* ESP-NOW monitor: listen, record,
and only enable forwarding if the monitor proved it hears the locomotive whose
packet the other locomotive is missing. The operator then directed that the
ESP32 attached to the Mac be programmed **as a repeater**.

Both are delivered in one artifact. `listen` and `shadow` are the passive
instrument; `repeat` is the relay. The boot mode is `repeat` (`MODE_DEFAULT`),
per the direction; the mode is switchable at runtime without reflashing.

The evidence discipline is unchanged by relaying being switched on. Recording
runs in **every** mode, so a `repeat` session is still admissible evidence and
not a treatment that destroys its own measurement. The operator's criterion —
the node must hear the *specific* locomotive missed during the *specific*
multi-second gap — is still what decides whether relaying is worth anything,
and it is not answered by any aggregate figure this node produces.

## Design points that are not arbitrary

**Relay before statistics.** In `handleFrame()` the forwarding decision is the
first thing done with a frame. Every millisecond spent on statistics or MQTT is
added to the relayed frame's age, and age is the hazard being bounded.

**The three gates** (decision 0039 states why): freshness ≤ 100 ms, strictly
greater sequence per source, and rate limiting. `ctoAcceptPeer()` in QUORUM has
no monotonicity test, so a late or duplicate relay could roll a peer's believed
position backwards or distort `rampFalling`. The gates are what keep the relay
a redundant *path* rather than a new *claim*.

**Per-frame RSSI comes from `rx_ctrl`, not `WiFi.RSSI()`.** `WiFi.RSSI()`
reports the link to the access point and says nothing about the
locomotive-to-here path, which is the entire question.

**Miss runs come from sequence arithmetic, not elapsed time.** A locomotive
that was itself stopped or rebooted must not be recorded as radio loss; a
sequence that goes backwards re-baselines the source (and the novelty gate with
it) instead of recording a spurious multi-thousand-packet loss.

**Channel watch.** A node on the wrong channel hears nothing and relays
nothing, and is indistinguishable from a perfect radio shadow. It measures its
channel, warns on serial, and publishes `CHANNEL_MISMATCH`.

## Two defects found on the bench and fixed

1. **The spacing gate discarded instead of deferring.** The two locomotives'
   2 Hz streams arrive about 10 ms apart, inside the 15 ms transmit-spacing
   floor, so the first build silently refused to relay roughly every second
   Otto frame (`SPACING` in the first capture). Spacing now waits out the
   remainder — a few ms against a 100 ms freshness budget — and re-checks
   freshness after the wait. Second capture: 76/76 relayed, no `SPACING`.
2. **Window delivery could exceed 100%** (observed 105.6%). The denominator was
   time-based (`span / 500 ms`), and a window whose start is the first frame
   *in* it systematically under-counts what the sender sent. It is now computed
   from sequence numbers, which is exact and cannot exceed 100%.

Both were caught only because the instrument prints its own reasoning per
frame. That is the argument for the `fwd` verdict field staying in the record.

## Build

`arduino-cli`, FQBN `esp32:esp32:esp32`, core 3.3.11, `--warnings all`,
explicit `--build-path` (the stale `firmware/QUORUM/build/` IDE artifact has
reported the wrong locomotive before).

- Clean: **zero warnings**.
- 939,657 bytes program (71%), 51,260 bytes global (15%).

## Flash

Target `/dev/cu.usbserial-0001`. Identified before erasing: the board was
running `CTO_Recorder_1.5` ("NGR COMPACT ESP-NOW RECORDER"), i.e. the spare /
dispatcher recorder, **not** a locomotive.

**That sketch's source is not in this repository** — a `grep` for its banner
finds nothing. A full 4 MB image was therefore read off the board before
flashing and committed compressed as
`field-records/firmware-images/dispatcher_esp32_pre_ESPNOW_REPEATER.bin.gz`.
It is the only surviving copy of that build; treat it accordingly.

## Bench verification (20 s capture, node beside the Mac, mode `repeat`)

| observation | value |
|---|---|
| channel | 11, matching both locomotives |
| sources heard | 2 — Otto 9950011 and Toby 9950012 |
| frames relayed | 76 of 76 decided; every verdict `SENT` |
| transmit result | `tx=134/0` — 134 done, 0 failed |
| window delivery at this position | Otto 95%, Toby 90% |
| cumulative delivery | 95.8% both |
| max gap seen even here | ~1.0 s per source |
| dropped frames / foreign frames | 0 / 0 |

The bench position is not a survey position and these percentages are not a
coverage claim. What they establish is that the instrument runs, hears both
locomotives, decides correctly, and transmits without failure.

Note in passing: at ~1 m from both locomotives this node still recorded
one-second gaps in each stream. That is consistent with the evening's finding
that loss is not simply a function of distance, but it is a bench observation,
not evidence — recorded here so it is not later mistaken for a result.

## What has NOT been done

- Not deployed at any candidate survey position. `SURVEY_SITE` is `MAC_BENCH`
  and must be set to the real placement before a run counts as a survey.
- No comparison yet between this node's reception and the locomotives' STALE
  episodes. That comparison is the whole point and it has not been made.
- The relay's effect on locomotive-observed delivery has not been measured;
  nobody should yet claim it helps.
- Open item 1 (per-peer receive-gap instrumentation *inside* QUORUM) is
  untouched and still the recommended next step.
