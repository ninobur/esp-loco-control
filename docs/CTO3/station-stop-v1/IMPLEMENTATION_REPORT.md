# Station Stop v1 — implementation report (QUORUM 1.9)

Date: 2026-08-08
Work item: `README.md` in this directory; CTO3_SPEC §12 step 3
Baseline: QUORUM 1.8, accepted without qualification (CODEX final
disposition 2026-08-08, decision 0017)
Status: **implemented, built both profiles, committed. NOT pushed, NOT
flashed** — awaiting CODEX review per the README's handoff rule.

---

## Did the routine already exist?

**Yes — complete, reachable, and correct.** The full R21 phase chain is in
the flashed 1.8: `ST_IDLE → ST_APPROACH → ST_FINAL → ST_RAMP → ST_DWELL →
ST_DEPART → RESET` (`serviceStations()`,
[QUORUM.ino:1716](../../../firmware/QUORUM/QUORUM.ino)), publishing every
event the README expects, in the README's order. Arches is defined centre
107, zone 60, final 45, stop offset +2; dwell 15 000 ms; zero-ramp primary
trigger is M+2 (`TRIGGER_M2_REACHED`) with the 5 s M+1 timeout as
fallback, exactly as the README describes.

Authority audit, per the assignment:

- **Chamber gate:** `serviceStations()` returns unless
  `autoRunning && navPositionUsable() && navDir != MAP_UNSET`. The two
  fallbacks that run above that line (APPROACH/RAMP 120 s phase timeout,
  M+1 5 s timeout) are themselves `autoRunning`-gated. No station PWM
  write can escape the AUTO chamber.
- **PWM ownership:** all station writes go through
  `requestPwm/requestPwmOver`; `servicePwmRamp()` remains the sole
  actuator; the NEUTRAL interlock and E-stop clamp sit below it.
- **Exits, all verified present:** phase timeout → `PHASE_TIMEOUT` reset
  (cruise restored only if `navPositionUsable()`); overshoot > +5 →
  `MISSED` reset; dispatcher STOP → `autoRunning=false`, PWM 0, reset
  (enlistment retained — chamber boundary honoured, `STOP_IGNORED` if not
  enlisted); RELEASE → drops both flags, PWM 0, reset, warning; E-STOP →
  immediate clamp, clearing drops to NEUTRAL; NO_QUORUM entry → controlled
  stop (AUTO only) + `stationReset` at §2.5 step 2a. ST_DEPART is
  deliberately exempt (`DEPARTURE_SLOW` notice instead) per the R21
  design.

## The exact activation gap

**One:** the arming loop considered **all four stations**. Nothing selects
a destination. On a CCW run Otto would perform Arches correctly — then
also arm Bamboo, Patio, and Grillers as reached. The README's acceptance
gate requires that no other station arm during the trial. This is
precisely README open question 2, resolved by the operator's instruction:
smallest mission-layer mechanism, compile-time preferred.

## The change (QUORUM 1.9)

Three pieces, +39/−2 lines, both files in `firmware/QUORUM/`:

1. **Otto's profile** (`LL_LocoConfig_9950011.h`):
   `#define MISSION_ONLY_STATION "Arches"` with an explanatory block.
2. **Sketch** — one predicate beside the STATIONS table:

   ```c
   #ifdef MISSION_ONLY_STATION
   static inline bool stationEnabled(uint8_t i){ return strcmp(STATIONS[i].name, MISSION_ONLY_STATION)==0; }
   #else
   static inline bool stationEnabled(uint8_t i){ (void)i; return true; }
   #endif
   ```

   and **one added line** in the arming loop:
   `if(!stationEnabled(i)) continue;`
3. **Version bump** to `QUORUM_1_9` with a header entry (version-bump
   rule; the validated 1.8 source is untouched in history).

Keyed on the station **name** so a table reorder cannot silently change
the destination. Skipped stations fall through to the existing
keep-cruise branch — Otto passes them at section cruise with no state
change. Not touched: phase logic, PWM ownership, timeouts, exits, MQTT
topics/payloads, navigation, the 1.8 motion gate, ESP-NOW/CTO2 compat
(QUORUM contains no ESP-NOW), both-profile support, one-sketch rule.

## Verification

| Step | Result |
|---|---|
| Baseline build (Otto, pre-edit) | 982 475 B — matches the flashed 1.8 exactly |
| Otto build (post-edit) | 982 539 B (+64) |
| Toby build (post-edit, selector swapped and restored byte-identical) | **982 475 B — identical to pre-change baseline count**; with no define the constant-true predicate folds away, so Toby's binary differs only by the version string. Station Stop v1 cannot activate for Toby: the define does not exist in his profile |
| `git diff --check` | clean |
| Diff scope | the two firmware files only; unrelated untracked files preserved; no `git add -A` |

**Sequence trace (Otto, CCW, declared before Arches):** arming window is
`o ∈ (−13, −12]` relative to centre 107 → `ARMED` at M−12; derived-ramp
`APPROACH` steps cruise→60 across −10…−6; `ZONE_HOLD` 60 through −5…−1;
`FINAL_APPROACH` at 0; `FINAL_TARGET` M (zone) and M+1 (45, fallback
clock starts); `ZERO_RAMP TRIGGER_M2_REACHED` at o ≥ +2; `DWELL_BEGIN`
when actualPwm reaches 0; 15 s; `DWELL_COMPLETE` → cruise;
`DEPARTURE_COMPLETE` + `RESET/DEPARTED` at o ≥ +5. With the filter, no
other station can enter this chain at all.

## Assumptions recorded (operator may overrule)

- Selection keys on the station name string (`"Arches"`).
- Version bumped to 1_9 rather than editing 1_8 in place.
- Toby intentionally retains all-stations arming (no define), which is his
  current field behaviour.

## How the dashboard blocker affects field validation

The routine activates only on genuine AUTO-running authority: enrollment
(`cmd/auto 1`) then dispatcher GO (`ngr/dispatcher/cmd/go/9950011`),
passing the seven firmware gates. The live console (v1.10.9) cannot
visibly enroll (the enlisted/running display conflation,
`NGR_DASHBOARD_FINDINGS_20260807.md` / authority-alignment spec §4) and
its per-locomotive GO buttons exist only on the dispatcher page. Raw
`mosquitto_pub` would exercise the routine today, but the operator has
ruled out Terminal operation. **Field validation therefore waits on the
console authority-alignment work (v1.10.10), not on firmware.** Nothing
in this change bypasses or weakens the enrollment boundary.

## Handoff

Per the README: this commit goes to CODEX for review before any field
run. Not pushed, not flashed. The trial procedure, preconditions,
acceptance gate (three consecutive qualifying cycles), and evidence
package are as specified in `README.md` and are unchanged by this report.
