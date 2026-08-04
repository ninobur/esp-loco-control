# Dashboard v1.10.2 — implementation report

**Describes commit 5a0310f (`server/ngr_app_v1_10_2.py`).** Operator ruling:
the dashboard must never say no to a manual operator. Field evidence
2026-08-03 (Toby tab): re-declaration demanded after an ordinary manual
stop; confirmation wording waited on "OTTO" regardless of tab; Forward never
illuminated; timeout warnings fired for confirmations sent to the wrong
locomotive; the throttle locked at session start.

## The six changes

1. **Per-tab binding.** Audit result: every element binding and fetch was
   already per-`{{ slug }}` (no hardcoded `9950011` in the loco template or
   its JS) — the defect was *wording*: ~20 user-facing strings said OTTO on
   every tab, so on Toby the operator read another locomotive's name on
   waits that then failed to resolve for unrelated firmware-generation
   reasons. All strings are now generic or use the active tab's name (`var
   LOCO`). The server's session-reset log line is generic too. The
   dispatcher console's OTTO/TOBY column headers name both locomotives by
   design and are unchanged.
2. **Declaration mirrors the locomotive.** `declared` is derived solely
   from the loco's reported nav state (`TRACKING`/`NORMAL`/`EVALUATING`).
   A manual stop changes nothing. Re-declaration is indicated only on
   loco-reported `UNSET` (its reboot, direction change, or the operator's
   own choice) or the QUORUM terminal states' own wording. The interval
   badge is neutral ("NO INTERVAL DECLARED"), never a demand.
3. **Throttle never locked in MANUAL.** The startup-order gate and the
   v1.10.1 session latch are gone; throttle, brake and motor direction are
   disabled only by the AUTO chamber (dispatcher control). Undeclared
   operation is signalled by "POSITION NOT DECLARED — NAVIGATION WILL NOT
   TRACK" and the empty MM tile. AUTO still requires a declared position —
   the firmware's `GO_REFUSED` path, untouched.
4. **Confirmations inform, never gate.** Pending confirmations disable
   nothing; press-lockouts reduced to a 1 s same-value double-tap bounce
   guard; timeout text "NO CONFIRMATION — COMMAND MAY NOT HAVE ARRIVED".
   Direction echo binds to the active loco's `state/direction` integer
   (SOLONAV 2_22 and QUORUM alike), with the `DIRECTION` nav event as the
   per-press freshness stamp where the firmware publishes it.
5. **Refusal inventory** — table below.
6. **Polarity agreement tile** per locomotive: session AGREE/DISAGREE
   counts, percentage (green ≥90, amber ≥60, red below), last ten verdicts
   as green/red ticks with mm numbers, from the active loco's nav events,
   reset on its epoch. (Diagnosed a noisy cable at 27% and a flipped sensor
   at 100% in the same week.)

## Change 5 — refusal inventory

Every point where v1.10.1 disabled or blocked a control reachable in
MANUAL. Presumption: removal.

| # | v1.10.1 refusal | Disposition |
|---|---|---|
| 1 | Throttle disabled until nav_ready confirmed (startup-order gate) | **Removed** |
| 2 | Throttle disabled while telemetry silent/stale (pre-latch `!alive`) | **Removed** |
| 3 | Throttle re-lock on epoch reset until re-confirmation (session latch) | **Removed** — controls are independent of declaration entirely |
| 4 | Motor-direction press-lockout while a confirmation was pending (up to 6 s) | **Removed** — replaced by a 1 s *same-value* bounce guard. Justification: it absorbs a physical double-tap glitch without ever refusing a deliberate command — a different value, or any press after 1 s, always sends |
| 5 | SET INTERVAL disabled while telemetry silent ("CANNOT SET INTERVAL") | **Removed** — silence is a warning; the declare may simply not arrive |
| 6 | SET INTERVAL disabled while motion UNKNOWN | **Removed** — informative warning "MOTION UNKNOWN — CONFIRM <name> IS STOPPED BEFORE DECLARING" |
| 7 | SET INTERVAL disabled until session direction confirmed ("SET SESSION DIRECTION FIRST") | **Removed** — the firmware itself refuses (`START_INTERVAL_REFUSED SET_SESSION_DIRECTION_FIRST`) and explains on the warning line; the dashboard does not pre-empt the locomotive's own answer |
| 8 | SET INTERVAL button disabled while a confirmation was pending | **Removed** — 1 s double-tap guard only |
| 9 | **SET INTERVAL disabled while the loco REPORTS motion** | **RETAINED — survivor.** Protects declaration integrity (2026-08-01 mid-motion declare corruption). It refuses a *declare*, never driving, and only on the locomotive's own reported motion |
| 10 | **E-STOP latch semantics** (active/clear toggle) | **RETAINED — survivor.** The operator's own act; the control itself is never disabled in any state |
| 11 | Controls disabled while `auto == 1` (CTO) | **Retained, out of scope** — the AUTO chamber is dispatcher control, not a dashboard refusal of a manual operator; END CTO is the release path |

## Verification (simulated broker, both loco IDs)

- **Cross-binding:** on the Toby tab, a triple-tap of Forward published
  exactly one `ngr/loco/9950012/cmd/direction 2`; an Otto (`9950011`)
  direction echo did **not** illuminate Toby's button; Toby's own
  `state/direction` + `DIRECTION` event did (Reverse lit, pending and note
  cleared).
- **Fresh MANUAL load:** throttle and brake live immediately; status
  "POSITION NOT DECLARED — NAVIGATION WILL NOT TRACK"; MM tile "—";
  SET INTERVAL enabled (loco reported stopped).
- **Manual stop while TRACKING:** declaration stood — status "TRACKING —
  MM 017", badge "INTERVAL SET — 015-016 — CONFIRMED", no re-declaration
  demand.
- **Loco-reported UNSET (live bootid reboot):** epoch bumped, agreement
  tally and badges reset, re-declare indicated by the status line only;
  throttle still live; E-STOP live.
- **Agreement tile:** 5 AGREE + 1 DISAGREE → 83% amber, ticks
  018 019 020 021(red) 022 023; reset to "—"/0/0 on epoch.
- **AUTO without declaration:** unchanged firmware `GO_REFUSED` path; the
  dashboard's mode control does not gate.
- **E-STOP:** enabled in every state (no code path ever disables it).
- v1.10.0/1.10.1 freshness, epoch, and retained-message behavior
  regressed nowhere: tiles still age and gray, retained replays still
  ignored, epoch still resets local staging.

## Deploy

```
scp server/ngr_app_v1_10_2.py david@192.168.68.142:/home/david/ngr_app.py.new
# on the Pi:
cp ~/ngr_app.py ~/ngr_app.py.backup_before_v1_10_2 && mv ~/ngr_app.py.new ~/ngr_app.py && sudo systemctl restart ngr-app
```
