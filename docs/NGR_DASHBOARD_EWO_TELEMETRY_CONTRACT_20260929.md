# NGR dashboard v1.12.0 — NAVI_EWO telemetry contract

Status: development candidate, not deployed. Implementation:
`server/ngr_app_v1_12_0.py` (copied from v1.11.2, which is unchanged and remains
the rollback). Firmware reference: `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2` at
`caf2280`. No firmware or NAVI logic is changed by this work.

## Rule

The dashboard displays what NAVI publishes. It does not choose a position,
target, magnet, sensor or station. Where the firmware does not publish a fact,
the dashboard shows it as not known (section 4). EWO handling applies only to a
locomotive that identifies as EWO (`state/nav` `authority=NAVI_EWO`, or a
`NAVI_EYES_WIDE_OPEN*` sketch/build). Legacy firmware keeps the v1.11.2 display.

## 1. Inventory and classification

| Dashboard input (topic: fields) | Class | EWO use |
|---|---|---|
| `online` | KEEP | link / last will |
| `state/bootid`: `sketch` | KEEP | version chip, EWO identification |
| `state/bootid`: `boot_id` | NEW EWO FIELD | reboot = live `boot_id` differing from last seen; replaces `alert.uptime_ms` |
| `alert`: `uptime_ms` | ADAPT | → `state/bootid.boot_id` (EWO publishes no `alert`) |
| `alert`: `moving`, `est_mm_s` → MOVING/STOPPED, pKPH | ADAPT | → NAVI-qualified IR speed `telem/ir.ir_valid/ir_mmps`; none = motion NOT MEASURED |
| `alert`: `auto` (RUNNING) | ADAPT | → `state/loopstat.running` |
| `alert`: `nav`, `dead_reckoned_mm`, `session_dir`, `pwm` | ADAPT | → `state/nav`, `state/loopstat.pwm` |
| `alert`: `agree`, `disagree`, `viable`, `candidate_mm`, `last_confirmed_landmark` | RETIRE | QUORUM/Navigator concepts |
| `state/nav`: `state`, `mm`, `session_dir` | KEEP | `state` is NORMAL only while declared and position_reliable |
| `state/nav`: `event` (AGREE/DISAGREE/DIRECTION), `miss_streak` | RETIRE | not published by EWO |
| `state/nav`: `target`, `dir`, `position_reliable`, `degraded`, `reference_ready`, `boot_positions`, `boot_incomplete` | NEW EWO FIELD | NAVI panel, mode, alerts |
| `state/loopstat`: `pwm` | KEEP | actual PWM |
| `state/loopstat`: `nav`/`mm` | ADAPT | EWO `mm` is NAVI's internal MM even when unreliable; not used for the MM tile |
| `state/loopstat`: `miss_streak` | RETIRE | |
| `state/loopstat`: `running` | NEW EWO FIELD | AUTO ACTIVE (autoRunning) |
| `state/loopstat`: `target_distance_mm`, `target_polarity`, `pwm0_ir_motion`, `confirmed`, `missed`, `reference`, `median5`, `hall_support`, `spatial_phase`, `ir_*`, drop counters | NEW EWO FIELD | target detail, unreliability, evidence panel |
| `state/station`: `event` | KEEP | |
| `state/station`: `phase`, `station`, `off` | NEW (published, previously unused) | station line; never derived from MM |
| `state/throttle`, `state/direction`, `state/brake`, `state/estop`, `state/auto` (enrolled), `state/lowvolt`, `state/warning`, `state/session_direction`, `state/nav_ready`, `state/start_interval` | KEEP | |
| `state/block`, `state/ce`, `state/cto` | KEEP (legacy only) | not published by EWO; panels stay hidden/blank |
| `telem/voltage`, `telem/current`, `telem/power` | KEEP | |
| `telem/ir`: `ir_valid`, `ir_mmps`, `ir_coupled`, `ir_speed_reason` | KEEP | IR pKPH tile |
| `telem/ir`: `ir_applicable`, `ir_reason`, `ir_seq`, `ir_pulses` | NEW EWO FIELD | evidence panel only |
| `telem/speed` | KEEP | EWO sends the `telem/ir` payload; console IR row |
| `mm/marker` | KEEP + ADAPT | touch; EWO result copy, deduplicated with `nav/evidence` |
| `mm/speed` | RETIRE for EWO | legacy only |
| `nav/evidence` | NEW EWO TOPIC | TARGET_CONFIRMED, MISSED_MAGNET, reference/spatial/PWM=0/re-anchor events |
| `state/trace`, `diag/ir_link` | NEW EWO TOPIC | evidence panel only |

