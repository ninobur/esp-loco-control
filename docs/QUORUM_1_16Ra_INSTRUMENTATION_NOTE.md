# QUORUM 1.16Ra — radio instrumentation, from a field incident

**Status: built, byte-identical to 1.16R across every replay, both profiles
compiling clean (1,000,055 bytes, 76%). Instrumentation only — no navigation,
motor, or wire-contract change. NOT flashed.** Origin: the 2026-08-15 Bubble test,
recorded in `field-records/20260815_QUORUM_1_16R_BUBBLE_TEST.md`.

## Why

Toby fleet-stopped on `peer STALE` while Otto was healthy on MQTT and his
CTO `tx` counter climbed at 2 Hz with `txe: 0`. Those counters were read —
by me, in the field, in front of the operator — as proof the transmitter was
working.

They cannot support that claim:

- `ctoTxAttempts` increments unconditionally, before the send;
- `ctoTxErrors` only catches `esp_now_send()` returning non-OK, and **ESP_OK
  means "queued to the driver"**, not transmitted;
- no send callback was ever registered, so the firmware had no knowledge of
  transmission outcomes at all.

A request counter was being treated as a measurement. That is precisely the
error decision 0024 exists to prevent — *"PWM is actuator effort, not proof
of motion"* — committed against the radio instead of the motor, in a project
whose entire discipline is the opposite. The operator falsified the first
diagnosis (range/masonry) in one sentence: **a reset restored the link**, and
masonry does not respond to a reboot.

The leading hypothesis is a **station channel divergence**. ESP-NOW peers are
added with `channel = 0` ("current"), and the station channel follows the AP
(measured: 11, 2.4 GHz). A locomotive that roams or re-associates onto
another channel keeps perfect MQTT — that path goes through the AP and does
not care — while becoming invisible to peers whose ESP-NOW lives on the old
channel. Healthy on the broker, silent on the air: exactly what was observed,
and a fresh association on reset fits it exactly.

It is unprovable after the fact, because nothing reported the channel. That
is the gap this revision closes.

## What was added

| addition | field | meaning |
|---|---|---|
| ESP-NOW send callback | `txd` / `txf` | what the MAC layer actually did with each frame |
| station channel | `ch` | the channel those frames went out on |
| drift alarm | `CTO_CHANNEL_CHANGED` + `chg` | published once a channel move is confirmed, on `state/cto` only |

`tx` and `txe` keep their existing meanings exactly, so no consumer breaks;
`tx` is now explicitly documented as an *intention* counter sitting beside
the measurement.

**Honest limit, stated in the code so nobody over-reads it:** ESP-NOW
broadcast frames are unacknowledged. `txd` means *this frame reached the
air* — never *a peer received it*. Reception is only ever evidenced by the
peer's own `rx` counter, which is why both locomotives publish theirs.
Comparing my `ch` against the peer's `ch` is the whole diagnosis for a
silent-partner incident.

The callback follows the same discipline as `ctoOnRecv`: WiFi-task context,
two counters, nothing else. The channel check and every publication happen in
loop-owned `ctoService()`.

## Verification

1. **Byte-identical to 1.16R on 43 of 43 replays** — 12 real capture segments
   plus all 31 fixtures, stdout and stderr compared together. This is the
   claim that matters: instrumentation must not move a single navigational
   outcome, and it does not.
2. **Both-era suite green**: quarantine era (30 fixtures, QGOLD, map facts,
   input-invariance) and legacy era against the frozen pre-1.16 binary.
3. **Both profiles compile clean** under `--warnings all`.
4. **Frozen contracts untouched**: `CtoPeerPacket`, `CTO2_VERSION`,
   `ESPNOW_VERSION`, and the CTO3 echo are unchanged. The only payload change
   is additive keys on the existing `state/cto` topic.
5. **Driven tests for the new code** (`tests/test_channel_watch.py`, wired
   into `run_suite.py`): 17 assertions covering the latch, the confirmation
   gate at one/two/three readings, four out-of-band channel values, the
   CTO-off case, the untouched warning slot, and JSON validity of every
   payload. The harness gained `wifi_channel`, `advance`, `cto` and
   `cto_off` commands to make the watch reachable at all.
6. **Adversarial verification pass** — the standing substitute for CODEX
   review while he is out of tokens. **It refuted the first draft**, and the
   fixes below are its work.

## What the self-review round changed

The first draft was reviewed-but-unexercised and carried four real defects.
Recording them because the pattern matters more than the fixes:

1. **A buffer overflow, in the same buffer, for the third time.** Four keys
   were appended to `char b[288]` without redoing the arithmetic: measured
   worst case 301 into 287 usable, truncating mid-key with no closing brace.
   The failure on the railway is not an error but *silence* — the console's
   `json.loads` sits in a bare try/except, so the CTO panel would freeze on
   stale values while the locomotive believed it was reporting. The worst
   possible failure for a diagnostic added to explain an unexplained stop,
   and invisible until counters crossed ~1e9. Now 384 (measured worst case
   300) **plus a truncation guard**, so arithmetic is no longer the only
   defence.
2. **The alarm fired with CTO off.** `ctoService()` gates on `ctoRadioUp`,
   never on `ctoEnabled`, so a locomotive running solo would have announced
   that "peers cannot hear this locomotive" with no peers in existence.
3. **It hijacked the operator warning slot.** That slot is single, global,
   and carries SELF_RESOLVED's *"BEGIN to resume"* prompt — the 1.16R
   finding-1 interlock telling the operator a locomotive is waiting for
   them. A channel notice would have silently overwritten it. The warning is
   gone; the `state/cto` event is the record.
4. **No range check.** `(uint8_t)WiFi.channel()` truncates an int, so a
   negative error return or a mid-reassociation sample became a plausible
   channel, fired the alarm, *and* latched — two false alarms and a
   permanently wrong reference from one bad sample. Now 1..14 or "unknown",
   with three consecutive readings required before believing a change, which
   also kills the boot-settling false alarm.

The through-line: the first draft was correct in intent and wrong in every
place where I substituted reasoning for measurement — including the buffer,
where I wrote out an arithmetic worst case (305) that was itself wrong. The
committed comment now cites the *measured* 300.

In an ordinary replay the new paths stay inert: the shim's `WiFi.channel()`
defaults to 0 (an unassociated station has no channel), which the drift watch
treats as "unknown" and never reports, and no send callback fires because the
replay has no radio. A replay transmits nothing, and the counters honestly say
so — which is why the capture corpus can prove no regression and nothing else,
and why the driven tests above exist.

## What this does NOT fix

The send callback would **not** have caught the 2026-08-15 incident. If a
locomotive transmits on the wrong channel, the MAC layer reports success —
the frame did reach the air. **The channel report is the diagnostic; the send
callback is a provenance fix** owed independently under decision 0016, so
that no future incident is analysed with a counter that measures intentions.

Nothing here prevents a divergence. It makes one say so, immediately, instead
of leaving an unexplained fleet stop behind.
