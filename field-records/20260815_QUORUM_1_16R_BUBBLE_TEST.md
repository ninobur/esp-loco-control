# 2026-08-15 — QUORUM 1.16R first field test, Otto and Toby in the CTO Bubble

**Gate 0 of the IR integration spec.** Both locomotives flashed and
identity-verified at boot: `[BOOT] QUORUM_1_16R — 9950011` (Otto),
`[BOOT] QUORUM_1_16R — 9950012` (Toby). Toby's USB adapter would not hold
921600 baud; flashed at 115200, hash verified. Supervised throughout,
E-stops at hand, per CODEX's approval conditions.

## Verdict: the navigator passed. Two incidents, neither caused by 1.16R.

## Navigation performance

Sampled live mid-session, both locomotives NAV_NORMAL:

| | markers agreed | disagreed | lost | quarantines |
|---|---|---|---|---|
| Toby (CW) | 795 | **0** | 0 | 0 |
| Otto (CW) | 779 | 13 (1.7%) | 0 | 0 |

Zero quarantines, zero `PHANTOM_REJECTED`, no evaluation opened on healthy
track — which is the correct 1.16R signature. The new machinery is supposed
to be silent when the railway is clean, and it was. Otto's 1.7% disagreement
rate is his familiar noisier detector, unchanged from the 1.13 era.

Bubble choreography worked as designed and unremarkably: Otto FOLLOWER
pacing behind Toby with a 9-marker gap, later Toby FOLLOWER capping at 42
PWM at 12 markers and 0 at 6 as the gap closed. Housekeeping clean
throughout — zero publish drops, zero queue drops, loop gaps ≤ 34 ms,
batteries 16.5 V / 16.3 V.

The IR sensor car was online and honest the whole session, reporting
`MARGINAL` / `REACQUIRING` with `speed_mmps: null` — no fabricated zeros.

## Incident 1 — NO_QUORUM after a direction change (cause: console workflow)

**Not a firmware defect.** Sequence from `all_20260815.log`:

- 08:41:35 — `cmd/session_direction CCW`. Toby was NORMAL and knew he was
  at **mm 59**.
- 08:42:09 — `cmd/start_interval 051-052` declared **mm 52**, a 7-marker
  jump. Almost certainly the previous session's interval left in the
  console field: the pre-AUTO gate requires an MM before it will permit GO,
  so a position the locomotive already knew had to be re-entered by hand.
- 08:43:19 onward — the polarity trail contradicts it immediately and
  incoherently: DISAGREE, AGREE, DISAGREE, AGREE, AGREE, DISAGREE,
  DISAGREE, DISAGREE.
- 08:43:30 — evaluation opens. The board never separates; twelve readings
  end `[8,5,6,6,6,7]` — leader 8, runner-up 7, margin 1 — QUORUM_TIED all
  the way to HARD_BOUND at 08:43:42.

The truth was −7. The fence spans −1..+4, so the correct answer was not
among the hypotheses the navigator is permitted to consider. It refused to
guess and stopped. Otto then held under fleet stop (decision 0031) because
separation against a discredited partner is meaningless.

Recovery required an operator declare: self-resolution needs 12 fresh
events and a stopped locomotive generates none. **That is a known and
permanent limit** — recovery from a bad declare while stationary will
always need the operator.

**Findings owed (console, not firmware):**

1. Do not demand a declaration when the navigator is healthy. Position
   survives a direction change by design; if nav is NORMAL the pre-AUTO
   gate should accept the navigator's own MM.
2. Never pre-fill a stale interval. Blank it, or seed it from the current
   position.
3. Flag the mismatch before obeying: *"navigator believes MM059, you
   declared MM052 (−7, outside the fence) — confirm?"* The operator stays
   sovereign; the operator gets told. This is the one input the system
   swallows with no scrutiny at all, in a navigator that quarantines a
   suspicious magnet reading.

## Incident 2 — fleet stop on "peer STALE" while the peer was healthy

Toby fleet-stopped at 09:06:35 and 09:06:44, `why: STALE`, expecting Otto.
Otto at that moment: `online 1`, publishing every second, nav NORMAL, and
his CTO `tx` counter climbing at 2 Hz with `txe: 0`. Toby's `rx` had fallen
to 0.4–0.8 Hz in seven bursts between 09:04 and 09:06; the worst delivered
2 packets in 5 s, exceeding the 3 s stale threshold.

**A first diagnosis of range/masonry was wrong, and the operator falsified
it: a reset restored the link.** Masonry does not respond to a reboot.

**The counters that suggested "transmitter healthy" cannot support that
claim.** `ctoTxAttempts` increments unconditionally; `ctoTxErrors` only
catches `esp_now_send()` returning non-OK, and ESP_OK means *queued to the
driver*, not transmitted. No send callback was registered, so the firmware
had no knowledge of transmission outcomes at all. A request counter was
read as a measurement — the exact error decision 0024 exists to prevent,
committed against the radio rather than the motor.

**Leading hypothesis: station channel divergence.** ESP-NOW peers are added
with `channel = 0` ("current"), and the station channel follows the AP
(measured: channel 11, 2.4 GHz, 20 MHz). A locomotive that roams or
re-associates onto another channel keeps **perfect MQTT** — that path goes
through the AP and does not care — while becoming invisible to peers whose
ESP-NOW lives on the old channel. That asymmetry is precisely what was
observed, and a reset (fresh association) restoring it fits exactly.

Unprovable after the fact, because nothing reported the channel. That gap
is closed by 1.16Ra.

**Also recorded:** taking a locomotive out of AUTO individually works
correctly and was *not* the cause here, contrary to a first reading.
`ctoTxStatus()` and `ctoService()` are not gated on `autoRunning`.

## Open items from this session

- Console: the three declaration findings above (Pi-side, reversible).
- A locomotive deliberately leaving AUTO is indistinguishable from one
  whose radio died; both produce a full fleet stop. An operator-defeatable
  fleet stop was requested and needs its own decision record — decision
  0031 is a safety ruling and cannot be quietly softened.
- `CTO_PEER_STALE_MS` (3000) tolerates 5 consecutive losses. Whether that
  is right should be settled from a measured gap/jitter distribution over a
  full lap, not chosen — the same discipline the IR cadence constants owe.
- Channel congestion is a live co-suspect for any future link trouble:
  both locomotives' MQTT, the IR car's MQTT, CTO peer packets at 2 Hz each
  way, role echoes, the command bridge, and household WiFi all share one
  20 MHz channel. The IR sender would add another stream. Gate 1's jitter
  measurement must be taken with locomotives running, not on a quiet bench.

## Photographic record

`hardware/` and the session photos show the two consists at maximum
separation — Toby at the patio corner, Otto at Grillers — with the brick
BBQ pillar and support post directly in the sight line. Retained because it
documents the physical geometry behind any range hypothesis, even though
the reset evidence points at channel state instead.

## Firmware identity

- Test article: `QUORUM_1_16R`, commit `31c4898` (decision 0035 Accepted).
- Follow-on: `QUORUM_1_16Ra`, instrumentation only, byte-identical to
  1.16R across all 43 replays. Not flashed during this session.