`tests/test_ngr_app_v1_12_0_ewo.py::test_contract_keys_match_firmware` fails if
the sketch's loopstat, state/nav or nav/evidence keys drift from this list.

## 2. EWO operator-state model (`_ewo_view`)

NAVI mode, from NAVI's flags only:

| Condition (published) | Mode | Display |
|---|---|---|
| position_reliable=1, boot_incomplete=1 | REFERENCE_INCOMPLETE | red: NAVI cannot confirm targets this boot |
| position_reliable=1, reference_ready=0 | REFERENCE_PENDING | amber: establishing Hall reference n/5 |
| position_reliable=1, degraded=1 | DEGRADED | amber: IR DEGRADED — HALL NAVIGATION (position still shown) |
| position_reliable=1 | NORMAL | green: HALL + IR NORMAL |
| position_reliable=0, pwm0_ir_motion>0 | UNRELIABLE | red: POSITION REFERENCE UNRELIABLE — AWAITING MM RE-ANCHOR; no MM; last established MM labelled not reliable |
| position_reliable=0, pwm0_ir_motion=0 | NOT_DECLARED | grey |

The UNRELIABLE rule relies on firmware facts: the PWM=0 counter increments only
while declared, and only a confirmed MM or a declaration restores reliability.
It clears when NAVI reports position_reliable=1.

Other operator facts: position → seeking target (polarity, mapped distance,
direction); last TARGET CONFIRMED (Hall+IR or degraded) or MISSED MMxxx —
SEEKING MMyyy for 30 s, otherwise SEEKING; motion from IR speed only; control
MANUAL / AUTO ENROLLED — NOT RUNNING / AUTO ACTIVE; station phase from the
station controller (APPROACHING, IN STATION ZONE, STOPPING, STOPPED — DWELL,
DEPARTING, DEPARTED).

Operator alerts: E-STOP, low voltage, reference incomplete, position
unreliable, IR degraded, recent Missed Magnet, station MISSED/PHASE_TIMEOUT,
NAVI state not received, stale telemetry. Everything else (Hall reference and
source, median-of-five, Hall support, spatial phase/events, IR health,
readiness, raw optical reason incl. INADEQUATE_CONTRAST, epoch, source MAC,
counters, recent events) is in the collapsed NAVI EVIDENCE panel.

## 3. Retired display for EWO locomotives

QUORUM pill, polarity AGREE/DISAGREE panel and console `%`, landmark line, and
the MM-derived station countdown (it decided station occupancy from MM). The
console chip RUNNING is renamed AUTO ACTIVE for all locomotives: it is
autoRunning, not motion.

## 4. Not published by the firmware (reported, not manufactured)

1. No explicit "declared" flag or unreliability cause in `state/nav`; the
   dashboard combines `position_reliable` with `pwm0_ir_motion` (above).
2. The console adapter reports `state=NORMAL` and `nav_ready=1` while the boot
   reference is pending or incomplete; only `reference_ready`/`boot_incomplete`
   reveal it (review of caf2280, MAJOR-1).
3. Station phase arrives only with station events; after a dashboard restart it
   is unknown until the next event.
4. The PWM=0 warning is sticky until re-declaration; the dashboard hides it once
   NAVI reports position_reliable=1 (raw text stays in the evidence panel).
5. No motion fact without NAVI-qualified IR speed. If the IR sender stops
   reporting while stationary, the display reads MOTION NOT MEASURED; needs a
   bench/track check.
6. `nav/evidence` is non-retained and can be dropped (`pub_drop`,
   `event_drop`); the confirmed/missed counters remain authoritative.
7. No uptime; reboot detection depends on `state/bootid` reaching the broker.

## 5. Open for David and Sam

- Whether the dashboard should withhold its AUTO request while the reference is
  pending/incomplete (not done: the firmware owns AUTO refusal).
- Whether items 1–4 above should become firmware telemetry.
- Deployment of v1.12.0 to the Pi (not done; `tools/provision_pi.sh` with
  `APP=server/ngr_app_v1_12_0.py`, rollback by redeploying v1.11.2).
