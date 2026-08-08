# Station Stop v1 — inaugural field run and two dashboard findings

Date: 2026-08-08, afternoon–evening
Firmware: **exact QUORUM 1.9** (`ede7a08`, CLI-flashed from a clean
worktree, SHA-256 `f3ead79c637f5626…`; boot witness `[BOOT] QUORUM_1_9`,
`[INA] ready`, `[CAL] baseline=2026`)
Console: v1.10.10 (deployed this session; live confirmed by the
`enlisted` field in `/dispatcher/state`)
Consist: Otto towing the IR test car (new mount, no outer spoke shield)
Mission: CCW, Arches-only
Evidence: `field-records/logs/20260808_T-sweep_arches.log` (part 1),
`…_part2.log` (part 2; capture continued after this record was written)
Scope note: the moving-AUTO attempt was **withheld by the console** and
never reached the firmware; QUORUM 1.9 has no enlistment guards and none
of the 1.10 P11/P13/P14 items are validated by anything in this run.

---

## 1. The inaugural automatic station stop — sequence VERIFIED

First automatic operations ever run under the QUORUM navigator. Complete
telemetry, part 2, one unbroken minute:

| Time | Event | Offset | cmd→act PWM | Note |
|---|---|---|---|---|
| 16:34:52 | ARMED | −12 | 100 | RANGE_ARM_NORMAL |
| 16:34:54–:35:03 | APPROACH ×5 | −10…−6 | 92→60 derived ramp | exactly the §6 derivation |
| 16:35:03–:12 | ZONE_HOLD ×5 | −5…−1 | 60 | HOLD_60 |
| 16:35:15 | FINAL_APPROACH / FINAL_TARGET | 0 | 60 | AT_CENTRE_ZONE_SPEED |
| 16:35:17 | FINAL_TARGET | +1 | 45 | M_PLUS_1_FINAL_SPEED |
| 16:35:21 | **ZERO_RAMP** | **+2** | 0←45 | **TRIGGER_M2_REACHED — the nominal trigger, not the timeout** |
| 16:35:24 | DWELL_BEGIN | +2 | 0/0 | FIXED_DWELL |
| 16:35:39 | DWELL_COMPLETE | +2 | 100 | **15.0 s exactly**; DEPART_TO_CRUISE |
| 16:35:45 | DEPARTURE_COMPLETE | +5 | 100/100 | CLEARED_ZONE |
| 16:35:45 | RESET | — | — | DEPARTED |

Operator's physical report: slowed, stopped just past MM 106, dwelled,
restarted decisively, passed Grillers. **No station other than Arches
armed at any point in either capture** — the 1.9 mission filter held,
including through the Grillers pass. PAUSE (16:36:27, per-locomotive
topic) stopped him; END (16:37:43) returned manual control.

Against the README acceptance gate this is **one qualifying cycle of the
three required** — the catalog row stays *ready for field test* until the
gate completes, but every per-cycle criterion was met.

## 2. Finding A — the console treats every live bootid as a reboot

**Confirmed, with the capture as proof.** The firmware publishes
`state/bootid` on **every MQTT connect** (`attemptReconnect()` republishes
online/boot/alert), not only after a boot. Console v1.10.10 (inherited
from v1.10.9) calls `_reset_session()` on every live bootid
([v1_10_10:727]) — clearing `agree_n`, `disagree_n`, `verdicts`, and
`start_interval`.

Uptime classification of all 24 live bootids in the two captures:

- **4 genuine ESP32 resets** (uptime restarts near zero): 14:08:26,
  14:11:54, 14:16:05 — the three flash/boot events — and 16:27:47 (power
  cycle).
- **14+ MQTT reconnects** (uptime continues, e.g. 893 170 → 1 532 170 ms
  across the 14:41:33 bootid): every one a **false reboot** to the
  console, every one a session wipe.

This explains tonight's symptoms exactly: Polarity Agreement reset to 0
(the tally is console-side and was wiped), pre-flight lost (the cleared
`start_interval` makes P6 withhold AUTO — correctly, given its false
input), while CCW and MM 26 survived (restored by the next 1 Hz alert,
which carries `session_dir` and `mm` but not the interval). It is also,
retroactively, the answer to the operator's 2026-08-07 question about
disappearing agree counts.

## 3. Finding B — the trigger is the known connectivity flapping

The false resets cluster where the alert stream stalls: uptime frozen at
3 378 170 ms across **eight consecutive bootids, 15:18–16:27** — the
stall-and-flush signature documented on 2026-08-07 (session held,
delivery gaps bridged by TCP). Each stall ends in an MQTT reconnect;
each reconnect republishes bootid; each bootid wipes the console session.
The watch-item RF fault and the console defect chain into one visible
failure. Neither capture shows a locomotive-side navigation consequence —
QUORUM's own state is untouched throughout; this is a display/session
defect only.

## 4. Finding C (minor) — `endcto` still publishes the fossil topic

END works via per-locomotive `dispatcher_release`, but
`dispatcher_endcto()` also publishes suffix-less `ngr/dispatcher/cmd/stop`
(16:37:43), which no locomotive subscribes to — the same ESP-NOW-era
fossil P2 removed from the BOTH buttons, surviving in one more place.
Harmless; should be fanned out or dropped in the next console pass.

## 5. Also on the record

- The console's **withholding worked as ruled**: the post-END moving-AUTO
  attempt produced **no `cmd/auto` publish at all** — R8's
  "absence of change is the signal," observed in the field.
- The P8 seed, ENLISTED display, grey-out, PAUSE≠END labels all behaved
  through the cycle (operator-observed; T-sweep formal pass to be run
  once the session-reset defect is fixed, since T5/T7 touch the same
  mechanism).

## 6. Smallest proposed correction — NOT implemented, for review

**Move reboot detection from bootid arrival to uptime regression.** The
console already receives `uptime_ms` at 1 Hz in the alert. Keep the
bootid handler for `sketch`/`online`/heard, but call `_reset_session()`
only when the observed uptime **regresses** (new < last, with a small
floor to ignore wrap/jitter, e.g. new < 15 000 ms and new < last):

- a genuine reboot always trips it (uptime restarts near zero);
- an MQTT reconnect never does (uptime continues);
- no firmware change, no new topic, one comparison and one stored value
  per locomotive.

Alternative considered: have the firmware distinguish boot-bootid from
reconnect-bootid. Rejected as larger scope and a payload change; the
uptime signal already exists and is authoritative.

Review before implementation, per process.
