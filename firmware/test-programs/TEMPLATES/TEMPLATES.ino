/*
 * ============================================================================
 * QUORUM_1_12  —  Ninobur Garden Railway single-locomotive navigation
 * ============================================================================
 * Successor to SOLONAV (v2.22 final). QUORUM navigator per spec R21.
 *
 * ---------------------------------------------------------------------------
 * v1.12B — PASSENGER-GENTLE STATION RAMPS (operator ruling 2026-08-09)
 * ---------------------------------------------------------------------------
 * "Passenger stops must be gentle. The customers complain, although the
 * chiropractors love it." Station stops and departures abandon the
 * duration-based requestPwmOver (whose step rate depended on the span,
 * leaving departures 2.4x steeper than a manual crawl) for the per-step
 * pacing everything else uses: UP 150 ms/step, DOWN 200 ms/step. A
 * departure 0->90 now takes ~13.5 s to full cruise; the final stop from
 * 45 takes ~9 s. Watch on first laps: landing distance grows with the
 * gentler stop (retune stopOffsets per station if needed), and the slow
 * pull-out may occasionally trip the DEPARTURE_SLOW advisory (cosmetic).
 * STOP/DEPART_RAMP_MS retired. Approach/zone trims keep duration ramps.
 *
 * ---------------------------------------------------------------------------
 * v1.12 — OPERATOR SPEED/RAMP TUNING (base: 1.10 per ruling; 1.11/1.11B
 * are diagnostic stubs and contribute nothing here)
 * ---------------------------------------------------------------------------
 * Two operating tunables, operator-directed 2026-08-09:
 *   CRUISE_PWM     100 -> 90    (open-main cruise; the derived approach
 *                                ramp adapts automatically by design)
 *   DEPART_RAMP_MS 2800 -> 5600 (station departure half as steep)
 *   STOP_RAMP_MS   2800 -> 5600 (v1.12A, same session: final stop ramp
 *                                half as steep. NOTE: gentler braking
 *                                lengthens the distance covered during
 *                                the zero ramp, so observed platform
 *                                landings may shift slightly beyond the
 *                                field-tuned stopOffsets; watch the
 *                                first laps and retune stopOffset per
 *                                station if needed)
 * NOT rescaled, deliberately: the Grillers CW climb segment keeps its
 * absolute cruisePwm 120 (a climb boost is about the hill, not the base),
 * and STOP_RAMP_MS keeps 2800 (braking feel unchanged). Otto's profile
 * carries the all-stations state (the v1 Arches-only define removed,
 * operator-directed 2026-08-08). The transport-resilience work
 * (QUORUM_1_12_TRANSPORT_RESILIENCE_SPEC.md) was reserved here as 1.13;
 * it is still unimplemented and now lands as **1.14**. 1.13 was taken by
 * the HARD_BOUND advisory below — named, flashed to Otto and field-tested
 * on 2026-08-11 before this reservation was noticed. The number is not
 * reclaimed because committed field evidence
 * (field-records/logs/20260811_QUORUM_1_13_beta_otto.log) carries
 * "sketch":"QUORUM_1_13" and renaming would orphan it.
 *
 * ---------------------------------------------------------------------------
 * v1.10 — CONSOLE-AUTHORITY FIRMWARE ITEMS P11/P13/P14 (spec Draft 5.1)
 * ---------------------------------------------------------------------------
 * Three changes, each an explicit operator ruling, from
 * docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md (five drafts, eight
 * reviews, CODEX-approved for implementation):
 *
 *  P11 (R12/R15/H2) — ENLISTMENT GUARDS. cmd/auto 1 is refused, with a
 *      published reason, unless the locomotive is de-energised, oriented
 *      (session direction set), in gear (not NEUTRAL), and navigating
 *      (not UNSET, not NO_QUORUM; EVALUATING usable). Enlistment is a
 *      deliberate act; the rule lives where the truth lives, whatever the
 *      command source. cmd/auto 0 is NEVER refused — disenrollment is a
 *      safety action.
 *
 *  P13 (R14) — COMMAND RESPONSES ALWAYS OBSERVABLE. *_REFUSED and
 *      STOP_IGNORED bypass stationPublish()'s transition dedup and carry
 *      a monotonic "seq"; a repeated command yields a visible repeated
 *      response, and a changed reason is never suppressed.
 *
 *  P14 (R13) — E-STOP PRESERVES DIRECTION, both branches. The interlock
 *      is `estopped` itself (PWM clamped every pass; BEGIN refused with
 *      ESTOP_ACTIVE). The dispatcher can now restart an enlisted,
 *      E-stopped locomotive with BEGIN alone. Clear publishes
 *      ESTOP_CLEARED (was ESTOP_CLEARED_NEUTRAL — the name would now lie).
 *
 * Navigation, detection, the 1.8 baseline motion gate, and the 1.9
 * station mission filter are unchanged.
 *
 * ---------------------------------------------------------------------------
 * v1.9 — CTO3 STATION STOP v1: MISSION STATION FILTER (spec §12 step 3)
 * ---------------------------------------------------------------------------
 * The complete R21 station phase chain (ARMED -> APPROACH/ZONE_HOLD ->
 * FINAL_APPROACH -> FINAL_TARGET -> ZERO_RAMP -> DWELL -> DEPART ->
 * RESET/DEPARTED) already existed and is UNCHANGED. The one missing v1
 * capability was destination selection: the arming loop considered all four
 * stations. A profile may now define MISSION_ONLY_STATION ("Arches" in
 * Otto's) and the arming loop skips every other station; skipped stations
 * fall through to the existing keep-cruise branch. No phase logic, PWM
 * ownership, timeout, exit, MQTT topic, or navigation behaviour changed.
 * Toby's profile has no define — his build arms all stations, as before.
 * One added line in the arming loop; one inline predicate; nothing else.
 *
 * ---------------------------------------------------------------------------
 * v1.8 — BASELINE ADAPTS ONLY IN MOTION (decision 0017; Sam+CODEX reviewed)
 * ---------------------------------------------------------------------------
 * One guard line. The Hall median baseline (128 samples x 500 ms) was
 * pushed unconditionally, "in or out of an event" — but its robustness to
 * magnets rests on an assumption that only holds while MOVING: a traversed
 * magnet contributes <=1 sample in 128; a PARKED-ON magnet contributes all
 * of them, and within ~32-64 s the reference BECOMES the magnet and
 * recomputeThresholds() re-centres the +/-38-count window on it (observed
 * magnet excursions: -254/+182). On departure every real N magnet then
 * arrives as an S reading and every S magnet is swallowed — the 2026-08-06
 * NO_QUORUM with the flat score vector [6,4,5,4,4,6].
 *
 * The fix: updateBaseline() pushes only while there is positive evidence
 * of tractive motion (actualPwm > MOTOR_DEAD_ZONE_PWM). NOT proof of
 * rest — a loco can coast downhill at PWM 0 — but the error cases are
 * asymmetric: refusing to adapt while secretly moving merely postpones
 * adaptation ~30 s; adapting while secretly parked can destroy the
 * reference. Freeze is the safe side. Boot calibration establishes the
 * baseline; motion maintains it; rest preserves it (0007's "persist only
 * the proven state", applied to a live reference; the Hall twin of 0006).
 *
 * Consequence, deliberate: parked on a magnet, the event that opens on
 * arrival now stays open for the whole dwell and closes at departure as
 * ONE marker — polarity from the arrival pole, detectedAtMs stamped at
 * arrival, durationMs saturating at 65535. Correct count, correct place.
 * See the SENSOR layer note below; acceptance matrix in
 * docs/QUORUM_BASELINE_MOTION_GATE_SPEC.md §5.
 *
 * Residual, accepted: a stall ABOVE the dead zone parked exactly on a
 * magnet still poisons ("believed moving" is belief, not measurement).
 * The seam for the real motion witness is decision 0005's hook.
 *
 * ---------------------------------------------------------------------------
 * v1.7 — INA219 TELEMETRY RESTORED (decision 0012; CTO3 plan step 2)
 * ---------------------------------------------------------------------------
 * SOLONAV 2.1 dropped the INA219 service without a record and QUORUM
 * inherited the gap; the dashboard's power tile has read a retained ghost
 * ever since. Restored per the r12 pattern, evidence layer only — no
 * control path reads a voltage in this version:
 *
 *   * telem/voltage, telem/current, telem/power every 5 s, retained as in
 *     r12 (fresh publishes overwrite the broker's stale ghosts).
 *   * state/lowvolt flag against LOW_VOLTAGE_THRESHOLD_V (14.4 V, r12
 *     value), published on change and reseeded retained at every connect —
 *     but ONLY when a sensor actually answered. A locomotive with no
 *     working INA219 publishes no "voltage is fine", because unknown
 *     reported as clear is the one inversion CTO3 §0/§14 forbids.
 *     A FLAG, not a cutoff: no PWM site reads it. Whether a low-voltage
 *     response actuates anything — and in which chamber it could ever be
 *     allowed to — is an open design decision (see 0012/0002 lineage).
 *   * "pwm" and "v" added to the mm/marker payload for the CTO3 §9
 *     calibration table: pwm is pwmActualAtDetect (already sampled at event
 *     open, §3), v is bus voltage read at drain on the loop thread. This
 *     AMENDS the §5.1 marker payload contract (was: raw fields + timing,
 *     "NOTHING else") — flagged for CODEX review, worst case recomputed
 *     below the 320-byte buffer. CTO3_SPEC §9 said "alongside pwm=/dist=",
 *     describing the SOLONAV-era key=value line this JSON contract
 *     replaced; pwm was NOT in fact on the line, and without it the
 *     segment×direction×PWM table cannot be built, so it rides along.
 *   * Boot tolerates a missing/faulted INA219 (ina219Available, r12
 *     pattern): "v":null and silent telem, never a blocked run. Otto's
 *     open INA219 hardware fault is exactly this case until repaired.
 *   * I2C strictly on the loop thread (setup + drainMarkers +
 *     serviceInaTelemetry). hallTask and networkTask never touch Wire.
 *
 * ---------------------------------------------------------------------------
 * v1.6 — THE BICAMERAL BOUNDARY, as the operator states it
 * ---------------------------------------------------------------------------
 * ONE control sketch, two chambers, and EXACTLY THREE CROSSINGS between them:
 *
 *   E-STOP       the operator's, working in both chambers and every state.
 *   ENLISTMENT   manual -> auto. The locomotive's OWN act: it volunteers and
 *                gives up control (cmd/auto).
 *   RELEASE      auto -> manual, from the dispatcher side
 *                (cmd/dispatcher_release).
 *
 * Inside AUTO the locomotive runs itself; the dispatcher sends only go, stop
 * and estop. Outside enlistment the automatic side touches NOTHING — it does
 * not drive the manual side, ever. Either side is usable from this one
 * firmware, so no reflash is needed to change how the locomotive is run.
 *
 * Three gaps closed to make that true:
 *   * DISPATCHER STOP reached into MANUAL. It zeroed the throttle of a
 *     locomotive that had never enlisted — the automatic side driving the
 *     manual side, the one thing the chambers must never do to each other.
 *     Now requires enlistment.
 *   * THE DISPATCHER'S E-STOP HAD NEVER WORKED. The console publishes to a
 *     broadcast topic, ngr/dispatcher/cmd/estop, that nothing in this lineage
 *     ever subscribed to — so the big red button on the console page was
 *     inert on SOLONAV and on every QUORUM before this one. Now subscribed,
 *     handled, and on the callback bypass so it cannot queue behind commands.
 *   * DIRECTION CHANGES were refused whenever motorIsMoving(), true at PWM 1,
 *     and discarded in silence. Ported from the MANUAL reference sketch:
 *     refuse above MOTOR_DEAD_ZONE_PWM and say the number to come below;
 *     at or under it, snap to zero as E-stop does, apply the direction and
 *     restore the operator's throttle. AUTO still owns the motor once the
 *     locomotive has enlisted.
 *
 * v1.5 — CODEX review of 1.4, three findings. Navigator control logic and the
 * twenty certified properties are untouched; §1's "advance by one on every
 * accepted event" is preserved, not weakened.
 *   F3  REVERSING MID-INTERVAL was wrong by one marker, by construction.
 *       navMm is the marker last REACHED and the locomotive sits between it
 *       and the next; reverse, and the next marker met is navMm itself — but
 *       the standard advance moved past it first. applyDirection() now steps
 *       the odometer back along the OLD direction so the advance lands
 *       correctly. QUORUM would have found this as offset -1 eventually;
 *       now it never has to.
 *   F4  cmd/session_direction refused while moving but NOT under AUTO, and
 *       during a station dwell both PWM values are zero — so a raw MQTT
 *       command could reset the station machine mid-session. Now matches
 *       cmd/direction.
 *   F6  nav payloads reported NEUTRAL as "REV". Three-way motorDirName().
 *
 * v1.4 — CODEX constitutional hardening after ratifying the bicameral
 * property: the M+1 station fallback gets its explicit autoRunning gate
 * (reclassified AUTO-chamber station automation, not a motor-safety fact —
 * behaviourally unreachable change today, constitutionally load-bearing
 * tomorrow), and the terminal-entry log tells the truth in both chambers
 * ("AUTO stop requested" / "MANUAL, motor unchanged").
 *
 * v1.3 — BICAMERAL CONTROL (operator ruling, spec §0.2). All NGR locomotive
 * controllers have two chambers: MANUAL — the operator is sovereign;
 * navigation observes, records, publishes and warns, but NEVER writes to
 * the motor. AUTO — navigation acts with full authority. E-STOP belongs to
 * the operator and works in every chamber and every state. The NO_QUORUM
 * terminal stop now gates on autoRunning; 1.0-1.2 issued it
 * unconditionally, a navigation override of a manual operator that v2.22's
 * LOST handler never permitted. Full call-site audit in
 * docs/QUORUM_1_3_IMPLEMENTATION_REPORT.md.
 *
 * v1.2 — generation counter closes the ABA race in retained-state
 * reconciliation (CODEX 1.1 review): the success guard compared only the
 * enum value, so a NEWER commit with the SAME value (snapshot B replacing
 * snapshot A, or SNAPSHOT->CLEAR->SNAPSHOT during a slow publish) could be
 * marked reconciled without ever being published. Every commit now bumps a
 * generation under the mux; success clears the reconcile flag only if state
 * AND generation both still match.
 *
 * v1.1 — terminal evidence fixes per the CODEX implementation review of 1.0
 * (docs/QUORUM_1_0_CODEX_FINDINGS.md). Navigator control logic untouched:
 *   F1  terminal snapshot reports OFFSETS for ld/ru, not candidate indices
 *   F2  tear-free snapshot handoff: portMUX critical section; the network
 *       task copies under the mux and publishes from its own copy
 *   F3  desired retained state is persistent; a per-connection reconcile
 *       flag re-syncs the broker after EVERY reconnect, indefinitely
 *   F4  alert payload compacted below the 512-byte transport, worst case
 *       shown; oversize builds publish ALERT_OVERSIZE, never truncated JSON
 *   F5  event-bearing AGREE/DISAGREE ride pubMarker() (durable, in-order);
 *       markerPubQueue 64 -> 128 slots (~2 events per marker now)
 *
 * A locomotive that knows where it has been, where it is, and what is
 * possible next.
 *
 * ---------------------------------------------------------------------------
 * QUORUM (docs/QUORUM_v3_0_implementation_spec.md, Revision 20)
 * ---------------------------------------------------------------------------
 * Layer 3 is replaced. The tally navigator (navConfidence, LOST, windowed
 * reacquisition) could express HOW MUCH it was disagreeing but not WHICH
 * position it might be in; when the tally emptied it discarded position and
 * rebuilt from nothing. QUORUM inverts that:
 *
 *   "I am on the tracks. I am not flying. I knew where I was a minute ago."
 *
 *   * One disagreement is free. The odometer still advances; position is held.
 *   * Three consecutive misses wake NAV_EVALUATING: six candidate offsets
 *     { -1, 0, +1, +2, +3, +4 } are scored against the evidence ring. Speed
 *     is NOT reduced while evaluating.
 *   * A unique two-point lead adopts the offset — one correction, applied
 *     once, validated by the next agreement.
 *   * Twelve readings without a margin, or a second failed adoption, is
 *     NAV_NO_QUORUM: controlled stop, terminal evidence snapshot (retained,
 *     via the desired-retained-state slot, never a queue), operator
 *     re-declares position to recover. There is no automatic exit.
 *   * A conservation timing gate rejects phantom events: two events whose
 *     intervals sum to one expected interval are one magnet read twice.
 *
 * The detector, thresholds, DNA table, spacingMm[] and the transport queues
 * are untouched. dnaMatch()/dnaPush()/dnaBuf remain as dead code by
 * instruction — present, unreferenced, compiling.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS IS A REWRITE AND NOT ANOTHER REVISION
 * ---------------------------------------------------------------------------
 * The previous lineage asked "where am I?" from scratch at every marker: take
 * the last twelve polarity readings, look them up, announce a position with
 * total confidence. History was the junior partner. When the pattern matcher
 * disagreed with the odometer, the matcher won — which is how a locomotive at
 * MM133 came to believe, at certainty 1.000, that it was at MM105 travelling
 * the other way.
 *
 * This inverts it. Position comes from history and the map. A magnet reading
 * is a vote, not a verdict. One bad read costs a confidence point; it cannot
 * teleport the train.
 *
 * The second change is that the DETECTOR NO LONGER MAKES DECISIONS. It reports
 * what it saw, with quality attached, and never rejects anything on its own
 * authority. Rejection needs context — what marker is due, which way we are
 * going, how fast — and the detector has none of that. Every rejection rule it
 * was ever given eventually threw away something real.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE 2026-07-26 DATA ESTABLISHED
 * ---------------------------------------------------------------------------
 *   * The median baseline works. `delta` stayed within a few counts for a
 *     whole session and the tracker never once froze. Carried over unchanged.
 *   * The Hall task works. Task gaps of 1–4 ms while the main loop stalled for
 *     20 seconds. Carried over unchanged.
 *   * Marker reading is excellent at full signal: 20 of 20, then 18 of 19.
 *   * Position was still lost, and the recovery jumped 26 markers.
 *   * Every state machine that wedged did so because a state had no exit.
 *     Four separate instances in one week. Every state here has one.
 *
 * ---------------------------------------------------------------------------
 * SCOPE — SINGLE LOCOMOTIVE
 * ---------------------------------------------------------------------------
 * Multi-train awareness was prototyped here in 2.5 and has been REMOVED. It
 * does not belong in a sketch named SOLONAV. The peer table, occupancy
 * comparison and traffic-dependent lost policy are deferred until the CTO3
 * architecture requires them; the reasoning and the review findings that
 * produced them are preserved in docs/CTO3_DESIGN_NOTES.md.
 *
 * Solo on a closed loop, position uncertainty has no physical consequence.
 * The train cannot hit anything and cannot leave the track. Being lost costs
 * station stops and nothing else. Behaviour here is sized to that fact.
 *
 * ---------------------------------------------------------------------------
 * FOUR LAYERS, ONE CONTRACT EACH
 * ---------------------------------------------------------------------------
 *   1  SENSOR      raw ADC + median baseline. The baseline itself cannot
 *                  stick; an OPEN EVENT still can, if the signal never
 *                  settles inside the exit band. No guard is added for
 *                  that -- no log has yet shown it. Watch event_open_ms.
 *                  v1.8: a DWELL ON A MAGNET now legitimately holds an
 *                  event open for the whole stop (the baseline no longer
 *                  migrates to close it); it closes at departure as one
 *                  arrival-stamped marker, ms saturating at 65535. That
 *                  signature is EXPECTED, not a stuck detector. A stuck
 *                  event WHILE MOVING still self-heals: pushes continue
 *                  in motion, so migration can still close it -- which is
 *                  why the gate is on motion, not on !evActive (spec R6).
 *   2  DETECTOR    threshold crossing -> event {polarity, ms, peak, drift}.
 *                  Reports everything. Judges nothing.
 *   3  NAVIGATOR   QUORUM. Odometer is truth. Map predicts. Reading votes.
 *                  Hold position on a disagreement; wake nearby hypotheses
 *                  only on a run of failures; stop only when no candidate
 *                  fits — and then only the operator recovers it.
 *   4  OPERATIONS  station profile and PWM ramp. Consumes position and
 *                  confidence; never touches the sensor.
 *
 * Deliberately absent: baseline freeze, tracker recovery state, stuck-event
 * guard, settling qualifier, dominance classifier, track window, stability
 * test, alternating service. Every one of those existed to prop up a detector
 * that had to be right on its own. Give it a navigator that can absorb being
 * wrong and they are all unnecessary.
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_now.h>   // LAYER 5 (CTO3): loco-to-loco peer truth. Shimmed inert in tests/.
#include <pgmspace.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "LocoConfig.h"

// ---------------------------------------------------------------------------
// IR TEST A (docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md). Observation only:
// IR has NO motor authority and NO navigation authority in this build — the
// only permitted effects are loop-owned observation state, counters, and new
// telemetry topics (spec §3). The gate is per-profile: Toby defines
// IR_TEST_A_ENABLED, Otto does not, and with it absent every IR path below
// compiles to an inert stub and the binary behaves as accepted Q1.16R.
// ---------------------------------------------------------------------------
#include "IRSpeedWire.h"
#if defined(IR_TEST_A_ENABLED) && IR_TEST_A_ENABLED
  #define IR_TEST_A_ON 1
#else
  #define IR_TEST_A_ON 0
#endif

// 1.13, not 1.12D: the letter suffixes through 1.12A-C were refinements of one
// theme (speed and ramp tuning). This adds a capability — the exact-or-silent
// HARD_BOUND advisory of decision 0023 — so it takes a minor. 1.11 is the
// precedent: diagnostic instrumentation, its own minor, "evidence stub, not a
// base". The name is published retained on state/bootid, which is what lets a
// capture be attributed to a build; leaving it at 1_12C would have made a beta
// log indistinguishable from a pre-advisory one.
//
// 1.14 (2026-08-13): LAYER 5 — CTO3 peer coordination, the Bubble v1 spec
// (docs/CTO3/BUBBLE_V1_SPEC.md; decisions 0030-0033). ESP-NOW peer truth at
// 2 Hz using the frozen CtoPeerPacket v3 layout, producer-applied occupancy
// bounds in markers, Q1/Q2-derived latched roles with a role-echo packet,
// one deceleration profile with a resume flag, leader hold-for-follower at
// stations, and the 0031 fleet stop enforced by absence. Solo behaviour is
// intended to be UNCHANGED: with no fresh peer ever seen, every CTO path is
// inert and the navigator/station machine run exactly as 1.13.
// NOTE: 1.14 had been pencilled for transport-resilience work when 1.13 took
// the advisory; that work moves to 1.15. Same collision as last time —
// recorded here so the librarian can object once, not twice.
// 1.14B (2026-08-14): two operator rulings about being told the truth.
//   (a) COMMAND ORDER MUST NOT MATTER. A start interval sent before the
//       session direction is now HELD and declared the moment the direction
//       arrives, instead of being refused. The old rule cost a session: one
//       locomotive's interval arrived a second early, was refused, and BEGIN
//       then reported NOT_ENROLLED_IN_AUTO — two steps from the cause — while
//       the other locomotive, given the same pair in the opposite order,
//       worked first time.
//   (b) IF BEGIN IS ACCEPTED, THE LOCOMOTIVE MOVES. BEGIN now evaluates the
//       CTO cap before committing and REFUSES with the specific condition
//       rather than publishing "GO / LAUNCH" and sitting still at PWM 0.
//
// 1.14A (2026-08-14): three operator corrections on the reviewed 1.14 base.
// A letter, not a minor — no new capability. (a) the leader's departure no
// longer depends on the follower at all, removing a deterministic deadlock;
// (b) follower hold gap 12 -> 9 markers; (c) CCW station landings one marker
// earlier. Also carries the 5 s follower dwell that was flashed on
// 2026-08-13 but never committed on its own.
//
// Deliberately NOT here: the CTO mode layer (BUBBLE/UNPAIRED/CE). It was
// briefly mixed into this branch, and CODEX correctly refused the mixture —
// unreviewed mode code plus a new echo wire version does not belong in a
// narrow behavioural test. It is preserved on agent/cto-mode-1-15 and will
// land as 1.15 with its own review and decision record.
// 1.16 (2026-08-14): QUARANTINE + NO_QUORUM SELF-RESOLUTION, the operator's
// "we are deliberately making the locos ignore what they know" session
// (docs/QUORUM_QUARANTINE_AND_SELF_RESOLUTION_PROPOSAL.md; decision 0035).
// A doubtful event is HELD, judged by its successor against the map, and
// discarded or committed — never silently promoted into the record. The
// decisive test is the 350 ms physical floor (two independent sensors);
// corroborating credentials are trailing-median flux, duration and polarity,
// no PWM model anywhere. NO_QUORUM keeps scoring: twelve fresh post-failure
// events plus a route-wide unique window match plus three confirmations
// relabel the navigator and return it to NORMAL — knowledge recovery, never
// motion recovery; AUTO stays dropped.
// 1.16R (2026-08-14, same day): THE CODEX REVIEW ROUND. Seven findings, all
// accepted; 1.16 was never flashed. R fixes: (1) SELF_RESOLVED drops
// autoRunning — the resume interlock, enforced not promised (the harness
// never calls serviceStations(), which is why no fixture could see it);
// (2) a successor-vouched commit passes the legacy conservation gate
// untested — the map authenticated it, the model gets no veto (live in the
// 2026-08-10 capture at mm 85); (4) the witness is credential-checked
// before it may testify (SUCCESSOR_SUSPECT), which also folds up slow
// consecutive phantom families; (5) the conjunction's PWM>=40 condition
// removed — PWM is a request, not a measurement (live in toby_0813_s02:
// pwm 24 and 19 events with 20-second durations were exempt from scrutiny);
// (6) resolved in the RECORD, not the code: three consistent matches TOTAL
// stands, 0035's "3 further" wording was wrong (capture datum: stricter
// counting pushes the only real-data recovery past the end of the record);
// (3, 7) are suite fixtures and document corrections. Suffix-rescue map
// fact, proven in the suite: no two candidates can share a 7-suffix on
// NGR_DNA1 (max self-agreement run at lags 1-5 is 6), so SUFFIX_RESCUE_N=7
// is the minimum unambiguous length — the 1/128 probability argument is
// retired. NOT flashed; gate is operator + CODEX re-review of 0035.
// 1.16Ra (2026-08-15): INSTRUMENTATION ONLY, from a field incident during the
// 1.16R Bubble test. Toby fleet-stopped on "peer STALE" while Otto was
// perfectly healthy on MQTT — publishing every second, nav NORMAL, and his
// CTO tx counter climbing at 2 Hz with zero errors. That counter was read as
// proof the transmitter was working. It was not: it counts esp_now_send()
// RETURNING ESP_OK, which means "queued to the driver", and no send callback
// was ever registered. A request counter was being treated as a measurement —
// the precise error decision 0024 exists to prevent, committed against the
// radio instead of the motor. A reset restored the link, which rules out
// range and masonry and points at radio state; the leading hypothesis is a
// station channel divergence (ESP-NOW peers are added with channel 0 =
// "current", and the station follows the AP, so a roam or re-association
// leaves MQTT untouched while making the locomotive invisible to its peers).
// Unprovable after the fact, because nothing reported the channel.
// This revision therefore adds: the send callback (txd/txf = what the MAC
// layer actually did, honestly limited — broadcast is unacknowledged, so
// "transmitted" never means "received"), the station channel in CTO status,
// and a CTO_CHANNEL_CHANGED alarm the moment a locomotive drifts. No
// navigation, motor, or wire-contract change: proven byte-identical to 1.16R
// across all 43 replays (12 capture segments + 31 fixtures, both streams).
// 1.16Rb (2026-08-16): DECISION 0037 — a pairing dissolves when nose-tail
// order inverts. Operator ruling, from the field: Toby was paused, Otto
// lapped the entire route, and arrived 12 markers BEHIND the locomotive he
// was still latched as LEADER of — braking for it while the console called
// him the leader. 0032 latches roles to stop them flapping and lists four
// dissolutions, none of which is "the physical order reversed", because the
// model assumes a linear order. A loop has none: lap another locomotive, or
// merely stop while it runs, and the ordering genuinely inverts.
// The cost was bounded — the role drives ONLY the follower dwell, and
// traffic protection is gap-based and never consults it, which is why Otto
// braked correctly for a train he thought he led. Bounded is not harmless:
// a role that can be false cannot be built on.
// Dissolution is CLEAR (the new near side must beat the far side by the
// 12-marker pairing range, so a near-diametric pair cannot chatter) and
// CONFIRMED (3 consecutive passes), then the role is re-derived on the same
// pass from real geometry. A stale or silent partner still does NOT dissolve
// — that stays 0031's jurisdiction, and the tests pin it.
// CTO also gained its first tests ever: the harness can now inject peer
// status and role-echo packets, so formation, all five dissolutions, the
// confirmation gate, the anti-chatter margin and the dwell are all pinned.
// Until now the pairing layer was dead code under replay.
// NOTE 1.15 is reserved for the CTO mode expansion (operator, branch
// agent/cto-mode-1-15); the 1.14 header's note reserving 1.15 for transport
// resilience is stale — that work moves to 1.17. Third collision; librarian
// beware.
#define SKETCH_NAME "TEMPLATES_0_3B"

// Broker lives here, not in LocoConfig.h — same as the previous lineage.
#define MQTT_BROKER "192.168.68.142"
#define MQTT_PORT   1883

// INA219 service (v1.7, decision 0012) — r12 values. The threshold lived in
// the sketch in r12, not the config headers; both locomotives run 4S packs,
// so it stays here. The Blynk-era voltage constants still present in the
// LL_LocoConfig headers are dead config this sketch does not read.
#define INA219_TELEM_INTERVAL_MS 5000UL
#define LOW_VOLTAGE_THRESHOLD_V  14.4f

// ===========================================================================
// TYPES — must precede every function.
// The Arduino IDE auto-generates function prototypes and inserts them near the
// top of the .ino, ABOVE anything declared further down. A type used in a
// signature but defined mid-file produces "does not name a type" even though
// the code reads correctly. Keep all structs and enums in this block.
// ===========================================================================
enum MapDirection : int8_t { MAP_UNSET=0, MAP_CW=1, MAP_CCW=-1 };

struct StationDefinition {
  const char* name;
  uint8_t centerMm;
  uint8_t zonePwm;    // held from M-5 through M: the station approach speed
  uint8_t finalPwm;   // M+1, the speed the stop is made from
  int8_t  stopOffset; // marker past centre at which the zero ramp begins
};

struct MarkerEvent {
  uint8_t       polarity;      // 1=N 0=S
  int           peak;
  uint16_t      durationMs;
  int16_t       baselineDrift; // counts the baseline moved during the event
  unsigned long detectedAtMs;  // captured at detection, not at processing
  // §3: both PWM values, sampled at event OPEN (the instant detectedAtMs is
  // stamped), so the timing gate is a measurement, not an approximation.
  uint8_t       pwmActualAtDetect;     // actualPwm at event open
  uint8_t       pwmCommandedAtDetect;  // commandedPwm at event open
};

// QUORUM evidence ring entry (§2.4): the reading and the odometer value it was
// recorded against. Scoring uses the navMm recorded WITH the reading.
struct RingEntry { uint8_t polarity; uint8_t navMm; };

// TEMPLATES 0.3 — one candidate's identity-test scorecard. Declared HERE,
// beside the other early types, because the Arduino prototype generator
// cannot hoist functions whose signatures use mid-file types (the same trap
// that bit the marker types once already). Body of the R3 machinery lives
// above drainMarkers().
struct R3Score {
  int      sPol, sStr, sDur, sTim, sSeq, sIr;   // 0..100, -1 = unavailable
  uint16_t wAvail;                               // denominator actually used
  uint8_t  conf;                                 // 0..100
  uint16_t expPeak, expDur, durN90;
  uint32_t expDt, mapSpanMm;
};

// TEMPLATES 0.3B — the navigator's per-event disposition and R3's proposal.
// Early for the same reason as R3Score: the prototype generator cannot hoist
// signatures that use mid-file types.
enum NavDisposition : uint8_t {
  NAV_D_ACCEPTED = 0,       // acceptEvent() ran for THIS event
  NAV_D_QUARANTINED,        // held as qPending, judged by its successor
  NAV_D_PHANTOM_REJECTED,   // conservation gate refused (inherited, preserved)
  NAV_D_NO_POSITION,        // received only
  NAV_D_NO_DIR,             // received only
  NAV_D_NOT_PRESENTED,      // R3 held the event; the navigator never saw it
};

struct R3Proposal {
  bool     present;        // an identity decision exists (false = bypass)
  bool     toNav;          // hand the event to the navigator
  uint8_t  corrOff;        // proposed extra advance (0 = ordinary confirm)
  uint8_t  mmAtProposal;
  uint8_t  expectMm, decidedMm, bestOff;
  uint8_t  confExp;
  uint8_t  excludedN;      // candidates vetoed as physically unreachable
  R3Score  dec;            // the deciding candidate's scorecard
  const char* proposed;    // outcome string
  unsigned long dtAcc;
  bool     irAvail; float irDistMm;
};


// Outbound and inbound MQTT messages cross the loop<->network task boundary as
// values on a queue, so no locomotive-state thread ever touches the radio and
// the network never touches locomotive state. Topic pointers in PubMsg are safe
// to store: the T_* topic strings are static char arrays that never move.
// payload[512] covers today's largest payload (navPublishState, 384) and the
// v3.0 QUORUM payload (512).
struct PubMsg { const char* topic; char payload[512]; bool retain; };
struct CmdMsg { char topic[64]; char payload[128]; };

// §2: the QUORUM state machine. NAV_TRACKING and NAV_LOST are deleted.
enum NavState     : uint8_t { NAV_UNSET=0, NAV_NORMAL, NAV_EVALUATING, NAV_NO_QUORUM };
enum StationPhase : uint8_t { ST_IDLE=0, ST_APPROACH, ST_FINAL, ST_RAMP, ST_DWELL, ST_DEPART };

// LAYER 5 (CTO3) types and state — hoisted above all functions so the Arduino
// prototype generator sees them; the layer's code lives before setup().
// ---- wire formats ----------------------------------------------------------
// CtoPeerPacket: layout copied field-for-field from r12 (CTO2_VERSION 3,
// frozen). Fields QUORUM cannot honestly fill are zeroed, never guessed:
// speedX10/speedValid stay 0 until IR is aboard (decision 0021 lineage).
static const uint8_t  CTO2_MAGIC   = 0xC4;
static const uint8_t  CTO2_VERSION = 3;
static const uint8_t  CTO3_ECHO_MAGIC   = 0xC5;  // new type; old receivers drop it
static const uint8_t  CTO3_ECHO_VERSION = 1;
typedef struct __attribute__((packed)) {
  uint8_t  magic; uint8_t version;
  uint32_t senderId; uint32_t sequence;
  uint8_t  hallMm; uint8_t frontBoundaryMm; uint8_t rearBoundaryMm;
  int8_t   mapDir;
  uint8_t  autoMode; uint8_t running; uint8_t motionState; uint8_t rampPwm;
  uint8_t  speedValid; uint16_t lastMoveAgeDs; uint16_t speedX10;
  uint8_t  frontOffset; uint8_t rearOffset;
  uint8_t  truthSource;          // 0 none/lost, 1 declared/evaluating, 2 confirmed
  uint8_t  stationPhase;         // OUR StationPhase enum; peers run this firmware
  uint8_t  trafficPhase;         // CtoTrafficPhase below, r12 numbering
  uint8_t  mustHoldEligible;     // always 0 in 1.14 — MHE not ported yet
  uint32_t trafficStopForId;
  uint32_t senderRxAccepted; uint32_t senderTxAttempts; uint32_t senderTxImmediateErrors;
} CtoPeerPacket;
typedef struct __attribute__((packed)) {
  uint8_t magic; uint8_t version;
  uint32_t senderId;
  uint8_t  role;                 // CtoRole — MY conclusion about MY role
  uint32_t partnerId;            // whom I believe I am paired with (0 none)
  uint32_t pairEpochMs;          // my millis() at latch; telemetry only
} Cto3RoleEcho;

enum CtoTrafficPhase : uint8_t { CTRAF_CLEAR=0, CTRAF_DECEL=1, CTRAF_HOLD=3 };
enum CtoRole    : uint8_t { CTO_ROLE_NONE=0, CTO_ROLE_LEADER=1, CTO_ROLE_FOLLOWER=2 };

// ---- tunables (route-common; per-loco extent lives in LL_LocoConfig) ------
static const uint16_t CTO_TX_INTERVAL_MS      = 500;    // r12 cadence
static const uint16_t CTO_ECHO_INTERVAL_MS    = 1000;
static const uint32_t CTO_PEER_STALE_MS       = 3000;   // r12 value
static const uint8_t  CTO_CLEAR_GAP_MARKERS   = 6;      // 0033: the invariant
// Operator 2026-08-14: the follower was stopping too far back — 12 -> 9, three
// markers closer. NOTE (CODEX): this is not a stronger guarantee, it is a
// smaller experimental margin. Zero is already requested at 9, so crossing the
// 6-marker clearance boundary cannot brake any harder; 6 remains the invariant
// of decision 0033 but the field test must MEASURE the minimum bound gap
// actually achieved rather than assume it.
static const uint8_t  CTO_STOP_GAP_MARKERS    = 9;      // bound gap: begin decel-to-stop
static const uint8_t  CTO_SLOW_GAP_MARKERS    = 18;     // bound gap: pre-slow to zone speed
static const uint8_t  CTO_PAIR_RANGE_MARKERS  = 12;     // bound gap: Q1/Q2 latch range
// 0037 (operator ruling 2026-08-16): a pairing whose nose-tail order has
// INVERTED no longer describes the railway and must dissolve. On a loop the
// physical order is not permanent — pause one locomotive, let the other lap,
// and the leader arrives behind the follower. Observed 2026-08-16: Otto held
// LEADER while sitting 12 markers behind Toby and braking for him.
// Not a bare comparison. Near-diametric peers sit at ga ~ gb where the
// nearer side is noise, so inversion must be CLEAR (the new near side shorter
// by a margin) and CONFIRMED (held across consecutive passes) — the same
// discipline the 1.16Ra channel watch earned.
static const uint8_t  CTO_ORDER_MARGIN_MARKERS = 12;   // clear-inversion margin
static const uint8_t  CTO_ORDER_CONFIRM_N      = 3;    // consecutive passes
static uint8_t        ctoOrderInvertedFor       = 0;
static const uint8_t  CTO_TIE_BAND_MARKERS    = 12;     // long-range order hysteresis
// Operator 2026-08-13, flashed to both locomotives that evening: 20 s -> 5 s.
// The 20 s dwell plus the station geometry held the pair 30-49 markers apart,
// so the 18/12/6 ladder never engaged and the bubble was never exercised.
static const uint32_t CTO_FOLLOWER_DWELL_MS   = 5000;   // follower platform dwell
#define CTO_MAX_PEERS 8

// ---- state -----------------------------------------------------------------
struct CtoPeer {
  bool     seen=false;
  uint32_t id=0, seq=0, rxMs=0;
  uint8_t  hallMm=255, frontB=255, rearB=255;
  int8_t   mapDir=MAP_UNSET;
  bool     autoMode=false, running=false;
  uint8_t  motionState=0, rampPwm=0, truthSource=0, stationPhase=0, trafficPhase=0;
  bool     rampFalling=false;    // finding 2: previous-sample comparison
  uint32_t stopForId=0;
  uint8_t  echoRole=CTO_ROLE_NONE; uint32_t echoPartner=0, echoRxMs=0;
};
static CtoPeer  ctoPeers[CTO_MAX_PEERS];
static bool     ctoEnabled=true;          // cmd/cto off|on
static bool     ctoRadioUp=false;
static CtoRole  ctoRole=CTO_ROLE_NONE;    // latched role (PAIRED when != NONE)
static int8_t   ctoPairDir=MAP_UNSET;     // finding 4: direction at latch
static uint32_t ctoPartnerId=0;           // latched partner
static uint32_t ctoPairEpochMs=0;
static uint32_t ctoExpectedId=0;          // 0031 membership: armed on first fresh sighting
static CtoTrafficPhase ctoTraffic=CTRAF_CLEAR;
static uint32_t ctoTrafficForId=0;
static bool     ctoFleetHold=false;
static bool     ctoEchoConflict=false;
static bool     ctoEchoConfirmed=false;   // finding 5: fresh reciprocal echo
static uint32_t ctoTxSeq=0, ctoLastTxMs=0, ctoLastEchoMs=0, ctoLastStateMs=0;
static uint32_t ctoRxAccepted=0, ctoTxAttempts=0, ctoTxErrors=0, ctoRxDropped=0;
// 1.16Ra INSTRUMENTATION (field incident 2026-08-15). ctoTxAttempts counts
// INTENTIONS: esp_now_send() returning ESP_OK means "queued to the driver",
// not "transmitted", and ctoTxErrors only catches that immediate return. On
// 2026-08-15 Otto showed tx climbing at 2 Hz with txe 0 while Toby heard
// almost nothing, and that pair of counters was read — wrongly — as proof
// the transmitter was healthy. It was the radio equivalent of trusting PWM
// as proof of motion (decision 0024), in a project whose whole discipline is
// the opposite. These two are fed by the send callback and report what the
// MAC layer actually did with each frame.
//
// HONEST LIMIT, stated so nobody over-reads them — including me, since the
// whole reason this exists is that I over-read tx/txe once already:
//   * ESP-NOW BROADCAST is unacknowledged, so the MAC reports SUCCESS on
//     transmission. ctoTxFailed is therefore structurally pinned near zero
//     FOREVER, and "txd climbing, txf 0" is NOT evidence of a healthy link.
//     It looks exactly like the "tx climbing, txe 0" that produced the
//     2026-08-15 misdiagnosis, one layer down. Do not repeat that reading.
//   * The only diagnostic weight here is whether txd TRACKS tx. A txd that
//     stalls while tx climbs means the driver stopped dequeuing frames. That
//     is the failure this pair can see, and the only one.
//   * A frame sent on the WRONG channel reports SUCCESS. ch/chg is what
//     catches that, not these counters.
//   * Reception is only ever evidenced by the PEER's rx counter, which is why
//     both locomotives publish theirs.
//   * txd/txf are incremented in WiFi-task context while tx is incremented on
//     the loop thread, and the status snprintf samples them unsynchronised. A
//     skew of one or two either way is NORMAL and is not evidence of loss.
static volatile uint32_t ctoTxDone=0, ctoTxFailed=0;
// The ESP-NOW channel is the STATION channel (peers are added with
// channel 0 = "current"), and the station follows the AP. A locomotive that
// roams or re-associates onto another channel keeps perfect MQTT — that path
// goes through the AP and does not care — while becoming completely
// invisible to its peers, whose ESP-NOW lives on the old channel. That
// asymmetry is exactly the 2026-08-15 signature: healthy on the broker,
// silent on the air. Latched at radio-up, checked every service pass.
static uint8_t  ctoChannel=0;
static uint32_t ctoChannelChanges=0;
static uint8_t  ctoChannelCand=0, ctoChannelConfirm=0;
static uint32_t ctoStatusTruncated=0;   // publishes refused as malformed
static uint32_t ctoLastChanCheckMs=0;
// 2.4 GHz only. WiFi.channel() returns int, and a negative error return or a
// value read mid-reassociation would otherwise truncate into a plausible-
// looking uint8_t, fire the alarm, AND latch — costing two false alarms (one
// out, one back) and a permanently wrong reference. Anything outside 1..14 is
// "unknown", which is the same answer as "not associated".
static inline uint8_t ctoWifiChannel(){
  int c=WiFi.channel();
  return (c>=1 && c<=14) ? (uint8_t)c : 0;
}
static QueueHandle_t ctoRxQueue=nullptr;  // recv callback -> loop thread, like cmdQueue
static const uint8_t CTO_BCAST[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};


// ===========================================================================
// HARDWARE
// ===========================================================================
#define HALL_PIN            33
#define ADC_SAMPLES          8
#define MOTOR_DEAD_ZONE_PWM 20
// v1.7: the ESP32 defaults, which is what r12 relied on implicitly by letting
// Adafruit_INA219::begin() call Wire.begin() with no arguments. Named here
// because this sketch names its pins, and because pin 34 next door is the IR
// sensor and 33 is the Hall — an unnamed bus in that neighbourhood invites the
// exact confusion CLAUDE.md warns about.
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22

static inline void pwmAttachCompat(){ ledcAttach(MOTOR_PWM_PIN,PWM_FREQUENCY,PWM_RESOLUTION); }
static inline void pwmWriteCompat(int v){ ledcWrite(MOTOR_PWM_PIN,constrain(v,0,255)); }

// PWM authority state. Declared here — above the detector — because §3 requires
// detectorSample() to sample both at event open. Written on the loop thread
// (core 1), read on the hall task (core 0): aligned 32-bit access is atomic on
// ESP32 hardware, but that is not a compiler visibility contract — hence
// volatile (§3). Targets normally enter via requestPwm(); the deliberate
// exceptions are servicePwmRamp() (the actuator), setup(), and E-stop.
static volatile int commandedPwm=0, actualPwm=0;

// ---------------------------------------------------------------------------
// DIRECTION — one source of truth.
//
// 2_1 kept motorDirection and navDir as independent variables, and Codex found
// three ways they could disagree: cmd/direction moved the motor without moving
// the navigator, session_direction flipped the pin under power, and the
// movement check raced the ramp. Two variables for one physical fact.
//
// They are not independent. sessionDir declares which MAP direction the motor
// travels when it is FORWARD; navDir is therefore DERIVED, never assigned.
// Reversing the motor reverses the navigator by construction.
// ---------------------------------------------------------------------------
static int    motorDirection = DIRECTION_FORWARD;
static int8_t sessionDir     = MAP_UNSET;
static int8_t navDir         = MAP_UNSET;   // derived; see applyDirection()

static inline int8_t oppositeDir(int8_t d){
  return d==MAP_CW?MAP_CCW:(d==MAP_CCW?MAP_CW:MAP_UNSET);
}

// ===========================================================================
// MAP  — 171 markers, verified unique at W=10, locked at W=12
// ===========================================================================
#define DNA_N 171

const uint8_t NGR_DNA1[DNA_N] PROGMEM = {
  1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,
  0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
  0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,
  0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
  1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,
  1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
  1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,
  0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
  1,0,1,0,0,0,0,1,1,1,0
};

// ---------------------------------------------------------------------------
// AMPLITUDE MAP — expected field strength per marker, as a PERCENTAGE of the
// railway's median marker. Measured 2026-08-20 from 6,613 Otto reads and
// 5,363 Toby reads; each entry is the mean of the two locomotives' normalised
// medians, so it carries neither machine's bias.
//
// This is a MAP, not a calibration of one locomotive. The evidence: Otto and
// Toby have different sensors, different mounts and a sensitivity difference
// of about a quarter, yet their normalised per-marker profiles correlate at
// r = 0.938 across all 171 markers. mm 140 reads 0.61x median on BOTH. The
// shape belongs to the magnets; only the gain belongs to the locomotive.
//
// Range 61..153. The weak cluster is mm 137-144 (mm 140 at 61), with mm 27-28
// and mm 71 also low; the strong markers reach 1.5x at mm 61-64 and mm 100.
//
// USE: the quarantine's "dim" test compared a read against the trailing median
// of ALL accepted peaks, which assumes every magnet is the same strength. They
// are not, by 2.5:1. A genuine read at mm 140 was within a whisker of being
// called a phantom, while a phantom-strength read at mm 100 looked ordinary.
// Scaling the expectation by this table asks the right question: is this read
// weak FOR THIS MAGNET.
//
// DRIFT: temperature, alignment and sensor changes move the GAIN, not the
// shape — which is exactly why the two profiles agree. The trailing median
// supplies the gain; this table supplies the shape.
//
// REVISED 2026-08-21 from POST-TAMPING data only (Otto 2539 + Toby 1506 reads,
// full 171-marker coverage from both). The tamped cluster measured 0.7-0.85x
// median - improved from 0.61x, NOT restored to par. mm 152 (double magnet)
// still reads 1.23x. Range 69..159.
// REVISION: rebuild from a session's mm/marker stream when magnets are moved,
// replaced or reseated. It is evidence, and it goes stale like any other.
// ---------------------------------------------------------------------------
static const uint8_t strengthPct[DNA_N] PROGMEM = {
  111,152,135,134,133,116,132, 97,120,102,105, 99, 86,107, 98,102, 97,108,110,111,
  107,103,109, 98,106, 95, 99, 98, 90, 97,103, 95, 94,100,103, 97, 99,105,103,101,
  101,102, 92, 98, 94,102,107,102,101,105, 98, 97,101, 96,136,102,101,103,105,110,
  102,152,108,159,152, 92, 96, 83, 92, 88, 88, 77, 88, 90, 92, 94, 88, 87,100,117,
  103,112,105,106,114,108,101,110,114,114,101, 94,110,109,101,107,107,106,104,150,
  151,142,139, 88, 86, 97, 91, 87, 91, 87, 92,103,103,101, 98,104, 97, 96,103,108,
  102,119,102,103,106,103,106,106, 98,106, 98, 95, 92, 90, 98,104, 97, 84, 86, 82,
   69, 76, 76, 93, 84, 98, 92, 98, 87, 89, 98, 92,123, 98, 92, 97,102, 96,102, 93,
   95, 93, 88, 98, 94,105, 99, 95,108, 92,116
};
static inline uint8_t strengthAt(uint8_t mm){ return pgm_read_byte(&strengthPct[mm%DNA_N]); }

static const uint16_t spacingMm[DNA_N] PROGMEM = {
  330,340,330,315,325,330,315,300,300,295,
  300,290,300,315,315,325,310,300,300,320,
  315,315,305,300,295,300,300,300,300,315,
  330,320,315,310,300,300,300,300,300,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,300,300,300,295,320,300,
  315,320,315,325,315,305,300,305,300,300,
  295,295,300,300,300,300,300,300,300,305,
  300,300,305,300,300,330,300,300,305,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,295,300,300,300,300,300,305,300,
  300,320,320,300,300,300,300,300,300,300,
  300,300,300,300,300,300,280,300,300,290,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,305,300,305,300,295,300,
  300,300,305,300,300,305,320,290,320,300,
  300,305,330,330,320,325,315,355,330,330,
  330
};

// Per-station speeds. One profile cannot serve a level approach and an uphill
// one: Grillers climbs, and on the 2026-07-27 lap it nearly stalled at the
// common approach speed. Its numbers are raised accordingly.
//
// zonePwm  is held for five markers before the centre, so the locomotive is
//          already settled at approach speed when it reaches the station --
//          not still decelerating through it.
// finalPwm is the speed the stop is made FROM. The zero ramp begins here, so
//          it should be the slowest speed the locomotive holds reliably on
//          that stretch.
// stopOffset is where passengers actually end up, set from observed stops on
// the 2026-07-27 run rather than from geometry. Grillers and Bamboo both
// overran the platform at +2 and were pulled back a marker.
// v1.12C (operator, 2026-08-09, both orientations): every stop one marker
// earlier — the passenger-gentle 200 ms/step brake (v1.12B) needs more
// road, so the landings drifted a marker past the platforms. stopOffset
// reduced by one across the board; Grillers and Bamboo (already pulled
// back once for the same reason at the old ramp) now stop from the
// centre marker itself.
//                          centre  zone  final  stop
static const StationDefinition STATIONS[] = {
  {"Patio",    15,            60,   45,     1},   // was 2 (obs. good at old ramp)
  {"Grillers", 63,            72,   58,     0},   // was 1
  {"Arches",  107,            60,   45,     1},   // was 2 (obs. good at old ramp)
  {"Bamboo",  157,            60,   45,     0}    // was 1
};
static const uint8_t STATION_COUNT = sizeof(STATIONS)/sizeof(STATIONS[0]);

// Operator 2026-08-14: CCW landings sat consistently one marker past the
// platform. stopOffset is direction-agnostic in the table above, so the
// correction is applied per direction here rather than by splitting every row.
// CW is unchanged. A negative result is deliberate and legitimate: it starts
// the zero ramp one marker BEFORE centre, which is the only way to gain a
// marker at the two stations already sitting at stopOffset 0.
static inline int8_t effStopOffset(uint8_t idx){
  int8_t s = STATIONS[idx].stopOffset;
  if(navDir==MAP_CCW) s -= 1;
  return s;
}
// Where ST_APPROACH hands over to ST_FINAL: the centre marker normally, or
// earlier when the effective stop offset is negative.
static inline int8_t finalEntryOffset(uint8_t idx){
  const int8_t s = effStopOffset(idx);
  return (s < 0) ? s : 0;
}

// CTO3 Station Stop v1 (docs/CTO3/station-stop-v1): a profile may restrict
// arming to one named station with MISSION_ONLY_STATION. This is the
// smallest mission-layer mechanism around the station machine — destination
// selection, not phase logic. Keyed on the station NAME so a table reorder
// cannot silently change the destination. Without the define every station
// arms, exactly as before (Toby's build).
#ifdef MISSION_ONLY_STATION
static inline bool stationEnabled(uint8_t i){ return strcmp(STATIONS[i].name, MISSION_ONLY_STATION)==0; }
#else
static inline bool stationEnabled(uint8_t i){ (void)i; return true; }
#endif

static inline uint8_t routeMod(int32_t v){ v%=DNA_N; if(v<0) v+=DNA_N; return (uint8_t)v; }
static inline uint8_t dnaAt(uint8_t mm){ return pgm_read_byte(&NGR_DNA1[mm%DNA_N]); }
static inline char    polChar(uint8_t p){ return p?'N':'S'; }
static inline uint8_t nextMm(uint8_t mm,int8_t dir){ return routeMod((int32_t)mm+dir); }
static const char* dirName(int8_t d){ return d==MAP_CW?"CW":(d==MAP_CCW?"CCW":"UNSET"); }

static const char* landmarkAt(uint8_t mm){
  switch(mm%DNA_N){
    case 0:return "Southpoint"; case 15:return "Patio";      case 63:return "Grillers";
    case 72:return "Westpoint"; case 98:return "Northpoint"; case 107:return "Arches";
    case 140:return "Eastpoint";case 157:return "Bamboo";    default:return "";
  }
}

// Signed marker offset from mm to a station centre, along the travel direction.
// Negative = station is ahead.
static int16_t offsetToCentre(uint8_t mm,int8_t dir,uint8_t centre){
  int32_t d = (dir==MAP_CW) ? routeMod((int32_t)mm-centre) : routeMod((int32_t)centre-mm);
  return (d > DNA_N/2) ? (int16_t)(d-DNA_N) : (int16_t)d;
}

// ===========================================================================
// LAYER 1 — SENSOR
// ---------------------------------------------------------------------------
// Median of the last 64 seconds. The operating assumption is that TOTAL magnet
// occupancy across the whole window stays under half -- not merely that each
// individual magnet does, since several crossings in one window add up.
// At observed spacing and speed that holds with wide margin. Drift moves every sample, so
// it follows immediately. No gating, no freeze, no recovery state, no way to
// wedge. Proven over a full session on 2026-07-26: delta within a few counts,
// TRACKING throughout, including while sitting 90 counts inside a magnet.
// ===========================================================================
#define MEDIAN_WINDOW     128
#define MEDIAN_SAMPLE_MS  500UL
#define CALIBRATION_MS   2000UL

static volatile int baselineCounts = 0;   // written by task, read by loop
static int      northEnter=0, northExit=0, southEnter=0, southExit=0;
static int16_t  medRing[MEDIAN_WINDOW];
static uint8_t  medIndex   = 0;
static bool     medPrimed  = false;
static unsigned long medLastPushMs = 0;

static void recomputeThresholds(){
  northEnter = baselineCounts + HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS;
  northExit  = baselineCounts + HALL_DEADBAND_COUNTS;
  southEnter = baselineCounts - HALL_DEADBAND_COUNTS - HALL_ENTRY_MARGIN_COUNTS;
  southExit  = baselineCounts - HALL_DEADBAND_COUNTS;
}

static int readAveragedADC(){
  uint32_t s=0; for(uint8_t i=0;i<ADC_SAMPLES;i++) s+=analogRead(HALL_PIN);
  return (int)(s/ADC_SAMPLES);
}

static int medianOfRing(){
  int16_t t[MEDIAN_WINDOW];
  memcpy(t,medRing,sizeof(t));
  for(int i=1;i<MEDIAN_WINDOW;i++){                 // 128 elements every 500 ms
    int16_t k=t[i]; int j=i-1;                      // is nothing on a 240 MHz core
    while(j>=0 && t[j]>k){ t[j+1]=t[j]; j--; }
    t[j+1]=k;
  }
  return (int)t[MEDIAN_WINDOW/2];
}

static void primeMedian(int seed){
  for(int i=0;i<MEDIAN_WINDOW;i++) medRing[i]=(int16_t)seed;
  medIndex=0; medPrimed=true; medLastPushMs=millis();
}

static void updateBaseline(int raw,unsigned long now){
  // PRIMING INVARIANT (1.8, CODEX): this fallback is unreachable in a
  // correct boot -- calibrate() primes the ring before hallTask exists, so
  // medPrimed is true before the first sample arrives. It is RETAINED as
  // last-resort defense: if a future edit ever breaks that ordering, a
  // single-sample prime still beats a zero baseline, which would pin the
  // thresholds at +/-38 around 0 and hold an event open forever. Do not
  // move the motion gate above this line: a prime taken from the first
  // moving sample could land mid-magnet.
  if(!medPrimed) primeMedian(raw);
  // 0017: adapt only with positive evidence of tractive motion. At or
  // below the dead zone the locomotive may still be coasting or pushed --
  // this is not proof of rest -- but the error cases are asymmetric:
  // refusing a push while secretly moving postpones adaptation ~30 s;
  // accepting pushes while secretly parked over a magnet migrates the
  // reference onto it in ~32-64 s and systematically corrupts the
  // post-departure event stream.
  // The median's robustness assumes magnets are OUTLIERS, which is a
  // property of traversal, not of the track. actualPwm is volatile and
  // already read on this task (s3 PWM-at-detect); no new threading.
  if(actualPwm <= MOTOR_DEAD_ZONE_PWM) return;
  if(now-medLastPushMs < MEDIAN_SAMPLE_MS) return;
  medLastPushMs=now;
  medRing[medIndex]=(int16_t)raw;
  medIndex=(uint8_t)((medIndex+1)%MEDIAN_WINDOW);
  int m=medianOfRing();
  if(m!=baselineCounts){ baselineCounts=m; recomputeThresholds(); }
}

// ===========================================================================
// LAYER 2 — DETECTOR
// ---------------------------------------------------------------------------
// Contract: a threshold crossing produces an event. Polarity is decided by the
// OPENING pole, so a merged N/S pair cannot be arbitrated into the wrong
// answer. Peak and duration ride along as quality. Baseline drift during the
// event rides along so the navigator can discount a reading taken against a
// moving reference.
//
// The only thing the detector refuses is an event shorter than the debounce
// floor, because that is electrically not a magnet. Everything else it reports
// and lets the navigator judge. Weak reads are TAGGED, not dropped — a weak
// reading that matches prediction is still good evidence.
// ===========================================================================
#define EVENT_EXIT_HOLD_MS   20UL
#define EVENT_FLOOR_MS       40UL
#define HALL_TASK_TICK_MS     1

// ---------------------------------------------------------------------------
// TEMPLATES 0.2 — CONSERVATIVE ADMISSION. Test sketch, not production QUORUM.
//
// Scope: the ADMISSION gate only. Everything downstream is 1.16R's, unchanged:
// navigation, the §3Q quarantine, timing gate, quorum recovery, NO_QUORUM
// self-resolution, stations, CTO3, IR Test A, E-stop, manual authority, MQTT.
// Proposition under test: QUORUM recovers well from a MISSING magnet and badly
// from a FALSE one, so a cleaner input diet may make it dependable without
// redesigning anything downstream. Missing a meal is acceptable. Junk is not.
//
// Evidence: 7,238 raw excursions measured offline from phys_raw/phys_baseline
// across Toby CCW and Otto CCW/CW pre-rewiring and post-rewiring captures.
//   * DURATION CANNOT SEPARATE. Impostors ran to 1,151 ms; genuine passages
//     start at 7 ms. The ranges fully contain each other. EVENT_FLOOR_MS is
//     therefore the weakest available axis, and it is the whole gate today.
//   * AMPLITUDE IS SPEED-STABLE and is the primary axis here. Median peak by
//     PWM at open: 188 (20-49), 195 (50-69), 190 (70-99), 171 (100+).
//   * FLUX IS A TRAP. peak x duration is the MOST speed-dependent measure
//     available; a 20,000 floor discarded 63% of Toby's real markers.
//
// HONEST LIMITS (CODEX review, 2026-08-26 — these are not settled facts):
//   * ADMIT_MIN_PEAK is NOT a positive identification of a magnet. It is a
//     conservative CANDIDATE THRESHOLD sited in a density valley. That valley
//     still contains 146 of the 7,238 excursions, so no naturally definitive
//     boundary exists and none is claimed.
//   * The duration rule below is physically motivated but UNFITTED. The
//     measurements do not establish the exact 80 x 90 / pwm relationship or
//     its clamps. Treat it as a provisional calibration rule.
//   * 97.6% recall on Toby says nothing about PRECISION. No capture has an
//     independent marker count for Otto, so false admission is UNMEASURED.
//   * Downstream residual risk, explicitly retained: clean admission does not
//     address same-polarity omission masking, which offline replay confirmed
//     is real. Removing the dominant failure source need not remove that one.
//   * Captures top out at PWM 120. The >120 regime is unvalidated.
//
// A rejection here is permanent: the event is never queued, so it can never
// advance position, enter the evidence ring, be quarantined, or score a
// recovery hypothesis. It has NO NAVIGATION AFTERLIFE. It does keep an AUDIT
// afterlife — see AdmitReject below; permanent rejection from navigation must
// not mean deletion from the diagnostic record.
// ---------------------------------------------------------------------------
// TEMPLATES 0.3 ADDENDUM — R3 TARGET ACQUISITION (spec: docs/
// TEMPLATES_REVISION_3_TARGET_ACQUISITION.md; decision 0044; build
// authorized 2026-08-27). The universal 140-count floor above is RETIRED as
// the admission decision. This gate is now the MAGNET TEST only: a
// permissive spike/noise filter whose rejects (SIGNAL_REJECTED) keep the
// audit afterlife below, unchanged. The admission decision proper — the
// IDENTITY TEST against the specific marker the locomotive is currently
// seeking — moved to the loop thread, where navMm/navDir/gain/history
// natively live. See the R3 section above drainMarkers().
// ---------------------------------------------------------------------------
#define TEMPLATES_ADMISSION

#ifdef TEMPLATES_ADMISSION
  // 0.3: MAGNET TEST floor, not an admission decision. 60 sits at the top
  // of the noise bulk (4,041 of 7,238 offline excursions at 30-59 counts);
  // the 60-139 valley the old 140 floor discarded wholesale is now
  // adjudicated per-marker by the R3 identity test on the loop thread.
  // Both 2026-08-27 genuine-noise field events sat below 60 AND at duration
  // ratios 0.09-0.14 of expectation — either axis alone refuses them.
  #define R3_MAGNET_MIN_PEAK     60    // counts from baseline; magnet test
  #define ADMIT_DUR_REF_MS       80UL  // provisional floor at the reference PWM
  #define ADMIT_DUR_REF_PWM      90UL  // where ADMIT_DUR_REF_MS was measured
  #define ADMIT_DUR_FLOOR_MIN_MS 30UL  // never below electrical debounce
  #define ADMIT_DUR_FLOOR_MAX_MS 250UL // never over-reject at crawl speed

  // Genuine width scales ~1/speed, so the floor tracks it. PROVISIONAL: the
  // FORM is motivated (width fell ~3x from PWM 40 to PWM 90 while peak did
  // not); the CONSTANT is not fitted. Integer math — this runs on hallTask.
  static inline unsigned long admitDurFloorMs(uint8_t pwmAtOpen){
    if(pwmAtOpen == 0) return ADMIT_DUR_FLOOR_MAX_MS;
    unsigned long f = (ADMIT_DUR_REF_MS * ADMIT_DUR_REF_PWM) / (unsigned long)pwmAtOpen;
    if(f < ADMIT_DUR_FLOOR_MIN_MS) f = ADMIT_DUR_FLOOR_MIN_MS;
    if(f > ADMIT_DUR_FLOOR_MAX_MS) f = ADMIT_DUR_FLOOR_MAX_MS;
    return f;
  }

  // AUDIT AFTERLIFE. Every rejected excursion is recorded with the evidence
  // that condemned it, so the experiment can be judged offline. Written on
  // hallTask, drained and published on the loop thread beside the marker
  // queue. Bounded and drop-oldest-counted: a pulse storm may cost audit
  // records, never a missed control deadline. Nothing on this path can
  // re-enter navigation — it is write-only with respect to nav state.
  #define ADMIT_REJECT_CAUSE_PEAK 1
  #define ADMIT_REJECT_CAUSE_DUR  2
  struct AdmitReject {
    uint32_t tMs;
    uint16_t durMs;
    uint16_t floorMs;    // the floor this event was actually judged against
    int16_t  peak;
    uint8_t  pwmActual;
    uint8_t  pole;
    uint8_t  cause;
  };
  static QueueHandle_t rejectQueue = nullptr;
  static volatile unsigned long peakRejects=0, rejectQueueDrops=0;
#endif

static QueueHandle_t eventQueue = nullptr;
static volatile unsigned long queueDrops=0, floorRejects=0;
static volatile unsigned long taskMaxGapMs=0, taskLastRunMs=0;
static volatile int lastRaw=0;

static bool     evActive=false;
static uint8_t  evOpenPole=0;
static int      evPeakN=0, evPeakS=0, evStartBaseline=0;
// §3: PWM pair captured at event OPEN, beside evStartBaseline — dt is measured
// opening-to-opening, so the aligned PWM sample is the one at the opening.
static uint8_t  evStartPwmActual=0, evStartPwmCommanded=0;
static unsigned long evStartMs=0, evReturnMs=0;

static void detectorSample(){
  int raw=readAveragedADC();
  unsigned long now=millis();
  lastRaw=raw;

  updateBaseline(raw,now);          // every sample, in or out of an event

  int n=max(0,raw-baselineCounts), s=max(0,baselineCounts-raw);

  if(!evActive){
    if(raw>=northEnter || raw<=southEnter){
      evActive=true; evStartMs=now; evReturnMs=0;
      evStartBaseline=baselineCounts;
      // §3: read the pair adjacently, in the same statement pair, so the two
      // cannot straddle a target change. Neither is used for control; a
      // one-tick skew is immaterial.
      evStartPwmActual    = (uint8_t)actualPwm;
      evStartPwmCommanded = (uint8_t)commandedPwm;
      evPeakN=n; evPeakS=s;
      evOpenPole=(raw>=northEnter)?1:0;
    }
    return;
  }

  evPeakN=max(evPeakN,n); evPeakS=max(evPeakS,s);

  if(raw<=northExit && raw>=southExit){
    if(evReturnMs==0) evReturnMs=now;
    if(now-evReturnMs>=EVENT_EXIT_HOLD_MS){
      unsigned long dur=now-evStartMs;
      evActive=false;
      int evPeak = evOpenPole?evPeakN:evPeakS;
#ifdef TEMPLATES_ADMISSION
      // 0.3 MAGNET TEST: a low amplitude floor (the noise bulk) plus the
      // speed-scaled duration floor that catches high-amplitude SPIKES.
      // Failing either is SIGNAL_REJECTED — both paths record an audit
      // entry and return: the event is never queued, so it cannot reach
      // navigation, quarantine or recovery by any route. Events that PASS
      // face the R3 identity test on the loop thread before the navigator.
      {
        unsigned long floorMs = admitDurFloorMs(evStartPwmActual);
        uint8_t cause = 0;
        if(evPeak < R3_MAGNET_MIN_PEAK) cause = ADMIT_REJECT_CAUSE_PEAK;
        else if(dur < floorMs)        cause = ADMIT_REJECT_CAUSE_DUR;
        if(cause){
          if(cause==ADMIT_REJECT_CAUSE_PEAK) peakRejects++; else floorRejects++;
          AdmitReject r;
          r.tMs       = (uint32_t)evStartMs;
          r.durMs     = (uint16_t)min(dur,(unsigned long)65535);
          r.floorMs   = (uint16_t)min(floorMs,(unsigned long)65535);
          r.peak      = (int16_t)evPeak;
          r.pwmActual = evStartPwmActual;
          r.pole      = evOpenPole;
          r.cause     = cause;
          if(rejectQueue && xQueueSend(rejectQueue,&r,0)!=pdTRUE) rejectQueueDrops++;
          return;
        }
      }
#else
      if(dur<EVENT_FLOOR_MS){ floorRejects++; return; }
#endif
      MarkerEvent e;
      e.polarity      = evOpenPole;
      e.peak          = evPeak;
      e.durationMs    = (uint16_t)min(dur,(unsigned long)65535);
      e.baselineDrift = (int16_t)(baselineCounts-evStartBaseline);
      e.detectedAtMs  = evStartMs;
      e.pwmActualAtDetect    = evStartPwmActual;     // §3: from event open
      e.pwmCommandedAtDetect = evStartPwmCommanded;
      if(eventQueue && xQueueSend(eventQueue,&e,0)!=pdTRUE) queueDrops++;
    }
  } else {
    evReturnMs=0;
  }
}

static void hallTask(void*){
  unsigned long prev=millis();
  for(;;){
    unsigned long now=millis();
    unsigned long gap=now-prev;
    if(gap>taskMaxGapMs) taskMaxGapMs=gap;
    prev=now; taskLastRunMs=now;
    detectorSample();
    vTaskDelay(HALL_TASK_TICK_MS);
  }
}

// ===========================================================================
// CROSS-LAYER STATE
// ---------------------------------------------------------------------------
// The navigator needs three things from the operations layer: whether AUTO is
// driving, whether a station currently owns the throttle, and the two speeds it
// may request. Declared here, above their use, rather than forward-declared
// piecemeal further down.
//
// This is the only coupling between layers 3 and 4, and it runs one way: the
// navigator may ask operations for a speed; operations never reaches back into
// navigation state.
// ===========================================================================
static const uint8_t  CRUISE_PWM       = 90;    // v1.12 operator tuning (was 100); approach ramps derive from this automatically

// ---------------------------------------------------------------------------
// CIRCUIT EXPRESS (CE) — the mission layer. Operator specification 2026-08-19.
//
// CE is a MISSION, never a spacing decision (BUBBLE_V1_SPEC §9 — bicameral, so
// the dispatcher may assign it). The leader becomes the EXPRESS, the follower
// the LOCAL, and the pairing is severed: they stop being a bubble and become
// two trains running different services on shared track.
//
// NOTHING HERE WATCHES FOR THE EXPRESS CATCHING THE LOCAL, and nothing should.
// Layer 5 already decelerates and holds behind a slower train, and Q1/Q2
// already re-derives roles from geometry. The operator's own note is the
// authority (docs/CTO3/resources/PHYSICAL_ENVELOPE_NOTE.txt:13): "circuit
// express becomes much simpler. It just becomes a command to temporarily
// suspend the station stops for a locomotive that happens to be in the lead."
// Every attempt to make CE clever about the chase is a re-invention of the
// traffic layer, and CTO2 died of exactly that.
//
// CE ENDS the moment traffic first slows or holds the express — that IS the
// express having caught the local. Missions clear, ordinary service resumes,
// and the pair re-forms by geometry with the roles swapped, because the local
// is now ahead. Operator specification 2026-08-19, matching original CTO2.
//
// Speeds are PWM, not speed: decision 0014's SPEED_HOLD is still unbuilt, so
// this is throttle-as-proxy and inherits every limitation 0014 names.
// ---------------------------------------------------------------------------
// Feature marker so tests/harness.cpp can offer CE commands only when the
// sketch under compilation actually has CE. verify_inert.py builds the CURRENT
// harness against an OLDER QUORUM.ino to prove behavioural inertness, and
// without this guard every such comparison across the CE boundary fails to
// build rather than reporting a result.
#define CE_MISSION_PRESENT 1
enum CeMission : uint8_t { CE_NONE=0, CE_EXPRESS, CE_LOCAL };
static CeMission      ceMission          = CE_NONE;
static const uint8_t  CE_EXPRESS_PWM     = 110;   // operator 2026-08-19
static const uint8_t  CE_LOCAL_PWM       = 75;    // operator 2026-08-19
static const uint8_t  CE_SKIP_EVERY      = 3;     // express skips every third station
static const uint32_t CE_EXPRESS_DWELL_MS= 5000UL;// when the express does stop
static uint16_t ceStationSeq = 0;    // stations met since this CE began
static int8_t   ceSkipLatch  = -1;   // station whose skip decision is already made
static bool     ceSkipNow    = false;// ...and what that decision was
// CE is a FLEET routine, and its lifecycle is shared. Both locomotives run the
// identical rule below and reach the same conclusion from their own geometry,
// so no CE field is needed on the frozen wire packet.
//
// ceSeparated arms the ending. At the instant CE is assigned the two trains are
// still inside pairing range — they were a bubble one tick ago — so "a peer is
// close" is true immediately and would end CE before the express ever pulled
// away. The mission therefore cannot end until the fleet has first been seen
// APART, which is the express actually running its service. Review found the
// first version ending CE ~100 ms after it began.
static bool     ceSeparated  = false;
// ceClosing: the fleet has come back within slow range after separating, so
// the formation inhibit lifts and Q1/Q2 may latch again. CE does NOT end here.
//
// Ending on the gap number was a P1 (review, 2026-08-19): CE ended at
// CTO_SLOW_GAP_MARKERS (18) while pairing forms at CTO_PAIR_RANGE_MARKERS (12),
// so gaps of 13..18 cleared the mission WITHOUT producing a pairing. That state
// is stable, not transient — both locomotives return to cruise 90 the moment
// the missions clear, so the gap stops closing and the fleet sits unpaired
// indefinitely, in neither CE nor paired service.
//
// CE therefore ends on the EVENT the operator actually specified — "they pair
// in the new arrangement, and CTO paired service resumes" — which is a latched
// role, not a distance.
static bool     ceClosing    = false;
static const char* ceMissionName(){
  return ceMission==CE_EXPRESS ? "EXPRESS" : ceMission==CE_LOCAL ? "LOCAL" : "NONE";
}
// Open-main cruise for the active mission. Grade sections keep their OWN cruise
// (see cruiseForPosition): those values were tuned to pull specific hills, and a
// mission is a service pattern, not a licence to re-tune the railway's physics.
static inline int ceCruisePwm(){
  if(ceMission==CE_EXPRESS) return (int)CE_EXPRESS_PWM;
  if(ceMission==CE_LOCAL)   return (int)CE_LOCAL_PWM;
  return (int)CRUISE_PWM;
}
static const uint8_t  STATION_ZONE_PWM = 60;
static const uint16_t NORMAL_STEP_MS   = 150;

static bool         autoEnrolled=false, autoRunning=false;
// `estopped` is written from BOTH threads: the loop-thread command handler and,
// as of v2.20, directly from the network task's MQTT callback so that engaging
// E-stop never waits on the command queue (see onMqttEnqueue). Single volatile
// bool, one writer path at a time, read every loop() pass by servicePwmRamp --
// safe on ESP32 without a lock.
static volatile bool estopped=false;
static StationPhase stPhase=ST_IDLE;

static void requestPwm(int target,uint16_t stepMs);
static int  cruiseForPosition();   // section cruise speed; defined in LAYER 4
static void stationReset(const char* note);   // §2.5 step 2a; defined in LAYER 4
// LAYER 5 (CTO3) hooks, defined after MQTT. All are inert until a peer has
// been seen; with none they cost one branch each and change no behaviour.
static int      ctoLimitPwm(int want);     // speed ceiling from traffic protection
static const char* ctoRefusalReason();     // why BEGIN would not move (operator 2026-08-14)
static uint32_t ctoDwellMs();              // confirmed follower dwell, else DWELL_MS
static void     ctoService();              // 10 Hz: registry, roles, gates; loop thread
static void     ctoRadioInit();            // esp_now up, once, after WiFi connects
static void     ctoHandleClear(const char* msg); // cmd/cto — operator clear/off
static void     ctoDissolve(const char* why);    // end a pairing, say why
// CE (Circuit Express) mission hooks. Declared here because the command
// handler calls ceBegin() ~1000 lines before it is defined: the Arduino IDE
// auto-generates prototypes and hides that, but tests/harness.cpp compiles the
// sketch as plain C++ and does not. Same trap the TYPES block at the top warns
// about — it cost a green IDE build and a red suite before this line existed.
static void     ceBegin();                       // dispatcher CE: assign missions, sever
static void     ceEnd(const char* why);          // missions clear, ordinary service resumes
// QUORUM decision events (adoption, incident open/close, phantom, fixture)
// ride pubMarker() per §5.1; defined after the transport, called from Layer 3.
// `extra` is a pre-formatted JSON fragment beginning with a comma, or "".
static void publishQuorumDecision(const char* ev, const char* extra);

// ===========================================================================
// LAYER 3 — NAVIGATOR  (QUORUM — docs/QUORUM_v3_0_implementation_spec.md R20)
// ---------------------------------------------------------------------------
// The locomotive knows where it is. A magnet that disagrees is assumed to be
// a bad read, not a lost position — so the locomotive holds its position,
// ignores the reading, and watches whether the NEXT magnets fit the pattern
// it already expected. Only when several in a row fail does it ask the second
// question: if not here, then where? And it asks against a short list,
// because it was right a minute ago.
//
//   navMm is the marker the locomotive believes it has just reached. Once
//   position and direction are declared it advances by exactly one on every
//   NAVIGATION-ACCEPTED event — in NAV_NORMAL, NAV_EVALUATING and
//   NAV_NO_QUORUM alike, including events whose polarity disagrees. "Hold
//   position on a disagreement" means DO NOT RELOCATE; it never means do not
//   advance. After a disagreement at navMm=154 the next event is compared
//   against dnaAt(155), not dnaAt(154).
//
//   An offset is a displacement in EVENT STEPS ALONG THE DIRECTION OF
//   TRAVEL, not an arithmetic marker number. It is applied ONLY as
//   routeMod((int32_t)navMm + navDir * offset) — never as navMm += offset:
//   under CCW navDir is negative and bare addition gives the wrong marker,
//   and near MM000 the wrap matters. Because navMm advances on every
//   accepted event throughout EVALUATING, the offset is constant while
//   evaluating: adoption is one correction applied once.
// ===========================================================================
#define QUORUM_TRIGGER    3   // consecutive misses that wake evaluation
#define QUORUM_MAX       12   // accepted events scored without adoption -> terminal
#define QUORUM_MARGIN     2   // unique lead required to adopt
#define QUORUM_CANDIDATES 6
// Asymmetric by measurement (§2.2): a phantom inserts one spurious event, so
// the odometer runs at most one AHEAD; dropped events arrive in bursts (run 1
// logged queue_drops 0->4 and the recovery returned off=+4).
#ifdef Q_OFFSETS_SYMMETRIC
// HARNESS-ONLY variant: the asymmetry rationale above ("the odometer runs at
// most one AHEAD") was falsified 2026-08-21 — dwell double-counts and
// quarantine skips ran the forward label 2+ ahead, a true offset of -2 the
// fence cannot express (docs/QUORUM_LABEL_SLIP_ROOT_CAUSE.md). Gated on the
// shadow replay converting the 11:50:43 cascade into a -2 adoption.
#undef  QUORUM_CANDIDATES
#define QUORUM_CANDIDATES 8
static const int8_t QUORUM_OFFSETS[QUORUM_CANDIDATES] = { -3, -2, -1, 0, +1, +2, +3, +4 };
#else
static const int8_t QUORUM_OFFSETS[QUORUM_CANDIDATES] = { -1, 0, +1, +2, +3, +4 };
#endif

// HARD_BOUND advisory (decision 0023). Parameters of a live, diagnostic-only
// feature, so they live with the other navigator constants; the matcher itself
// is still down in the v2.x block with the notes on why it is not an authority.
// DNA_W 12 >= 10 is what makes the match unambiguous: every window of length
// 10 or more is unique across the 171-marker route, W=9 still collides four
// ways. REACQ_WINDOW_MARKERS bounds the search to the locomotive's own
// neighbourhood so the advisory can never name a remote position.
#define DNA_W                12
#define REACQ_WINDOW_MARKERS  5
#define ADVISORY_NONE       255   // no exact unique window; 0..170 are markers

// §3 timing gate
#define GATE_LOW_PWM_FLOOR 40   // below this the velocity model is invalid
#define GATE_RAMP_DELTA    10   // |actual-commanded| beyond this = mid-ramp
static const float DT_CONSERVE_TOL = 0.30f;   // provisional, named (§3)
#ifdef Q_TIMING_MEASURED
// Longest interval allowed to become the timing predecessor. The slowest
// genuine ACTIVE-gate cadence observed is ~6 s (pwm 46 crawl with dwell
// returns); beyond 8 s the interval spans a stop and predicts nothing.
static const uint16_t Q_PREV_SANE_MS = 8000;
// Rhythm escape: a phantom splits ONE interval ONCE, so two consecutive
// events at the SAME new cadence are a train, not noise. Remembering the
// rejected interval and accepting its rhythm BOUNDS rejection runs sharply in
// practice (observed 16 -> 1-3 on the 2026-08-21 replays) but does NOT cap
// them mathematically (CODEX finding 3): a cadence drifting more than 30%
// between consecutive events - e.g. 1000 -> 1500 -> 2200 ms under hard
// acceleration - defeats the escape while each interval still fails the
// conservation test against a long predecessor. Third revision from the
// shadow replays: the T3 sanity cap alone still allowed a 17-run after crawl.
static uint16_t qLastRejectedDt = 0;
#endif
// PWM -> velocity, mm/s. Explicitly provisional: PWM is a request, not a
// result (ROAD_TO_CTO.md) — grade, load, battery and railhead all move what
// it produces. M2's wheel sensor replaces this; the wide tolerance above
// exists to absorb the model error until then.
static const float VEL_MODEL_SLOPE     = 3.90f;
static const float VEL_MODEL_INTERCEPT = -99.2f;

#define STATUS_BROADCAST_MS  1000UL

// A loco that is misreading markers may also be MISSING them, so it can be
// AHEAD of its own dead reckoning. Forward margin on the published bound.
#define LOST_FRONT_MARGIN_MARKERS 5

static NavState navState      = NAV_UNSET;
static uint8_t  navMm         = 0;
static uint32_t navMarkers=0, navAgree=0, navDisagree=0, navLostCount=0;

// ---------------------------------------------------------------------------
// Recovery-incident state (§2.6). An incident begins when NAV_NORMAL first
// enters NAV_EVALUATING and ends only at a confirming agreement, a
// declaration, a direction change, or NAV_NO_QUORUM entry.
// ---------------------------------------------------------------------------
constexpr int8_t NO_ADOPTED_OFFSET = INT8_MIN;   // zero is a VALID offset (§2.6)
static uint8_t  missStreak=0;
static bool     adoptionPendingValidation=false; // adoption not yet confirmed
static uint8_t  adoptionDisagreeStreak=0;        // disagreements while pending
static uint8_t  adoptionFailureCount=0;          // failed adoptions THIS incident
static int8_t   adoptedOffset=NO_ADOPTED_OFFSET;
static bool     candidateExcluded[QUORUM_CANDIDATES]={false,false,false,false,false,false};
static uint8_t  evalCount=0;                     // accepted events scored, vs QUORUM_MAX
static int8_t   scores[QUORUM_CANDIDATES]={0,0,0,0,0,0};
static int8_t   leaderIdx=-1, runnerUpIdx=-1;    // indices into QUORUM_OFFSETS
static int8_t   quorumMargin=0;                  // scores[leader]-scores[runnerUp]

// Evidence ring (§2.4): every accepted event is appended once position is
// declared — in NAV_NORMAL, NAV_EVALUATING and NAV_NO_QUORUM alike. Rejected
// events are never pushed. Cleared on adoption, navDeclare(), direction change.
static RingEntry evRing[QUORUM_MAX];
static uint8_t   evRingLen=0, evRingHead=0;      // head = oldest

// §3 timing-gate state. previousAcceptedDt holds the interval of the last
// ACCEPTED event — the name is the specification; rejection leaves it alone.
static uint16_t previousAcceptedDt=0;
static bool     previousAcceptedDtValid=false;

// ---------------------------------------------------------------------------
// §3Q QUARANTINE (docs/QUORUM_QUARANTINE_AND_SELF_RESOLUTION_PROPOSAL.md).
// Operator rule 2026-08-14: a doubtful event is HELD, not committed — "the
// record should come from the next magnet reading with good credentials."
//
// The decisive test is PHYSICAL IMPOSSIBILITY, not a model and not a ratio:
// two independent sensors (38,671 clean marker intervals; 17,584 IR readings)
// agree the fleet's top speed is ~441 mm/s at p99.9. At twice that speed the
// route's shortest spacing (280 mm) still takes 317 ms, so an event arriving
// sooner than Q_FLOOR_MS after the previous DETECTED event cannot be the next
// magnet. Corroborating credentials — flux, duration, polarity, all measured
// against trailing medians of ACCEPTED events, no PWM anywhere — catch the
// slower phantom families (crawl doubles at 420-660 ms) and are gated to
// steady running where their distributions were calibrated.
//
// Arbitration: the NEXT event's polarity is tested against the map under both
// hypotheses (pending genuine vs pending phantom). Discard is the PRIMARY
// verdict on a tie or a double-miss (operator: phantom is "the most likely
// answer" and a wrong discard is an offset of -1, inside the fence, which
// QUORUM adopts and closes routinely — the shadow path costs one adoption).
// A discarded event's interval is folded into its successor's dt so the
// committed timing chain still measures physical travel.
// 2026-08-21: per-locomotive so a raised floor can be TRIALLED on one machine
// with the other as an untouched control. The shortest interval on the map is
// 280 mm (mm 126->127); the fleet's measured maximum is 400 mm/s (Toby,
// 2026-08-20), which crosses it in 700 ms. 350 ms therefore implies 800 mm/s,
// a 2.0x margin. Otto trials 500 ms = 560 mm/s, a 1.4x margin.
// Do NOT raise this toward 700: that is the operating point, not a margin.
#ifndef Q_FLOOR_MS_OVERRIDE
static const uint16_t Q_FLOOR_MS        = 350;   // physics; see derivation above
#else
static const uint16_t Q_FLOOR_MS        = Q_FLOOR_MS_OVERRIDE;
#endif
static const float    Q_INT_RATIO      = 0.40f;  // vs trailing median accepted dt
static const float    Q_FLUX_RATIO     = 0.55f;  // vs trailing median accepted peak
static const float    Q_DUR_RATIO      = 3.5f;   // vs trailing median accepted duration
static const uint16_t Q_STEADY_DT_MIN  = 600;    // conjunction calibrated in this band
static const uint16_t Q_STEADY_DT_MAX  = 4000;
#define Q_HIST_N 11
static uint16_t qDtHist[Q_HIST_N], qPkHist[Q_HIST_N], qDuHist[Q_HIST_N];
static uint8_t  qHistLen=0, qHistIdx=0;
static bool        qPendingValid=false;
static MarkerEvent qPending;
static uint16_t    qPendingDt=0;
static uint8_t     qPendingCorrOff=0;   // 0.3B: R3 proposal held with the event
static uint32_t    qQuarantined=0, qDiscarded=0, qCommitted=0;  // loopstat-grade counters
static void qNoteAccepted(const MarkerEvent& e, uint16_t dt);
static uint16_t qMedian(const uint16_t* h, uint8_t n);

// §2.5Q NO_QUORUM SELF-RESOLUTION — "never stop learning" (same proposal).
// The terminal state keeps its motor policy (stop once, in AUTO) and its
// snapshot; what changes is that the ring keeps being SCORED. Once DNA_W
// fresh events have arrived since the terminal entry — i.e. the ring holds
// only post-failure evidence — every further event runs a ROUTE-WIDE exact
// unique window match (not the fenced ±REACQ advisory: the 2026-08-14
// incident sat 32 markers outside the fence, and 141 post-failure markers
// matched exactly one route position at every window length tested). A
// unique match must then stay consistent — each new event's match advancing
// by exactly one — until NQ_CONFIRM_N consistent matches have been seen IN
// ALL (the first match plus two further advances; 0035's wording was
// corrected to this, the code's actual rule, in the CODEX review round)
// before the navigator relabels itself, returns to NORMAL and publishes
// SELF_RESOLVED. AUTO is DROPPED BY the resolution (the 1.16R resume
// interlock); recovery of knowledge is not recovery of motion (CODEX).
static const uint8_t NQ_CONFIRM_N = 3;
static uint16_t nqFreshEvents=0;
static uint8_t  nqCandidate=255;   // route-wide match, 255 = none
static uint8_t  nqConfirm=0;
static uint8_t  dnaMatchWide(int8_t dir);
static void     nqLearn(const MarkerEvent& e);
static void     nqDropAutoInterlock();   // finding 1: defined after MQTT (topics)

// ---------------------------------------------------------------------------
// IR TEST A — types, state and prototypes (bodies live in the IR section
// below the CTO layer). Declared HERE, above every use site, because the
// Arduino prototype generator cannot hoist functions whose signatures use
// mid-file types — the same trap that bit the marker types once already.
// ---------------------------------------------------------------------------
#if IR_TEST_A_ON
static const uint8_t  IR_SENSOR_MAC[6]  = IR_SENSOR_MAC_BYTES;
static const uint32_t IR_LINK_STALE_MS  = 500;   // provisional, observation-only (§2)
static const uint32_t IR_PROJECT_MAX_MS = 150;   // provisional, observation-only (§2)
static const float    IR_MM_PER_PULSE   = 9.652f;
static const float    PKPH_PER_MMPS     = 1.0f/5.37325f;

// ---- receive queue (§5.2): dedicated, so a 20 Hz sensor can never evict or
// delay CTO truth. 32 entries, callback never blocks, overflow counts.
struct IrRxItem { IrSpeedPacketV1 p; uint8_t mac[6]; uint32_t rxMs; };
static QueueHandle_t irRxQueue=nullptr;
// ---- the eight validation reject counters (§5.1), callback context --------
static volatile uint32_t irRejBadLen=0;      // 0xC6-first-byte frame, wrong length
static volatile uint32_t irRejBadMagicVer=0; // magic/version mismatch at length 72
static volatile uint32_t irRejBadWire=0;     // bytes!=72 or reserved!=0
static volatile uint32_t irRejBadSource=0;   // source MAC != configured sensor
static volatile uint32_t irRejBadSensor=0;   // sensorId mismatch
static volatile uint32_t irRejBadTarget=0;   // targetLocoId != LOCO_ID
static volatile uint32_t irRejBadEnum=0;     // validity outside closed 0-5
static volatile uint32_t irRejBadEncoding=0; // non-canonical speed encoding
static volatile uint32_t irRxQueueDrops=0;
// ---- loop-owned accepted state (§5.3). No mux: the loop owns all of it. ----
struct IrSnap { uint32_t rxMs, capturedMs, pulses, speedX100, seq; uint8_t validity; };
static IrSnap   irHist[32];                 // newest at irHistLen-1 semantics via ring
static uint8_t  irHistLen=0, irHistHead=0;
static bool     irEpochOpen=false;
static uint32_t irBootId=0, irLastSeq=0, irLastPulses=0;
static uint32_t irAccepted=0, irSeqGaps=0, irDups=0, irOutOfOrder=0;
static uint32_t irPulseRegressions=0, irSensorReboots=0;
static uint32_t irPulseRebases=0;
static uint8_t  irRegressStreak=0;
static uint32_t irLastAcceptRxMs=0, irRxGapMs=0, irRxMaxGapMs=0;
static IrSpeedPacketV1 irLatest;            // last accepted packet (copy)
static uint32_t irLatestRxMs=0;
static bool     irHaveLatest=false;
// ---- IR distance anchor + event-estimate ring (§8.3) ----------------------
static bool   irAnchorValid=false;
// IR odometry coverage accumulators (2026-08-19, observation only). The
// per-segment ratio in mm/speed is the primary record; this rolling pair is a
// convenience for the operator and MUST NOT become an input to any decision.
// Reset on declaration and on direction change — a session's coverage is not
// comparable across either, and a stale denominator would quietly flatter a
// later run.
static float    irCovPulses=0.0f, irCovExpected=0.0f;
static uint32_t irCovSegments=0;
static void irCovReset(){ irCovPulses=0.0f; irCovExpected=0.0f; irCovSegments=0; }
static float  irAnchorDistMm=0.0f;          // projected cumulative distance at last accepted Hall anchor
struct IrEventEst {
  uint32_t detectedAtMs=0;                  // stable identity (§9.3)
  bool     valid=false;
  float    absDistMm=0.0f;                  // projection at event time
  float    deltaMm=0.0f;                    // vs anchor at estimate time
  int32_t  deltaPulses=0;
  uint32_t irAgeMs=0;
  bool     anchorWasValid=false;
};
static IrEventEst irEstRing[16];
static uint8_t    irEstNext=0;
// ---- marker-speed observer (§7) --------------------------------------------
static bool     mmAnchorValid=false;
static uint8_t  mmAnchorMm=0;
static uint32_t mmAnchorDetMs=0;
static bool     mmSpeedValid=false;
static float    mmSpeedMmps=0.0f;
static uint8_t  mmSpeedFrom=0, mmSpeedTo=0;
static uint32_t mmSpeedAtMs=0;
static uint32_t mmSpeedDtMs=0;
static char T_MM_SPEED[64], T_IR_SPEED_T[64], T_IR_STATUS[64], T_SPEED_VIEW[64];
static uint32_t irOversizePubs=0;
struct IrView { bool numeric; float mmps; const char* state; uint32_t ageMs; };
static uint32_t irLastSpeedPubMs=0, irLastStatusPubMs=0, irReportSeq=0;

static void irEnsureQueue();
static void serviceIrRx();
static void serviceIrTelemetry();
static void irObserversReset(const char*);
static void irOnAccepted(const MarkerEvent& e);
static void irObsFinalize(const MarkerEvent& e,const char* disp);
static void irAcceptFrame(const uint8_t* srcMac,const uint8_t* data,int len,uint32_t rxMs);
static IrEventEst irEstimateAt(uint32_t detMs);
static void irPublishMmSpeed(const MarkerEvent& e,const IrEventEst& est,
                             uint8_t fromMm,int revision,const char* finalDisp);
static uint32_t irRouteSpanMm(uint8_t fromMm,int8_t dir,uint8_t k);
static IrView irCurrentView();
static const char* irWireStateName(uint8_t v);
static void irHistPush(const IrSnap& sn);
static const IrSnap* irHistAt(uint8_t i);
static const IrEventEst* irEstFind(uint32_t detMs);
static bool irSensorMacUsable();
static void irObserveEventPre(const MarkerEvent& e);
static void irObserveEventPost(const MarkerEvent& e);
#else
static inline void irEnsureQueue(){}
static inline void serviceIrRx(){}
static inline void serviceIrTelemetry(){}
static inline void irObserversReset(const char*){}
static inline void irOnAccepted(const MarkerEvent&){}
static inline void irObsFinalize(const MarkerEvent&,const char*){}
static inline void irObserveEventPre(const MarkerEvent&){}
static inline void irObserveEventPost(const MarkerEvent&){}
#endif

static unsigned long lastMarkerMs=0;             // EVERY received event advances this
static uint16_t lastSegmentDt=0;
// Published on every marker by drainMarkers() (§5): why the gate was inactive
// and what it computed, explicit rather than omitted.
static const char* lastTimingGate="NO_POSITION";
static uint32_t    lastDtExpected=0;
static float       lastDtConserveRatio=-1.0f;

// The last marker whose reading actually AGREED with the map. Everything after
// it is inference; this is the position a following locomotive can trust.
// Operator ruling 2026-08-14: a start interval sent before the session
// direction is HELD, not refused, and applied as soon as the direction lands.
static uint8_t       pendingIntervalA=0, pendingIntervalB=0;
static bool          havePendingInterval=false;
static uint8_t       lastConfirmedMm=0;
static unsigned long lastConfirmedMs=0;
static uint16_t      markersSinceConfirmed=0;
static bool          haveConfirmed=false;

// NAV_NO_QUORUM timing, published as lost_ms / lost_markers (field names kept
// for the dashboard; the LOST state itself no longer exists).
static unsigned long lostSinceMs=0;
static uint16_t      lostMarkers=0;

// ---------------------------------------------------------------------------
// §2.5 desired-retained-state slot for mm/no_quorum. The snapshot never
// enters a queue: retain acts at the BROKER, and a queued snapshot was an
// ordinary evictable entry in the local drop-oldest pubQueue — churning
// exactly when NO_QUORUM fires (broker unreachable). One protected variable
// outside every queue; networkTask() reconciles the broker to it and returns
// it to NONE only on publish success. Terminal entry sets SNAPSHOT;
// navDeclare() sets CLEAR; both directions get the same durability.
// ---------------------------------------------------------------------------
enum DesiredRetained : uint8_t { DRS_NONE=0, DRS_SNAPSHOT, DRS_CLEAR };
// F3 (CODEX finding 3): the DESIRED state is PERSISTENT — changed only by
// terminal entry (SNAPSHOT) and navDeclare() (CLEAR), never by a successful
// publish. A separate per-connection flag says the broker needs re-syncing;
// attemptReconnect() re-arms it on every successful reconnect, so a broker
// restart while the locomotive sits in NAV_NO_QUORUM still gets the snapshot
// back, indefinitely.
//
// POLICY: this slot mirrors CURRENT truth. If the operator redeclares while
// the broker is unreachable, CLEAR legitimately supersedes an undelivered
// SNAPSHOT — the forensic record of the incident is the NO_QUORUM decision
// event in the durable marker queue, not the retained mirror.
static volatile uint8_t desiredRetainedNoQuorum = DRS_NONE;
static volatile bool    noQuorumNeedsReconcile  = false;
// v1.2 (CODEX 1.1 review): the completion guard compared only the enum value,
// which misses a NEWER commit with the SAME value — snapshot B committed while
// snapshot A publishes (repeated force_lost NOQUORUM), or the classic ABA
// SNAPSHOT->CLEAR->SNAPSHOT during a slow publish. Every commit increments
// this generation under the mux; reconciliation completes only if state AND
// generation both still match what it copied out.
static uint32_t noQuorumGeneration = 0;   // read/written only under noQuorumMux
// F2 (CODEX finding 2): loop() writes the snapshot buffer; networkTask()
// reads it. Both sides take this mux; the network task copies out under it
// and publishes from the copy — the mux is NEVER held across mqtt.publish().
static portMUX_TYPE noQuorumMux = portMUX_INITIALIZER_UNLOCKED;
static char noQuorumSnapshot[512];   // §2.5: sized to PubMsg::payload — do NOT enlarge PubMsg

static void navPublishState(const char* ev,const MarkerEvent* e);
static void publishAlert(const char* level,const char* reason);
// HARD_BOUND advisory (decision 0023); defined with dnaMatch() below, called
// only by buildNoQuorumSnapshot(). Diagnostic — it never moves the locomotive.
static uint8_t quorumAdvisoryMarker();

// §6.1 — the only navState vocabulary the rest of the sketch uses.
static inline bool navPositionUsable(){ return navState==NAV_NORMAL || navState==NAV_EVALUATING; }
static const char* navStateName(){
  switch(navState){
    case NAV_NORMAL:     return "NORMAL";
    case NAV_EVALUATING: return "EVALUATING";
    case NAV_NO_QUORUM:  return "NO_QUORUM";
    default:             return "UNSET";
  }
}
static const char* navAlertLevel(){   // one helper, BOTH broadcast and reconnect
  switch(navState){
    case NAV_NO_QUORUM:  return "NO_QUORUM";
    case NAV_EVALUATING: return "EVALUATING";   // reconnect mid-evaluation must not report CLEAR
    case NAV_NORMAL:     return "CLEAR";
    default:             return "UNSET";
  }
}

// Track distance spanned by n markers travelled from mm in direction dir.
static uint32_t spanMm(uint8_t mm,int8_t dir,uint16_t n){
  uint32_t t=0; uint8_t cur=mm;
  for(uint16_t i=0;i<n && i<DNA_N;i++){
    uint8_t nxt=nextMm(cur,dir);
    t += (dir==MAP_CW) ? pgm_read_word(&spacingMm[cur]) : pgm_read_word(&spacingMm[nxt]);
    cur=nxt;
  }
  return t;
}

// ---------------------------------------------------------------------------
// JSON fragments shared by decision events, state and the snapshot.
// Excluded candidates publish as null with a parallel boolean array (§5) —
// whatever integer remains in the underlying score slot must never be read.
// ---------------------------------------------------------------------------
static int jsonScores(char* out,size_t n){
  int w=snprintf(out,n,"[");
  for(uint8_t i=0;i<QUORUM_CANDIDATES;i++){
    if(candidateExcluded[i]) w+=snprintf(out+w,n-w,"%snull",i?",":"");
    else                     w+=snprintf(out+w,n-w,"%s%d",i?",":"",(int)scores[i]);
  }
  w+=snprintf(out+w,n-w,"]");
  return w;
}
static int jsonExcluded(char* out,size_t n,bool numeric){
  int w=snprintf(out,n,"[");
  for(uint8_t i=0;i<QUORUM_CANDIDATES;i++)
    w+=snprintf(out+w,n-w,"%s%s",i?",":"",
                numeric?(candidateExcluded[i]?"1":"0")
                       :(candidateExcluded[i]?"true":"false"));
  w+=snprintf(out+w,n-w,"]");
  return w;
}
// §4 viability: an offset is viable when it trails the leader by FEWER than
// QUORUM_MARGIN points — strictly. A candidate one behind has NOT been
// excluded; the published list must cover it.
static int jsonViable(char* out,size_t n){
  int w=snprintf(out,n,"[");
  bool first=true;
  if(leaderIdx>=0){
    for(uint8_t i=0;i<QUORUM_CANDIDATES;i++){
      if(candidateExcluded[i]) continue;
      if(scores[leaderIdx]-scores[i] < QUORUM_MARGIN){
        w+=snprintf(out+w,n-w,"%s%d",first?"":",",(int)QUORUM_OFFSETS[i]);
        first=false;
      }
    }
  }
  w+=snprintf(out+w,n-w,"]");
  return w;
}

// --- evidence ring ---------------------------------------------------------
static void clearRing(){ evRingLen=0; evRingHead=0; }
static RingEntry* ringAt(uint8_t i){            // 0 = oldest
  return &evRing[(uint8_t)((evRingHead+i)%QUORUM_MAX)];
}
static void pushRing(uint8_t pol,uint8_t mm){
  if(evRingLen<QUORUM_MAX){
    evRing[(uint8_t)((evRingHead+evRingLen)%QUORUM_MAX)]={pol,mm};
    evRingLen++;
  }else{
    evRing[evRingHead]={pol,mm};                // overwrite oldest
    evRingHead=(uint8_t)((evRingHead+1)%QUORUM_MAX);
  }
}

// --- scoring (§2.2): plain match counts over the evaluation window ---------
// A match adds exactly 1; a mismatch adds nothing. No penalty, no weighting,
// no prior — a weak or drifting read counts exactly as much as a strong one.
// Excluded candidates are not scored at all.
static void clearScores(){
  for(uint8_t i=0;i<QUORUM_CANDIDATES;i++) scores[i]=0;
  leaderIdx=-1; runnerUpIdx=-1; quorumMargin=0;
}
static void scoreEntry(const RingEntry* r){
  for(uint8_t c=0;c<QUORUM_CANDIDATES;c++){
    if(candidateExcluded[c]) continue;
    // Score against the navMm recorded WITH that reading, not the current one.
    if(r->polarity == dnaAt(routeMod((int32_t)r->navMm + navDir*QUORUM_OFFSETS[c])))
      scores[c]++;
  }
}
static void scoreNewestRingEntry(){ if(evRingLen) scoreEntry(ringAt(evRingLen-1)); }
static void scoreLastN(uint8_t n){
  uint8_t from=(evRingLen>n)?(uint8_t)(evRingLen-n):0;
  for(uint8_t i=from;i<evRingLen;i++) scoreEntry(ringAt(i));
}
static void computeLeaderRunnerUpMargin(){
  leaderIdx=-1; runnerUpIdx=-1; quorumMargin=0;
  for(uint8_t i=0;i<QUORUM_CANDIDATES;i++){
    if(candidateExcluded[i]) continue;          // no part in leader selection
    if(leaderIdx<0 || scores[i]>scores[leaderIdx]){ runnerUpIdx=leaderIdx; leaderIdx=(int8_t)i; }
    else if(runnerUpIdx<0 || scores[i]>scores[runnerUpIdx]) runnerUpIdx=(int8_t)i;
  }
  if(leaderIdx>=0 && runnerUpIdx>=0)
    quorumMargin=(int8_t)(scores[leaderIdx]-scores[runnerUpIdx]);
}

// --- incident lifecycle (§2.6) ---------------------------------------------
static void clearCandidateExclusions(){
  for(uint8_t i=0;i<QUORUM_CANDIDATES;i++) candidateExcluded[i]=false;
}
#ifdef Q_TIMING_MEASURED
// CODEX 2026-08-21 finding 1: the rhythm-escape memory is TIMING HISTORY and
// must die with the rest of it. Before this, a cadence rejected before a
// direction change / stop / LOW_PWM / RAMP / declaration survived the reset,
// and an unrelated event resembling it could be accepted with no conservation
// test at all.
static void invalidatePreviousAcceptedDt(){ previousAcceptedDtValid=false; qLastRejectedDt=0; }
#else
static void invalidatePreviousAcceptedDt(){ previousAcceptedDtValid=false; }
#endif

// After a confirming agreement: the incident is over, every offset eligible.
static void endSuccessfulIncident(){
  clearScores(); clearCandidateExclusions();
  evalCount=0; adoptionFailureCount=0;
  adoptionPendingValidation=false; adoptionDisagreeStreak=0;
  adoptedOffset=NO_ADOPTED_OFFSET;
  publishQuorumDecision("QUORUM_CLOSED","");    // incident close (§5.1)
}

// On NAV_NO_QUORUM: clears ONLY the provisional-adoption state. Scores,
// exclusions, evalCount, the ring, leader, runner-up and margin are RETAINED
// until navDeclare() or a full reset — they are the record of why the
// locomotive stopped, and every state publication while stopped reports them.
static void closeIncidentNoQuorum(){
  adoptionPendingValidation=false;
  adoptedOffset=NO_ADOPTED_OFFSET;
}

// navDeclare() and direction change both clear EVERYTHING recovery-related.
static void fullRecoveryReset(){
  clearRing(); clearScores(); clearCandidateExclusions();
  evalCount=0; missStreak=0;
  adoptionPendingValidation=false; adoptionDisagreeStreak=0;
  adoptionFailureCount=0; adoptedOffset=NO_ADOPTED_OFFSET;
  invalidatePreviousAcceptedDt();
  // lastMarkerMs must reset too: it used to survive navDeclare(), so the first
  // interval after a declaration spanned a stop or a carried locomotive.
  // Resetting makes that first dt zero, which §3 refuses to bootstrap from.
  lastMarkerMs=0;
}

// §5: stamp last-confirmed from the event's detection time, not millis() — a
// 49-second loop stall (2026-07-29) makes a millis() stamp report fresher
// than it is.
static void updateLastConfirmed(uint8_t mm,unsigned long detectedAtMs){
  lastConfirmedMm=mm; lastConfirmedMs=detectedAtMs;
  markersSinceConfirmed=0; haveConfirmed=true;
}

// --- §2.5 terminal snapshot -------------------------------------------------
// Step 1 of NO_QUORUM entry: a RAM write into the desired-retained-state
// slot — nothing is enqueued and nothing can block, which is why it may
// safely precede the stop request. If this mechanism is ever replaced by a
// synchronous publish, the order must be reversed. Short keys are acceptable
// here and nowhere else — this message is read by a human doing forensics.
// `reason` is the terminal reason, forwarded from enterNoQuorum(). It exists
// solely to scope the advisory: decision 0023 grants it to HARD_BOUND and to
// nothing else.
static void buildNoQuorumSnapshot(const char* reason){
  char sc[48], ex[24], ld[8], ru[8];
  jsonScores(sc,sizeof(sc));
  jsonExcluded(ex,sizeof(ex),true);
  // F1 (CODEX finding 1): ld/ru are OFFSETS (-1..+4), converted through
  // QUORUM_OFFSETS[] exactly as the decision events do — never the raw
  // candidate indices 0..5. null when no leader/runner-up exists.
  if(leaderIdx>=0)   snprintf(ld,sizeof(ld),"%d",(int)QUORUM_OFFSETS[leaderIdx]);   else strlcpy(ld,"null",sizeof(ld));
  if(runnerUpIdx>=0) snprintf(ru,sizeof(ru),"%d",(int)QUORUM_OFFSETS[runnerUpIdx]); else strlcpy(ru,"null",sizeof(ru));
  // Advisory (decision 0023): exact unique window match, or nothing. Computed
  // here because the ring, navMm and navDir are all still intact — step 2's
  // stop and step 2a's stationReset() have not run yet. Read-only.
  //
  // HARD_BOUND ONLY. This function serves all three terminal reasons, and the
  // other two carry evidence the advisory has no right to interpret:
  //
  //   SECOND_ADOPTION_FAILED — handleFailedAdoption() has REBASED the ring's
  //     navMm values to undo the failed adoption and rescored only the last
  //     three entries, so evalCount is 3 while evRingLen may still be 12. The
  //     ring is in what the code at the call site calls "the uncorrected
  //     frame": the polarities dnaMatch() reads are real, but the incident is
  //     one where the navigator has already been wrong once about position.
  //   FORCED_BY_FIXTURE — the operator forced the terminal state. The ring can
  //     hold anything, including readings from an unrelated stretch, and a
  //     confident marker number offered on that basis is exactly the wrong
  //     hint this advisory exists to avoid being.
  //
  // Neither has field evidence behind it, so neither gets an advisory. The
  // audit fields still publish, so a null here is distinguishable from a null
  // caused by a short ring or a failed match: advn reports the ring length.
  const bool advisoryAllowed = (strcmp(reason,"HARD_BOUND")==0);
  uint8_t adv = advisoryAllowed ? quorumAdvisoryMarker() : ADVISORY_NONE;
  char av[8];
  if(adv==ADVISORY_NONE) strlcpy(av,"null",sizeof(av));
  else                   snprintf(av,sizeof(av),"%u",adv);
  // F2: build into a loop-thread local buffer, then hand off under the mux.
  char tmp[512];
  int w=snprintf(tmp,sizeof(tmp),
    "{\"e\":\"NO_QUORUM\",\"mm\":%u,\"lm\":\"%s\",\"since\":%u,\"dir\":\"%s\","
    "\"sc\":%s,\"ex\":%s,\"ld\":%s,\"ru\":%s,\"mg\":%d,\"ev\":%u,"
    // adv/advw/advr/advn are the advisory and the three inputs that decide it,
    // so silence can be told apart from a match: advn<advw means the ring was
    // short, otherwise no unique exact window existed at that width and radius.
    // With those, navMm, dir and the ring below, the verdict is recomputable.
    "\"adv\":%s,\"advw\":%u,\"advr\":%u,\"advn\":%u,\"ring\":[",
    navMm, haveConfirmed?landmarkAt(lastConfirmedMm):"",
    (unsigned)markersSinceConfirmed, dirName(navDir),
    sc, ex, ld, ru, (int)quorumMargin, (unsigned)evalCount,
    av, (unsigned)DNA_W, (unsigned)REACQ_WINDOW_MARKERS, (unsigned)evRingLen);
  for(uint8_t i=0;i<evRingLen && w<(int)sizeof(tmp);i++){
    const RingEntry* r=ringAt(i);
    w+=snprintf(tmp+w,sizeof(tmp)-w,
                "%s[\"%c\",%u]", i?",":"", polChar(r->polarity), r->navMm);
  }
  if(w<(int)sizeof(tmp))
    w+=snprintf(tmp+w,sizeof(tmp)-w,"]}");
  // §2.5: a silently truncated forensic record is worse than a missing one.
  if(w>=(int)sizeof(tmp)){
    publishAlert("NO_QUORUM","SNAPSHOT_TRUNCATED");
    Serial.printf("[NAV] SNAPSHOT_TRUNCATED at %d bytes\n",w);
  }
  portENTER_CRITICAL(&noQuorumMux);
  memcpy(noQuorumSnapshot,tmp,sizeof(noQuorumSnapshot));
  desiredRetainedNoQuorum=DRS_SNAPSHOT;
  noQuorumGeneration++;                 // v1.2: every commit is a new generation
  noQuorumNeedsReconcile=true;
  portEXIT_CRITICAL(&noQuorumMux);
}

// --- NAV_NO_QUORUM entry (§2.5, in this order) ------------------------------
static void enterNoQuorum(const char* reason){
  // v1.4 (CODEX): capture the chamber at entry so the log tells the truth in
  // both — "stopped" was false in MANUAL, where the motor is untouched.
  bool wasAuto = autoRunning;
  navState=NAV_NO_QUORUM;
  navLostCount++;
  lostSinceMs=millis(); lostMarkers=0;
  // §2.5Q: the self-resolution clock starts now. The ring still holds the
  // twelve readings that caused this; they must age out before it is trusted.
  nqFreshEvents=0; nqCandidate=255; nqConfirm=0;
  // 1. snapshot the terminal evidence before deceleration can overwrite it
  buildNoQuorumSnapshot(reason);
  // 2. controlled stop — AUTO CHAMBER ONLY, issued once on entry, never
  //    reissued on later markers. BICAMERAL RULE (operator ruling, spec
  //    §0.2): in MANUAL the operator is sovereign — navigation observes,
  //    records, publishes and warns, but NEVER writes to the motor; in AUTO
  //    navigation acts with full authority. v2.22's LOST handler stated the
  //    same doctrine — "AUTO only. In manual the operator has the throttle
  //    and the navigator does not take it. Navigation observes always;
  //    navigation acts only in AUTO." — and 1.0-1.2 regressed it here, once.
  //    In MANUAL the locomotive keeps the operator's commanded PWM and the
  //    operator learns of NO_QUORUM through every publication channel.
  if(autoRunning) requestPwm(0,NORMAL_STEP_MS);
  // 2a. the station machine must not retain a continuation that is no longer
  //     valid (ST_DEPART would resume without cruise; ST_FINAL without its
  //     M+1 timer). Existing routine; no new station machinery.
  stationReset("NO_QUORUM");
  // 3. retained alert: last confirmed marker/landmark, markers since, and the
  //    viable candidate list (occupancy bounds themselves are M5).
  publishAlert("NO_QUORUM",reason);
  { char extra[96];
    snprintf(extra,sizeof(extra),",\"reason\":\"%s\"",reason);
    publishQuorumDecision("NO_QUORUM",extra); }
  // Retain navMm, navDir, autoRunning, last-confirmed and ALL diagnostics.
  // AUTO is not dropped, but the locomotive does not resume.
  closeIncidentNoQuorum();          // never endSuccessfulIncident() here
  if(wasAuto)
    Serial.printf("[NAV] NO_QUORUM (%s) — AUTO stop requested; operator declaration required\n",reason);
  else
    Serial.printf("[NAV] NO_QUORUM (%s) — MANUAL, motor unchanged; operator declaration required\n",reason);
}

// --- evaluation decisions (§2.2/§2.4) ---------------------------------------
static void adoptLeader(){
  int8_t off=QUORUM_OFFSETS[leaderIdx];
  uint8_t oldMm=navMm;
  navMm=routeMod((int32_t)navMm + navDir*off);
  { char extra[64];
    snprintf(extra,sizeof(extra),",\"old_mm\":%u,\"new_mm\":%u,\"offset\":%d",
             oldMm,navMm,(int)off);
    // Publish BEFORE clearing — scores, leader, runner-up and margin are the
    // record of why the adoption was made (§2.2).
    publishQuorumDecision("QUORUM_ADOPTED",extra); }
  clearRing(); clearScores(); missStreak=0;     // AFTER publishing
  evalCount=0;
  adoptionPendingValidation=true; adoptionDisagreeStreak=0;
  adoptedOffset=off;
  navState=NAV_NORMAL;    // the incident stays open through validation (§2.6)
}

// §2.4Q THE INSERTION HYPOTHESIS (CODEX, 2026-08-14, weakness #4).
//
// QUORUM scores CONSTANT offsets. A phantom is an INSERTION: before it the
// correct offset is 0, after it the correct offset is -1, and a scoring
// window that straddles the insertion fits no single candidate — the
// 2026-08-14 17:21 board [8,8,8,4,7,3] is exactly that shape. QUORUM had no
// hypothesis meaning "discard event five; everything else fits", so a
// temporary ambiguity became terminal at the hard bound.
//
// The minimum sound remedy: at the hard bound, before going terminal, ask
// whether the NEWEST entries — a suffix the contamination has aged out of —
// fit exactly one candidate perfectly. Suffix length 7 of ring 12: a wrong
// offset matches a random suffix of 7 with p≈1/128, and the test runs only
// at the hard bound, only over the existing fence, and demands uniqueness.
// If two candidates both fit, or none does, the terminal entry proceeds
// exactly as before — and §2.5Q self-resolution takes over from there with
// wholly fresh evidence.
static const uint8_t SUFFIX_RESCUE_N = 7;
static uint8_t suffixLenAt(uint8_t c){
  uint8_t run=0;
  for(uint8_t k=0;k<evRingLen;k++){
    const RingEntry* r=ringAt(evRingLen-1-k);   // newest first
    if(r->polarity == dnaAt(routeMod((int32_t)r->navMm + navDir*QUORUM_OFFSETS[c])))
      run++;
    else break;
  }
  return run;
}
static int8_t suffixRescueCandidate(){
  int8_t hit=-1;
  for(uint8_t c=0;c<QUORUM_CANDIDATES;c++){
    if(candidateExcluded[c]) continue;
    if(suffixLenAt(c)>=SUFFIX_RESCUE_N){
      if(hit>=0) return -1;                     // two fit: ambiguous, no rescue
      hit=(int8_t)c;
    }
  }
  return hit;
}

// One decision function, adoption tested FIRST (§2.4): if reading twelve is
// what finally produces the two-point margin, the locomotive has identified
// its position and must adopt. The hard bound means "twelve readings without
// a margin", not "twelve readings then stop regardless".
static void decideEvaluation(){
  computeLeaderRunnerUpMargin();
  if(leaderIdx>=0 && runnerUpIdx>=0 && quorumMargin>=QUORUM_MARGIN){
    adoptLeader();
  }else if(evalCount>=QUORUM_MAX){
    // §2.4Q: the insertion hypothesis gets one look before the terminal.
    int8_t rescue=suffixRescueCandidate();
    if(rescue>=0){
      char extra[96];
      snprintf(extra,sizeof(extra),
        ",\"suffix\":%u,\"rescue_offset\":%d",
        (unsigned)suffixLenAt((uint8_t)rescue),(int)QUORUM_OFFSETS[rescue]);
      publishQuorumDecision("QUORUM_SUFFIX_RESCUE",extra);
      leaderIdx=rescue;                 // adoptLeader() reads the leader slot
      adoptLeader();
    }else{
      enterNoQuorum("HARD_BOUND");
    }
  }else{
    // UNRESOLVED: a leader exists but margin < 2 — publish with every viable
    // candidate (§4) and keep collecting. Run 3 held -1 and +1 level at 4/4
    // for four readings through an alternating stretch. This is not lost.
    publishQuorumDecision("QUORUM_TIED","");
  }
}

// beginNewEvaluation(): a NEW incident — missStreak reached 3 in ordinary
// running. Must NOT share initialisation with handleFailedAdoption(): running
// incident-begin code on a reopen would erase the failure count and the
// exclusion that reopening exists to preserve (§2.6).
static void beginNewEvaluation(){
  adoptionFailureCount=0;
  clearCandidateExclusions();
  clearScores();
  // the three triggering entries are ALREADY in the ring — do not re-push
  scoreLastN(QUORUM_TRIGGER);
  evalCount=QUORUM_TRIGGER;      // NOT 0 (the bound would fire at 15), not 6
  decideEvaluation();            // may adopt without a fourth marker
}

// handleFailedAdoption(): the SAME incident, continued (§2.3). A provisional
// adoption was contradicted three times while still under validation.
static void handleFailedAdoption(){
  int8_t failed=adoptedOffset;
  // Remove the correction from the CURRENT odometer — the post-adoption
  // advances were real. navMm=154, adopt -1 -> 153, three events -> 156,
  // judged wrong: removing the correction gives 157. Restoring 154 would
  // throw away three markers of travel.
  navMm=routeMod((int32_t)navMm - navDir*failed);
  // Rebase the evidence ring: the retained entries recorded navMm in the
  // adopted frame; unrebased they test every hypothesis against the wrong
  // map positions.
  for(uint8_t i=0;i<evRingLen;i++){
    RingEntry* r=ringAt(i);
    r->navMm=routeMod((int32_t)r->navMm - navDir*failed);
  }
  adoptionFailureCount++;
  for(uint8_t i=0;i<QUORUM_CANDIDATES;i++)      // excluded = removed, not deprioritised
    if(QUORUM_OFFSETS[i]==failed) candidateExcluded[i]=true;
  adoptionPendingValidation=false; adoptionDisagreeStreak=0;
  adoptedOffset=NO_ADOPTED_OFFSET;
  missStreak=0;                  // already served its purpose (§2.3)
  clearScores();
  evalCount=QUORUM_TRIGGER;
  scoreLastN(QUORUM_TRIGGER);    // reconstruct over the 3 rebased entries
  computeLeaderRunnerUpMargin();
  if(adoptionFailureCount==1){
    navState=NAV_EVALUATING;     // same incident, continued
    char extra[64];
    snprintf(extra,sizeof(extra),",\"removed_offset\":%d,\"rebased_mm\":%u",
             (int)failed,navMm);
    publishQuorumDecision("QUORUM_REOPENED",extra);  // record the failure first
    decideEvaluation();          // may adopt another offset at once — same
                                 // function, not a copy of its logic
  }else{
    // Second failure: the snapshot reports the reconstructed vector, both
    // failed offsets excluded, evalCount=3, in the uncorrected frame.
    enterNoQuorum("SECOND_ADOPTION_FAILED");
  }
}

// §2.3: while validation is pending it owns the comparison outright. Without
// this precedence three post-adoption disagreements would increment
// missStreak AND adoptionDisagreeStreak, firing beginNewEvaluation()
// alongside handleFailedAdoption().
static void handleValidationResult(const MarkerEvent& e,uint8_t reading){
  bool agrees=(reading==dnaAt(navMm));
  if(agrees){
    // Validation ends at the FIRST post-adoption agreement, and a confirming
    // agreement still does all the ordinary work.
    navAgree++;
    navPublishState("AGREE",&e);
    updateLastConfirmed(navMm,e.detectedAtMs);
    missStreak=0;
    endSuccessfulIncident();
    return;
  }
  navDisagree++;
  navPublishState("DISAGREE",&e);
  adoptionDisagreeStreak++;      // NOT missStreak — it stays 0 throughout
  if(adoptionDisagreeStreak==QUORUM_TRIGGER) handleFailedAdoption();
}

// §2.1 NAV_NORMAL — the common path, one comparison. 1.3% of readings are
// bad; being lost is far rarer. One disagreement is free.
static void processNormalComparison(const MarkerEvent& e){
  uint8_t reading=e.polarity;
  if(adoptionPendingValidation){          // §2.3 owns the result entirely
    handleValidationResult(e,reading);
    return;
  }
  if(reading==dnaAt(navMm)){
    navAgree++;
    navPublishState("AGREE",&e);
    missStreak=0;
    updateLastConfirmed(navMm,e.detectedAtMs);
  }else{
    navDisagree++;
    navPublishState("DISAGREE",&e);
    missStreak++;
    // nothing else. no scoring, no search, no relocation.
    if(missStreak==QUORUM_TRIGGER){       // ~one in half a million at 1.3%
      navState=NAV_EVALUATING;
      publishQuorumDecision("QUORUM_OPEN","");   // incident open (§5.1)
      beginNewEvaluation();
    }
  }
}

// Navigation acceptance (§2.4): advance, append, dispatch on state — the
// entry pushed is the entry scored. Do not reduce speed on entering
// NAV_EVALUATING; only NAV_NO_QUORUM stops the locomotive (§2.5 — the old
// LOST drop to PWM 60 drove the train into the regime that collapses the
// baseline).
static void acceptEvent(const MarkerEvent& e, uint8_t corrOff){
  qNoteAccepted(e, lastSegmentDt);   // §3Q: accepted events train the medians
  // 0.3B: navMm is written HERE and nowhere else on the event path. corrOff
  // is R3's correction proposal (0 for an ordinary advance): it commits in
  // the same statement as the acceptance, so a refusal anywhere upstream
  // leaves position untouched — atomic by construction.
  navMm=routeMod((int32_t)navMm + (int32_t)navDir*(int32_t)(1+corrOff));
  irOnAccepted(e);                   // IR TEST A §7/§8.3: observation only —
                                     // marker speed + IR anchor; void, no
                                     // navigation effect, inert on Otto.
  if(markersSinceConfirmed<65535) markersSinceConfirmed++;   // AGREE re-zeroes it
  pushRing(e.polarity,navMm);
  switch(navState){
    case NAV_NORMAL:
      processNormalComparison(e);
      break;
    case NAV_EVALUATING:
      scoreNewestRingEntry();
      evalCount++;
      decideEvaluation();
      break;
    case NAV_NO_QUORUM:
      // §2.5 as amended by §2.5Q: deceleration events are real and are not
      // pretended away. The odometer advances, the ring appends — and the
      // evidence now keeps being SCORED (nqLearn), because 2026-08-14 proved
      // a locomotive can sit for an hour on top of 141 markers that resolve
      // its position uniquely while the old code refused to look at them.
      // Still no motor action, no repeated stop or alert; a self-resolution
      // relabels knowledge only.
      if(lostMarkers<65535) lostMarkers++;
      nqLearn(e);
      break;
    default: break;
  }
}

// Declares POSITION only. Direction has exactly one assignment point,
// applyDirection(), and this is not it. BOTH cmd/start_mm and
// cmd/start_interval land here — recovery from NAV_NO_QUORUM must not depend
// on which one the operator uses (§2), and the console uses start_interval.
static void navDeclare(uint8_t mm){
  navMm=mm; navState=NAV_NORMAL;
  irObserversReset("DECLARED");      // IR TEST A §7: observation only
  fullRecoveryReset();
  lostMarkers=0;
  // §3Q/§2.5Q: a declaration is a clean slate for the credential medians, any
  // held event and the self-resolution pass alike.
  qPendingValid=false; qHistLen=0; qHistIdx=0;
  nqFreshEvents=0; nqCandidate=255; nqConfirm=0;
  // A declaration has no event, so millis() is correct here — the §5 rule to
  // stamp from e.detectedAtMs applies to marker-driven confirmations only.
  lastConfirmedMm=mm; lastConfirmedMs=millis();
  markersSinceConfirmed=0; haveConfirmed=true;
  // §2.5 CLEAR arm: without this the retained snapshot lingers as a ghost,
  // like the CTO2 r10 relics mistaken for a second device on 2026-07-30.
  // F3: CLEAR is the new persistent desired state (it supersedes even an
  // undelivered SNAPSHOT — the mirror carries current truth; the forensic
  // record lives in the durable NO_QUORUM decision event).
  portENTER_CRITICAL(&noQuorumMux);
  desiredRetainedNoQuorum=DRS_CLEAR;
  noQuorumGeneration++;                 // v1.2: every commit is a new generation
  noQuorumNeedsReconcile=true;
  portEXIT_CRITICAL(&noQuorumMux);
  navPublishState("DECLARED",nullptr);
}

// ---------------------------------------------------------------------------
// The v2.x window matcher. dnaPush()/the streaming buffer remain DEAD BY
// INSTRUCTION — present, unreferenced, compiling, not wired up.
//
// dnaMatch() itself is now called from ONE place: buildNoQuorumSnapshot(), to
// put an advisory marker on the terminal record (decision 0023). It is not an
// authority and never was allowed to be. The lineage QUORUM replaced asked
// "where am I?" from scratch at every marker and let the matcher outvote the
// odometer, which is how a locomotive at MM133 concluded at certainty 1.000
// that it was at MM105 travelling the other way. The advisory differs in three
// ways that keep that failure unreachable:
//
//   * it runs ONCE, at HARD_BOUND — after the navigator has already concluded
//     it is lost and is stopping anyway;
//   * it PUBLISHES, never adopts. navMm, the throttle, the station machine,
//     NAV_NO_QUORUM and the operator's declaration are all untouched;
//   * it demands an EXACT, UNIQUE window. Every DNA window of length >= 10 is
//     unique route-wide (W=9 still collides four ways), so a 12-wide exact
//     match names one marker or none. One bad reading in twelve silences it.
//
// Exact-or-silent is the whole contract: a wrong non-null advisory is a
// blocking defect, because its only purpose is to save the operator a walk and
// a wrong hint is worse than no hint.
// (The rest of the v2.x recovery machinery — pendingMm/pendingValid/
// pendingConfirms, REACQ_CONFIRMS, lastOdomDisagreement, navConfidence and
// navEnterLost() — is deleted per spec §6.2.)
// ---------------------------------------------------------------------------
// DNA_W and REACQ_WINDOW_MARKERS are defined with the navigator constants
// above, alongside the rest of the advisory's parameters.
static uint8_t dnaBuf[DNA_W];
static uint8_t dnaBufLen = 0;

__attribute__((unused))
static void dnaPush(uint8_t pol){
  if(dnaBufLen<DNA_W){ dnaBuf[dnaBufLen++]=pol; return; }
  for(uint8_t i=1;i<DNA_W;i++) dnaBuf[i-1]=dnaBuf[i];
  dnaBuf[DNA_W-1]=pol;
}

// Unique window match, searched only near navMm and only in the declared
// direction. Returns the mm of the LAST marker in the window, or 255 if not
// unique within the window.
static uint8_t dnaMatch(int8_t dir){
  if(dnaBufLen<DNA_W || dir==MAP_UNSET) return 255;
  uint8_t found=255, count=0;
  for(int8_t off=-REACQ_WINDOW_MARKERS; off<=REACQ_WINDOW_MARKERS; off++){
    uint8_t end   = routeMod((int32_t)navMm + off);
    uint8_t start = routeMod((int32_t)end - (int32_t)dir*(DNA_W-1));
    bool ok=true; uint8_t mm=start;
    for(uint8_t i=0;i<DNA_W;i++){
      if(dnaAt(mm)!=dnaBuf[i]){ ok=false; break; }
      mm=nextMm(mm,dir);
    }
    if(ok){ if(++count>1) return 255; found=end; }
  }
  return count==1 ? found : 255;
}

// HARD_BOUND advisory (decision 0023). Loads the evidence ring — oldest first,
// which is the order dnaMatch() walks — and asks for an exact unique window.
// Returns the marker, or ADVISORY_NONE when there is nothing exact and unique
// to say. Pure: it writes only the scratch buffer, and the caller publishes the
// result without acting on it.
//
// evRingLen < DNA_W means fewer than twelve readings survive, so the question
// cannot be asked. That is silence, not an error: HARD_BOUND fires at
// evalCount 12 but the ring is cleared on adoption, so a late adoption inside
// the same incident can leave it short.
static uint8_t quorumAdvisoryMarker(){
  if(evRingLen<DNA_W) return ADVISORY_NONE;
  for(uint8_t i=0;i<DNA_W;i++) dnaBuf[i]=ringAt(i)->polarity;
  dnaBufLen=DNA_W;
  return dnaMatch(navDir);
}

// ---------------------------------------------------------------------------
// §2.5Q ROUTE-WIDE unique window match. dnaMatch() is deliberately fenced to
// ±REACQ_WINDOW_MARKERS — correct while the position is merely doubted, and
// useless once it is formally discredited: the 2026-08-14 incident's true
// offset was 32 markers, and constraining a search to a fence around a belief
// we have declared untrustworthy protects nothing. This scans every end
// position on the route in the session direction and returns the ending
// marker ONLY when exactly one window matches. Cost ~DNA_N*DNA_W ≈ 2k byte
// compares per call, loop-thread, negligible.
static uint8_t dnaMatchWide(int8_t dir){
  if(evRingLen<DNA_W || dir==MAP_UNSET) return 255;
  for(uint8_t i=0;i<DNA_W;i++) dnaBuf[i]=ringAt(i)->polarity;
  uint8_t found=255, count=0;
  for(int16_t end=0; end<DNA_N; end++){
    uint8_t start = routeMod((int32_t)end - (int32_t)dir*(DNA_W-1));
    bool ok=true; uint8_t mm=start;
    for(uint8_t i=0;i<DNA_W;i++){
      if(dnaAt(mm)!=dnaBuf[i]){ ok=false; break; }
      mm=nextMm(mm,dir);
    }
    if(ok){ found=(uint8_t)end; if(++count>1) return 255; }
  }
  return (count==1)?found:255;
}

// §2.5Q learning pass — runs on every accepted event while NAV_NO_QUORUM.
static void nqLearn(const MarkerEvent& e){
  (void)e;
  if(nqFreshEvents<65535) nqFreshEvents++;
  // The ring must hold only post-failure evidence before it is trusted: the
  // twelve readings that CAUSED the collapse must have aged out.
  if(nqFreshEvents < DNA_W) return;
  uint8_t m = dnaMatchWide(navDir);
  if(m==255){ nqCandidate=255; nqConfirm=0; return; }
  // Consistency: each new event advances the window by one, so the unique
  // match must advance by exactly one with it.
  // CODEX finding 6 flagged a words-vs-code mismatch: 0035 said "3 further
  // confirmations" while this counts three consistent matches TOTAL (the
  // candidate plus two further advances). The CODE stands and the RECORD's
  // wording was corrected, on capture evidence: each match here is a
  // route-wide-unique 12-window (suite-proven map fact) re-verified under
  // coherent advance, and on the 2026-08-10 capture the stricter reading
  // would have pushed the only real-data recovery (fresh=17, the session's
  // final event) one marker past the end of the record — same protection,
  // measurable cost.
  if(nqCandidate!=255 && m==nextMm(nqCandidate,navDir)) nqConfirm++;
  else                                                  nqConfirm=1;
  nqCandidate=m;
  if(nqConfirm < NQ_CONFIRM_N) return;

  // Decisive and confirmed: relabel. Knowledge only — AUTO stays dropped.
  uint8_t oldMm=navMm;
  int16_t delta=(int16_t)m-(int16_t)navMm;
  navMm=m;
  for(uint8_t i=0;i<evRingLen;i++){
    RingEntry* r=ringAt(i);
    r->navMm=routeMod((int32_t)r->navMm + delta);
  }
  navState=NAV_NORMAL;
  // CODEX 1.16 review, finding 1 — THE RESUME INTERLOCK. enterNoQuorum()
  // never drops autoRunning (it stops via a zero request only), so restoring
  // NAV_NORMAL here would let serviceStations()'s idle branch re-request
  // cruise on the next loop pass and RESTART A LOST-AND-FOUND LOCOMOTIVE
  // without an operator. "Knowledge recovery is not motion recovery" must be
  // enforced, not promised: AUTO is dropped here, explicitly, and resuming
  // requires a deliberate GO. (The harness could not catch this — it never
  // calls serviceStations(); the reviewer read the loop instead.)
  nqDropAutoInterlock();
  irObserversReset("SELF_RESOLVED"); // IR TEST A §7: relabel breaks anchors
  fullRecoveryReset();
  lostMarkers=0;
  updateLastConfirmed(navMm, e.detectedAtMs);
  uint16_t fresh=nqFreshEvents;                 // capture BEFORE the reset
  nqFreshEvents=0; nqCandidate=255; nqConfirm=0;
  // §2.5 CLEAR arm, exactly as navDeclare(): the retained terminal snapshot
  // must not linger as a ghost once the navigator has found itself.
  portENTER_CRITICAL(&noQuorumMux);
  desiredRetainedNoQuorum=DRS_CLEAR;
  noQuorumGeneration++;
  noQuorumNeedsReconcile=true;
  portEXIT_CRITICAL(&noQuorumMux);
  // ONE publish. The decision event carries the state ("NORMAL") like every
  // other QUORUM decision; a second navPublishState with the same event name
  // double-published the recovery and read as two resolutions in the log.
  char extra[96];
  snprintf(extra,sizeof(extra),",\"old_mm\":%u,\"new_mm\":%u,\"fresh\":%u,\"confirms\":%u",
           oldMm,navMm,(unsigned)fresh,(unsigned)NQ_CONFIRM_N);
  publishQuorumDecision("SELF_RESOLVED",extra);
}

// ---------------------------------------------------------------------------
// §3Q helpers.
static uint16_t qMedian(const uint16_t* h, uint8_t n){
  uint16_t c[Q_HIST_N];
  for(uint8_t i=0;i<n;i++) c[i]=h[i];
  for(uint8_t i=1;i<n;i++){ uint16_t k=c[i]; int8_t j=i-1;
    while(j>=0 && c[j]>k){ c[j+1]=c[j]; j--; } c[j+1]=k; }
  return c[n/2];
}

static void qNoteAccepted(const MarkerEvent& e, uint16_t dt){
  if(dt==0) return;                       // session bootstraps train nothing
  qDtHist[qHistIdx]=dt;
  qPkHist[qHistIdx]=(uint16_t)e.peak;
  qDuHist[qHistIdx]=(uint16_t)e.durationMs;
  qHistIdx=(qHistIdx+1)%Q_HIST_N;
  if(qHistLen<Q_HIST_N) qHistLen++;
}

// Should this event be HELD rather than committed?
static bool qSuspect(const MarkerEvent& e, uint16_t dt){
  if(dt==0) return false;                 // no interval, no verdict
  // Decisive: the physical floor. Unconditional — no regime, no model, no
  // reference that could itself be corrupt. 350 ms over 280 mm demands
  // 800 mm/s — 1.81x the fleet's demonstrated maximum. (The 2x safety
  // factor was applied deriving 317 ms; rounding UP to 350 lands the
  // enforced margin at 1.81x. CODEX finding 7: say the real number.)
  if(dt < Q_FLOOR_MS) return true;
  // Corroborating conjunction, only inside the band it was calibrated in.
  if(qHistLen < 8) return false;
  uint16_t mdt=qMedian(qDtHist,qHistLen);
  if(mdt < Q_STEADY_DT_MIN || mdt > Q_STEADY_DT_MAX) return false;
  // CODEX finding 5: no PWM condition here. PWM is a request, not a
  // measurement (decision 0024's whole point), and gating scrutiny on
  // pwm>=40 let a reported 39 exempt an event from examination. The trailing
  // median interval band IS the operating-regime test, measured rather than
  // asked for.
  if(!((float)dt < Q_INT_RATIO*(float)mdt)) return false;
  const RingEntry* last = evRingLen ? ringAt(evRingLen-1) : nullptr;
  bool opp = last && (last->polarity != e.polarity);
  if(!opp) return false;
  uint16_t mpk=qMedian(qPkHist,qHistLen), mdu=qMedian(qDuHist,qHistLen);
  // The dim test asks whether this read is weak FOR THE MAGNET IT SHOULD BE,
  // not weak for the railway. mpk is the trailing median across all markers,
  // i.e. this locomotive's current GAIN; strengthAt() is the magnet's expected
  // SHAPE. Multiply them for the expectation at the next marker.
  //
  // Guarded on position: with no usable position there is no "next marker",
  // and an expectation drawn from a wrong mm would be worse than none. Then
  // the flat median stands and behaviour is exactly as it was.
  uint16_t expPk = mpk;
  if(navPositionUsable() && navDir!=MAP_UNSET){
    uint8_t nextMm = routeMod((int32_t)navMm + navDir);
    expPk = (uint16_t)(((uint32_t)mpk * strengthAt(nextMm)) / 100u);
  }
  bool dim  = (float)e.peak       < Q_FLUX_RATIO*(float)expPk;
  bool lng  = (float)e.durationMs > Q_DUR_RATIO *(float)mdu;
  return dim || lng;
}

static void qPublish(const char* verdict, const MarkerEvent& e, uint16_t dt,
                     const char* why){
  char extra[160];
  snprintf(extra,sizeof(extra),
    ",\"pol\":\"%c\",\"peak\":%d,\"dur\":%u,\"dt\":%u,\"why\":\"%s\","
    "\"held\":%lu,\"discarded\":%lu,\"committed\":%lu",
    polChar(e.polarity),e.peak,(unsigned)e.durationMs,(unsigned)dt,why,
    (unsigned long)qQuarantined,(unsigned long)qDiscarded,(unsigned long)qCommitted);
  publishQuorumDecision(verdict,extra);
}

// ===========================================================================
// §3 TIMING GATE + navOnMarker — an event must earn its advance.
// Runs on the LOOP thread (via drainMarkers). No new tasks.
//
// Two levels of acceptance, not the same thing (§3):
//   received            — the detector event is real and is published by
//                         drainMarkers(), exactly once. Nothing else.
//   navigation-accepted — it also advances navMm, enters the evidence ring
//                         and participates in comparison or scoring.
// NO_DIR events are received only — the sole case where the distinction bites.
//
// Two timestamp streams, never merged: lastMarkerMs/dt advance on EVERY
// received event including NO_DIR and PHANTOM_REJECTED (run 3: the 57 ms
// interval is measured from the phantom to the real magnet — skip the
// phantom and the pair never sums to one interval). previousAcceptedDt
// advances only per the gate rules. Do not make dt accepted-to-accepted.
// ===========================================================================
// 0.3B — ONE AUTHORITATIVE POSITION OUTCOME (CODEX requirements, 2026-08-27).
// navOnMarker()/navLadder() return the event's final disposition so the R3
// identity layer can distinguish "my proposal committed" from "the inherited
// machinery refused it". Root cause of the 82-marker drift: R3 mutated navMm
// before the phantom detector ruled, never learned the ruling, and resynced
// its shadow to the wreckage. navMm is now written in exactly ONE place —
// acceptEvent() — and an R3 correction rides the event through the ladder as
// a PROPOSAL, committing atomically with acceptance or not at all.
// (NavDisposition is declared with the early types — prototype-generator trap.)
static const char* navDispName(uint8_t d){
  switch(d){
    case NAV_D_ACCEPTED:         return "ACCEPTED";
    case NAV_D_QUARANTINED:      return "QUARANTINED";
    case NAV_D_PHANTOM_REJECTED: return "PHANTOM_REJECTED";
    case NAV_D_NO_POSITION:      return "NO_POSITION";
    case NAV_D_NO_DIR:           return "NO_DIR";
    default:                     return "NOT_PRESENTED";
  }
}
static NavDisposition navLadder(const MarkerEvent& e, uint16_t dt, bool vouched,
                                uint8_t corrOff);

static NavDisposition navOnMarker(const MarkerEvent& e, uint8_t r3CorrOff){
  navMarkers++;
  // Timed from DETECTION, not from when loop() got round to draining the
  // queue. Diagnostic only -- no PWM authority. (Codex)
  uint16_t dt = lastMarkerMs ? (uint16_t)min(e.detectedAtMs-lastMarkerMs,(unsigned long)65535) : 0;
  lastSegmentDt = dt;
  lastMarkerMs  = e.detectedAtMs;

  // §5: when timing values are unavailable they publish as 0 / -1.0, never
  // omitted. Overwritten in the ladder on an ACTIVE evaluation.
  lastDtExpected      = 0;
  lastDtConserveRatio = -1.0f;

  if(navState==NAV_UNSET){
    lastTimingGate="NO_POSITION";
    return NAV_D_NO_POSITION;
  }
  if(navDir==MAP_UNSET){
    lastTimingGate="NO_DIR";
    invalidatePreviousAcceptedDt();
    return NAV_D_NO_DIR;
  }

  // ---- §3Q ARBITRATION: a pending event is judged by its successor --------
  if(qPendingValid){
    // CODEX 1.16 review, finding 4: the witness is credential-checked BEFORE
    // it may testify. Judged on the RAW pending->e interval: under H-genuine
    // that IS e's own interval, so failing on it disqualifies genuine
    // testimony outright; under H-phantom e's true interval is the folded
    // one, and e is re-judged on exactly that below, like any other arrival.
    // An unfit witness leaves only the primary verdict for the pending
    // (operator 2026-08-14: the record comes from the next reading with GOOD
    // credentials) — and this is how a slow consecutive phantom family folds
    // up: each unfit successor discards its elder and is held in turn, until
    // a genuine event finally testifies.
    bool witnessFit = !qSuspect(e,dt);
    // Which marker does THIS event's polarity fit?
    //   H-phantom : pending was spurious -> e is the marker after navMm.
    //   H-genuine : pending was real (at navMm+1) -> e is at navMm+2.
    // 0.3B: a held event carries its R3 correction proposal (qPendingCorrOff),
    // so H-genuine means the pending was real at navMm+1+corrOff and e is one
    // past THAT. H-phantom leaves e as the plain next marker — the proposal
    // died with the phantom.
    uint8_t mmP = nextMm(navMm,navDir);
    uint8_t mmG = routeMod((int32_t)navMm + (int32_t)(2+qPendingCorrOff)*(int32_t)navDir);
    bool matchP = (dnaAt(mmP)==e.polarity);
    bool matchG = (dnaAt(mmG)==e.polarity);
    qPendingValid=false;
    if(witnessFit && matchG && !matchP){
      // The successor vouches for the pending event: commit it first, with
      // its own interval — then fall through and judge e normally (its dt
      // already measures pending -> e). vouched=true: the map itself has
      // authenticated this event through its successor's polarity, and the
      // PWM velocity model does not get to overrule the map (CODEX finding
      // 2: at some spacings the legacy conservation gate would re-reject
      // the very event the arbitration just proved genuine — commit first,
      // then killed on the model 0024 already convicted; whether it fired
      // depended on WHICH spacingMm[] entry the event landed on).
      qCommitted++;
      qPublish("QUARANTINE_COMMITTED",qPending,qPendingDt,"SUCCESSOR_FITS_GENUINE");
      lastSegmentDt=qPendingDt;
      navLadder(qPending,qPendingDt,true,qPendingCorrOff);
      irObsFinalize(qPending,"QUARANTINE_COMMITTED");  // IR TEST A: revision-2
                                                       // record; void (§8.3)
      lastSegmentDt=dt;
    }else{
      // Primary verdict (operator 2026-08-14): phantom. Covers an unfit
      // witness, successor-fits-phantom, both-fit (ambiguous) and
      // neither-fits (e may itself be suspect; it is credential-checked
      // below on the folded interval). The discarded interval folds into
      // e's dt so the committed timing chain still measures physical travel
      // — without this the conservation gate would judge a half interval.
      // A wrong verdict here means a genuine advance went unrecorded: the
      // odometer runs BEHIND and the true position is one marker AHEAD, so
      // the cost is offset +1 in the direction of travel (CODEX finding 7
      // corrected the sign) — inside the fence, adopted routinely. That
      // asymmetry is the design.
      qDiscarded++;
      irObsFinalize(qPending,"QUARANTINE_DISCARDED");  // IR TEST A: revision-2
      qPublish("QUARANTINE_DISCARDED",qPending,qPendingDt,
               !witnessFit      ? "SUCCESSOR_SUSPECT"
             : matchP&&matchG   ? "AMBIGUOUS_DEFAULT_PHANTOM"
             : matchP           ? "SUCCESSOR_FITS_PHANTOM"
                                : "NEITHER_FITS");
      uint32_t merged=(uint32_t)dt+(uint32_t)qPendingDt;
      dt = (merged>65535UL)?65535U:(uint16_t)merged;
      lastSegmentDt=dt;
    }
  }

  // ---- §3Q CREDENTIALS: hold a doubtful event for its successor -----------
  if(qSuspect(e,dt)){
    qPending=e; qPendingDt=dt; qPendingValid=true;
    qPendingCorrOff=r3CorrOff;   // 0.3B: the proposal is held WITH the event
    qQuarantined++;
    lastTimingGate="QUARANTINED";
    qPublish("QUARANTINED",e,dt,(dt<Q_FLOOR_MS)?"BELOW_PHYSICAL_FLOOR":"CONJUNCTION");
    return NAV_D_QUARANTINED;
  }

  return navLadder(e,dt,false,r3CorrOff);
}

// The original §3 gate ladder, unchanged except for taking dt as a parameter:
//   LOW_PWM -> RAMP -> NO_PREV -> ACTIVE
static NavDisposition navLadder(const MarkerEvent& e, uint16_t dt, bool vouched,
                                uint8_t corrOff){
  if(navState==NAV_UNSET){
    // Direction may be known; position is not. No advance, no ring, no
    // last-confirmed, no conservation — navMm holds nothing meaningful to
    // index spacingMm[] with — and no timing history to invalidate.
    lastTimingGate="NO_POSITION";
    return NAV_D_NO_POSITION;
  }
  if(navDir==MAP_UNSET){
    // Received, NOT accepted: there is no direction to advance in.
    // nextMm(mm,MAP_UNSET) returns mm unchanged — it would silently push a
    // duplicate ring entry, corruption rather than a crash. Defensive, not a
    // reachable operational path: both declaration handlers refuse to declare
    // without a direction. Do not reorder them to make this reachable.
    lastTimingGate="NO_DIR";
    invalidatePreviousAcceptedDt();
    return NAV_D_NO_DIR;
  }
  if(e.pwmActualAtDetect < GATE_LOW_PWM_FLOOR){
    // Below the velocity model's validity: accept the advance, and the
    // interval must not seed a later conservation test.
    lastTimingGate="LOW_PWM";
    invalidatePreviousAcceptedDt();
    acceptEvent(e,corrOff);
    return NAV_D_ACCEPTED;
  }
  if(abs((int)e.pwmActualAtDetect-(int)e.pwmCommandedAtDetect) > GATE_RAMP_DELTA){
    // Mid-ramp — both values from the SAME instant, event open (§3). The
    // event-time pair is what makes this a measurement: a stop request
    // between detection and drain cannot turn a steady 100/100 into "RAMP".
    lastTimingGate="RAMP";
    invalidatePreviousAcceptedDt();
    acceptEvent(e,corrOff);
    return NAV_D_ACCEPTED;
  }
  if(!previousAcceptedDtValid){
    // NO_PREV is a BOOTSTRAP, not a suspension — it ESTABLISHES the
    // predecessor rather than invalidating it (getting this backwards
    // livelocks the gate and conservation never runs). A zero dt — the first
    // event of a session — must never become a predecessor: the next genuine
    // marker would test 0+dt against one expected interval and be rejected.
    lastTimingGate="NO_PREV";
#ifdef Q_TIMING_MEASURED
    // A dwell-spanning first interval must never become the predecessor: with
    // expectedDt = prev, a 40 s seed makes every genuine 1-3 s marker satisfy
    // dt <= 0.30*prev and the rejection freeze preserves the poison (measured:
    // runs of 43 consecutive genuine markers rejected). Same doctrine as the
    // zero-dt rule above, at the other extreme.
    if(dt>0 && dt<=Q_PREV_SANE_MS){ previousAcceptedDt=dt; previousAcceptedDtValid=true; }
#else
    if(dt>0){ previousAcceptedDt=dt; previousAcceptedDtValid=true; }
#endif
    acceptEvent(e,corrOff);
    return NAV_D_ACCEPTED;
  }

  // ACTIVE — the conservation test, the whole rule (§3): did the previous
  // accepted event and this one divide ONE physical interval into two pieces?
  lastTimingGate="ACTIVE";
  // The interval that ENDED at navMm, not the one leaving it: navMm is the
  // marker the PREVIOUS accepted event reached, so the interval under test is
  // the one behind it. CW at MM50: MM49->MM50 -> spacingMm[49]. CCW at MM50:
  // MM51->MM50 -> spacingMm[50]. Off-by-one here is up to 31% — comparable to
  // the whole tolerance.
  uint8_t conserveIntervalIndex = (navDir==MAP_CW) ? routeMod((int32_t)navMm-1) : navMm;
#ifdef Q_TIMING_MEASURED
  // HARNESS-ONLY variant (decision 0024 / QUORUM_TIMING_EXPECTATION_PROPOSAL
  // §3, CODEX boundary: "implement IN THE HARNESS ONLY. Not approved for
  // firmware"). The expectation is the interval the locomotive just MEASURED,
  // not the one a duty model predicts. The whole test then reduces to
  // "reject an event arriving within 30% of the previous accepted interval",
  // and the frozen-predecessor trap cannot form: a poisoned short predecessor
  // SHRINKS the threshold and self-heals on the next acceptance. This block
  // must never be compiled into a locomotive image without a decision record
  // lifting the 0024 boundary; no build flag in firmware/ passes it.
  // REVISED after the 2026-08-21 shadow replay: the proposal's literal form
  // (expectedDt = previousAcceptedDt) FAILED out-of-sample. A dwell-spanning
  // interval (40,441 ms observed; 65,535 saturated) becomes the predecessor,
  // every genuine 1-3 s marker then satisfies dt <= 0.30*prev, and the
  // rejection freeze preserves the poison: runs of 43/42/28 consecutive
  // genuine markers rejected on three of five replayed sessions. The trailing
  // MEDIAN of accepted intervals is immune - one dwell cannot move an
  // 11-sample median. Cold start (fewer than 8 accepted intervals) falls back
  // to the PWM model, i.e. exactly the BASE behaviour.
  // SECOND REVISION, same day: the median form ALSO failed the replay - a
  // median is robust and therefore STALE, so after every speed change two
  // new-regime intervals sum to one old-regime median and land in the kill
  // band (rejection runs to 136; one session took 2,514 rejections). The
  // expectation must be REACTIVE: the interval just measured. The actual
  // defect in the proposal's literal form was never the ratio rule - it was
  // letting a dwell-spanning interval become the predecessor. Fixed at the
  // update sites below with Q_PREV_SANE_MS, the same doctrine as the existing
  // zero-dt seeding rule.
  (void)conserveIntervalIndex;
  float expectedDtMs = (float)previousAcceptedDt;
#else
  float velocityMmPerSec = VEL_MODEL_SLOPE*(float)e.pwmActualAtDetect + VEL_MODEL_INTERCEPT;
  // The 1000 is not optional: the model is mm/s, dt is MILLISECONDS. The unit
  // travels with the name (§3), though the payload field stays dt_expected.
  float expectedDtMs = (1000.0f*(float)pgm_read_word(&spacingMm[conserveIntervalIndex]))
                       / velocityMmPerSec;
#endif
  float combinedDtMs = (float)dt + (float)previousAcceptedDt;
  lastDtExpected      = (uint32_t)expectedDtMs;
  lastDtConserveRatio = (expectedDtMs>0.0f) ? (combinedDtMs/expectedDtMs) : -1.0f;
  // fabsf, not abs: the Arduino abs() macro truncates floats (§3).
  float errorMs = fabsf(combinedDtMs-expectedDtMs);
#ifdef Q_TIMING_MEASURED
  if(qLastRejectedDt &&
     fabsf((float)dt-(float)qLastRejectedDt) <= DT_CONSERVE_TOL*(float)qLastRejectedDt){
    qLastRejectedDt=0;
    if(dt<=Q_PREV_SANE_MS){ previousAcceptedDt=dt; previousAcceptedDtValid=true; }
    acceptEvent(e,corrOff);
    return NAV_D_ACCEPTED;
  }
#endif
  if(!vouched && errorMs <= DT_CONSERVE_TOL*expectedDtMs){
    // vouched events pass here untested (finding 2): the telemetry above is
    // still computed and published, but the model gets no veto over an event
    // the map has already authenticated. The unvouched path is unchanged.
    // Two events inside one interval's worth of travel: one magnet, two
    // events. Do NOT advance navMm, push the ring, touch missStreak — and do
    // NOT replace previousAcceptedDt: a rejected event must never become the
    // timing predecessor (914 -> 57 -> 879: if 57 replaced it, the real 879
    // would be rejected too and the odometer would stop advancing). No
    // attempt to decide which of the two was spurious — the first was
    // consumed; declining to advance on the second leaves the count correct.
    char extra[128];
    snprintf(extra,sizeof(extra),
      ",\"dt\":%u,\"prev_dt\":%u,\"sum\":%.0f,\"expected\":%.0f,\"ratio\":%.2f",
      dt,previousAcceptedDt,(double)combinedDtMs,(double)expectedDtMs,(double)lastDtConserveRatio);
    publishQuorumDecision("PHANTOM_REJECTED",extra);
#ifdef Q_TIMING_MEASURED
    qLastRejectedDt=dt;
#endif
    return NAV_D_PHANTOM_REJECTED;
  }
  // Every accepted ACTIVE event replaces the predecessor — the variable means
  // "the interval of the LAST ACCEPTED event", so it advances with each
  // acceptance; only PHANTOM_REJECTED leaves it unchanged (§3).
#ifdef Q_TIMING_MEASURED
  qLastRejectedDt=0;
  if(dt<=Q_PREV_SANE_MS){ previousAcceptedDt=dt; previousAcceptedDtValid=true; }
  else invalidatePreviousAcceptedDt();
#else
  previousAcceptedDt=dt; previousAcceptedDtValid=true;
#endif
  acceptEvent(e,corrOff);
  return NAV_D_ACCEPTED;
}

// ===========================================================================
// LAYER 4 — OPERATIONS
// ---------------------------------------------------------------------------
// Prescribed PWM by location. Nothing here calculates speed from marker
// timing; Hall dt is diagnostic only and has no motor authority.
//
// EVERY STATE HAS AN EXIT. The 2026-07-26 run wedged because a station armed
// for Patio, the position jumped past the centre, and the machine sat in
// STOP_SERVICE forever — silently skipping every station for the rest of the
// session. Two changes prevent it:
//   * arming triggers on a RANGE, not on exact equality with one marker;
//   * overshooting the centre publishes MISSED and returns to IDLE.
// ===========================================================================
// ---------------------------------------------------------------------------
// RAMPS ARE DURATIONS, NOT STEP RATES
//
// 2_10 specified ramps in milliseconds per PWM count, so how long a ramp took
// depended on where it started. The zero ramp at 300 ms/step from PWM 61 took
// EIGHTEEN SECONDS -- fourteen marker intervals at the measured 1.26 s spacing.
// The locomotive could not stop at any of the four stations; the overshoot
// escape fired at +6 every time, with PWM still at 47-69.
//
// Measured on the 2026-07-27 lap: marker interval 1.26 s median, 243 mm/s.
// Ramps are now given a DURATION and the step rate is derived, so a ramp takes
// the same wall time from any starting speed.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SECTION CRUISE SPEED
//
// Some stretches must be driven at a different speed than the open main. This
// is a property of the LAYOUT, not of any station, so it lives in its own
// table: a section carries its OWN cruise speed, which REPLACES CRUISE_PWM
// while the locomotive is inside it.
//
// This is a whole target, not an additive boost. The previous boost mechanism
// had two faults Codex flagged: it wrote commandedPwm outside requestPwm(),
// creating a second PWM authority, and because it changed mid-ramp it broke the
// guarantee that a ramp is a fixed duration. A section speed is just another
// target requested through the normal path, so neither can recur —
// requestPwm()/requestPwmOver() stay the only writers of commandedPwm, and a
// ramp always runs to completion.
//
// MM065-MM080 CW is the climb out of Grillers. Toby stalled there on
// 2026-07-27 with three coaches at the common cruise of 100. Clockwise only:
// counter-clockwise the same stretch is a descent, where more speed would be
// exactly wrong.
//
// AUTO only. In manual the operator has the throttle and nothing modifies it;
// every request path that consults cruiseForPosition() runs under AUTO.
// ---------------------------------------------------------------------------
struct GradeSegment {
  uint8_t fromMm, toMm;   // inclusive, in the direction given
  int8_t  dir;            // MAP_CW or MAP_CCW
  uint8_t cruisePwm;      // cruise speed WITHIN this section
};
static const GradeSegment GRADES[] = {
  { 65, 80, MAP_CW, 120 }   // climb out of Grillers: stalled at 100 with three coaches
};
static const uint8_t GRADE_COUNT = sizeof(GRADES)/sizeof(GRADES[0]);

static const uint16_t APPROACH_RAMP_MS = 700;   // ~half a marker: settled before the next
static const uint16_t FINAL_RAMP_MS    = 700;
// v1.12B (operator ruling 2026-08-09): "Passenger stops must be gentle."
// Station stops and departures move from DURATION-based ramps (reach the
// target in N ms — which made the step rate depend on the PWM span, and
// left departures 2.4x steeper than a manual crawl) to PER-STEP pacing,
// the same mechanism as manual/BEGIN/PAUSE. One PWM count per:
static const uint16_t STATION_UP_STEP_MS   = 150;  // departure: matches the gentlest ramp on the railway
static const uint16_t STATION_DOWN_STEP_MS = 200;  // final stop: gentler still
// (Approach/zone adjustments between nonzero speeds keep their short
// duration ramps — they are speed trims, not passenger jolts.)
static const int8_t   APPROACH_START      = -10;
static const int8_t   ZONE_START          = -5;
// (superseded by per-station stopOffset in STATIONS[])
static const int8_t   OVERSHOOT_ABANDON   = 5;   // markers past centre -> MISSED
static const uint32_t DWELL_MS            = 15000UL;
// If M+1 is crossed and M+2 does not arrive within this, the zero ramp starts
// anyway. M+2 stays the PRIMARY trigger, so accurate stops stay accurate; this
// only covers the case where the loco runs out of tractive effort short of it,
// which is what left it standing past Grillers on 2026-07-26. Expected M+1 to
// M+2 interval at final approach is 1-2 s, so 5 s will not fire on a merely
// slow crossing.
static const uint32_t FINAL_M1_TIMEOUT_MS = 5000UL;

// Backstop for ST_APPROACH only. ST_FINAL now has the fallback above, ST_RAMP
// always completes because the ramp is monotonic, and ST_DEPART is exempt --
// a loco accelerating from a stop on clear track has nothing to be rescued
// from, and a timeout there protected against nothing.
static const uint32_t STATION_MAX_PHASE_MS= 120000UL;

// Approach targets are DERIVED from cruise and station speed, not written out.
// Hard-coded values silently stop matching the moment CRUISE_PWM changes; with
// the derivation the ramp always begins at cruise and lands exactly on station
// speed at the zone boundary, whatever those two numbers are.
//
//   step = (CRUISE - ZONE) / (ZONE_START - APPROACH_START)
//   at 100/60 over offsets -10..-6:  92, 84, 76, 68, 60
static int approachTargetForOffset(int16_t o,uint8_t zonePwm){
  // CE: derive from the MISSION's cruise, not the bare constant. The comment
  // above warns that hard-coded values stop matching the moment CRUISE_PWM
  // changes; a mission changes it just as surely, and a LOCAL approaching from
  // 75 must not be handed a ramp built for 90.
  const int cruise = ceCruisePwm();
  if(o <  APPROACH_START) return cruise;
  if(o >= ZONE_START)     return zonePwm;
  const int steps = (int)ZONE_START - (int)APPROACH_START;      // 5
  const int idx   = (int)(o - APPROACH_START) + 1;              // 1..5
  return cruise - (cruise - (int)zonePwm)*idx/steps;
}

// M and M+1 only. The 2026-07-26 run showed PWM 25 is below the tractive
// floor on at least one approach — the loco reached 25 a marker early and
// stopped short of its own zero-ramp trigger, wedging the station machine.
// The zero ramp now begins from 42 at M+2 instead of stepping through 25.
// (superseded by per-station zonePwm / finalPwm in STATIONS[])

static int8_t        stIndex=-1;
static unsigned long stPhaseEnteredMs=0;
static unsigned long stDwellStartedMs=0;
static unsigned long stMPlus1AtMs=0;      // when M+1 was crossed, 0 if not yet
static unsigned long stDepartBeganMs=0;   // for the slow-departure notice only
static bool          stDepartWarned=false;

// PWM authority: commandedPwm/actualPwm are declared volatile beside the
// detector (§3 — event-open capture). Targets normally enter via requestPwm();
// the deliberate exceptions are servicePwmRamp() (the actuator), setup(), and
// E-stop.
static uint16_t pwmStepMs=NORMAL_STEP_MS;
static unsigned long lastPwmStepMs=0;


static void stationPublish(const char* ev,int16_t off,const char* note);

// True if the motor is turning OR about to be. Codex found that testing
// actualPwm alone raced the ramp: during acceleration commandedPwm can already
// be high while actualPwm is still under the deadzone, so a direction command
// was accepted a moment before real power arrived in the new direction.
// Any energisation at all, not merely enough to turn the wheels. Codex noted
// the deadzone version did not deliver what the comment claimed; a stopped
// loco has both values at zero anyway, so the stricter test costs nothing and
// makes "no direction-pin write while PWM is applied" literally true.
static bool motorIsMoving(){
  return actualPwm>0 || commandedPwm>0;
}

// The single place direction is applied. Recomputes navDir from sessionDir and
// motorDirection, discards direction-dependent reacquisition evidence if it
// changed, and writes the pin.
static void applyDirection(){
  // NEUTRAL is an interlock, not a third travel direction. It prevents motion
  // (see the clamp in servicePwmRamp) but leaves the navigator's direction
  // alone -- a stationary loco has not changed which way it faces on the map,
  // and unsetting navDir here would reintroduce the frozen-odometer hole.
  if(motorDirection!=DIRECTION_NEUTRAL){
    int8_t derived = (sessionDir==MAP_UNSET) ? MAP_UNSET
                   : (motorDirection==DIRECTION_FORWARD ? sessionDir : oppositeDir(sessionDir));
    if(derived!=navDir){
      irObserversReset("DIRECTION"); // IR TEST A §7: anchors die with the frame
      int8_t prevDir = navDir;
      navDir=derived;
      // §3Q: a direction change while an event is held makes the arbitration
      // geometry meaningless (the "next marker" changed identity). Discard
      // the pending event rather than judging it in a frame that no longer
      // exists — CODEX's ordering-fault exception, made explicit.
      if(qPendingValid){
        qPendingValid=false; qDiscarded++;
        qPublish("QUARANTINE_DISCARDED",qPending,qPendingDt,"DIRECTION_CHANGED");
      }
      // F3 (CODEX review of 1.4) — REVERSING MID-INTERVAL. navMm is the marker
      // last REACHED, and the locomotive is always somewhere between it and
      // the next one along. Reverse, and the next marker it physically meets
      // is the one it just left — navMm itself. But every accepted event
      // advances navMm by one (§1, certified), so without this the first
      // marker after a reversal is compared against dnaAt(navMm - 1): wrong
      // by one, by construction, on every reversal.
      //
      // Step the odometer back along the OLD direction so the standard
      // advance lands on navMm again. Self-consistent under a double
      // reversal, which returns it exactly where it started. Only when both
      // directions are real and a position exists — an UNSET->CW transition
      // is initial setup, not a reversal, and has no interval to be inside.
      if(prevDir!=MAP_UNSET && derived!=MAP_UNSET && navState!=NAV_UNSET)
        navMm = routeMod((int32_t)navMm + prevDir);
      // §6.3: full recovery reset — readings collected in one direction cannot
      // be scored against candidates in another, and neither can an exclusion
      // or a failure count. Evaluation is abandoned, not continued.
      fullRecoveryReset();
      if(navState==NAV_EVALUATING) navState=NAV_NORMAL;
      // A direction change in NAV_NO_QUORUM resets the diagnostics but does
      // NOT leave the terminal state (§2) — changing direction is not
      // evidence about where the locomotive is.
    }
    digitalWrite(MOTOR_DIR_PIN,(motorDirection==DIRECTION_FORWARD)?HIGH:LOW);
  }
}

// The cruise speed for the current position: a section's own cruise while the
// locomotive is inside one, otherwise the open-main CRUISE_PWM. The membership
// arithmetic is the same modular test the old grade boost used, which Codex
// verified correct in both directions.
static int cruiseForPosition(){
  // §0.1: position is held (and probably correct) during EVALUATING, so the
  // section map keeps working; the navState test is gone, navDir stays.
  if(navDir!=MAP_UNSET){
    for(uint8_t i=0;i<GRADE_COUNT;i++){
      const GradeSegment& g=GRADES[i];
      if(g.dir!=navDir) continue;
      // Distance in markers from the segment start, travelling in g.dir.
      int32_t into = (g.dir==MAP_CW) ? (int32_t)routeMod((int32_t)navMm-g.fromMm)
                                     : (int32_t)routeMod((int32_t)g.fromMm-navMm);
      int32_t span = (g.dir==MAP_CW) ? (int32_t)routeMod((int32_t)g.toMm-g.fromMm)
                                     : (int32_t)routeMod((int32_t)g.fromMm-g.toMm);
      if(into<0 || into>span) continue;               // not inside this segment
      return (int)g.cruisePwm;
    }
  }
  return ceCruisePwm();   // CE: mission cruise on the open main; grades above keep theirs
}

// LAYER 5 (CODEX 1.14 review, finding 1): the CTO limiter lives HERE, inside
// the only two writers of commandedPwm, so no AUTO request path can restore
// speed past an active traffic cap, fleet stop or role conflict — the
// original five call-site wrappers missed ST_APPROACH/ST_FINAL, which
// re-request zone/final speed every pass and were overwriting a CTO stop
// within one loop. AUTO chamber only: MANUAL authority is untouched (§0.2),
// and a cap can only lower a request, so E-stop and NEUTRAL behave exactly
// as before.
// LAYER 5 (CODEX 1.14 reviews, rounds 1 AND 2): the limiter architecture.
// Round 1 put ctoLimitPwm() inside these two writers. Round 2 found that is
// necessary but NOT sufficient — a cap applied only at request time cannot
// react to a peer EVENT during steady cruise, when commandedPwm==want and no
// request ever fires. So: ctoDesiredPwm keeps the last UNCAPPED AUTO intent,
// these writers apply the cap at request time, and ctoService() re-applies
// cap(desired) EVERY pass on the loop thread — a cap that appears mid-cruise
// takes effect within one loop, and when it lifts the desired speed is
// restored (the spec's traffic resume). Callers that ask "did my request
// take?" must compare against ctoDesiredPwm, never commandedPwm, or a
// standing cap makes them re-request forever and recompute the ramp from a
// shrinking delta (round 2 finding 3).
static int ctoDesiredPwm=0;
// Round 3 (CODEX): ONE deceleration profile, literally. Every CTO-caused
// reduction — request-time cap in either writer, or the continuous
// enforcement pass — brakes at STATION_DOWN_STEP_MS (200 ms/PWM), the rate
// the 18/12/6 ladder's measured stopping distance was derived from. Without
// this, the same traffic condition braked at 150, 200 or a duration-derived
// rate depending on which path saw it first, and the ladder's justification
// did not hold. Restores (cap lifting) are not braking and keep their own
// rates.
static void requestPwm(int target,uint16_t stepMs){
  int t=constrain(target,0,255);
  ctoDesiredPwm=t;
  if(autoRunning){
    int c=ctoLimitPwm(t);
    if(c<t) stepMs=STATION_DOWN_STEP_MS;   // CTO reduced requested authority: the one profile
    t=c;
  }
  commandedPwm=t;
  pwmStepMs=stepMs;
}

// Reach the target in roughly durationMs, whatever the current PWM. Derives
// the step rate from the distance still to travel.
static void requestPwmOver(int target,uint16_t durationMs){
  int t=constrain(target,0,255);
  ctoDesiredPwm=t;
  bool ctoBraking=false;
  if(autoRunning){
    int c=ctoLimitPwm(t);
    if(c<t) ctoBraking=true;
    t=c;
  }
  int delta=abs(t-actualPwm);
  commandedPwm=t;
  pwmStepMs = ctoBraking ? STATION_DOWN_STEP_MS
            : (delta>0) ? (uint16_t)max(5UL,(unsigned long)durationMs/(unsigned long)delta) : 50;
}

static void servicePwmRamp(){
  // §3 stop-transition invalidation: three paths zero actualPwm (the NEUTRAL
  // interlock, the E-stop clamp, the ramp decrement), so the nonzero->zero
  // edge is detected once here rather than patched at all three. Edge-
  // triggered, not level-triggered; the one-pass lag (~35 ms) is immaterial
  // against ~900 ms marker intervals. Without this, a dwell then a restart
  // would test the first marker against an interval from before the stop.
  static int lastSeenActual = 0;
  if (lastSeenActual > 0 && actualPwm == 0) invalidatePreviousAcceptedDt();
  lastSeenActual = actualPwm;
  // NEUTRAL interlock: no throttle command can produce movement. This is what
  // made an accidental throttle nudge harmless in the previous system, and
  // what makes clearing an E-stop require a deliberate direction selection.
  if(motorDirection==DIRECTION_NEUTRAL){ commandedPwm=0; actualPwm=0; pwmWriteCompat(0); return; }
  // Under E-stop the ramp is HELD AT ZERO, not merely masked at the hardware.
  // Masking allowed actualPwm to climb internally while stopped, so clearing
  // E-stop exposed an already-elevated PWM on the next write. (Codex)
  if(estopped){ commandedPwm=0; actualPwm=0; pwmWriteCompat(0); return; }
  unsigned long now=millis();
  if(now-lastPwmStepMs < pwmStepMs) return;
  lastPwmStepMs=now;
  if(actualPwm<commandedPwm) actualPwm++;
  else if(actualPwm>commandedPwm) actualPwm--;
  else return;
  pwmWriteCompat(actualPwm);
}


// ---------------------------------------------------------------------------
// OPERATOR DIRECTION CHANGE (v1.6, operator rule) — obey where obeying is
// physically possible. Until now any direction change was refused while
// motorIsMoving(), which is true at PWM 1: the command was discarded and the
// operator was left guessing. The only refusal that survives is the one whose
// reason is the hardware, not a preference.
//
//   actualPwm >  MOTOR_DEAD_ZONE_PWM   REFUSE, and name the number to come
//        below. Reversing an energised H-bridge is plugging: back-EMF adds to
//        supply voltage and current reaches roughly twice stall, which can
//        trip the driver or brown out the ESP32 and take navigation with it.
//   actualPwm <= MOTOR_DEAD_ZONE_PWM   OBEY. Below the tractive floor the
//        wheels are not turning. Snap to zero exactly as E-stop does, apply
//        the direction, restore the operator's own throttle.
//
// AUTO is a separate question and is tested by the callers: a locomotive that
// has enlisted is not taking manual direction commands.
// ---------------------------------------------------------------------------
static bool operatorDirectionPermitted(const char* what){
  if(actualPwm > MOTOR_DEAD_ZONE_PWM){
    char n[96];
    snprintf(n,sizeof(n),"REDUCE_THROTTLE_TO_%d_OR_BELOW_FIRST",MOTOR_DEAD_ZONE_PWM);
    stationPublish(what[0]=='S'?"SESSION_DIR_REFUSED":"DIR_REFUSED",0,n);
    char w[128];
    snprintf(w,sizeof(w),"DIRECTION NOT CHANGED — reduce throttle to %d or below first (now %d)",
             MOTOR_DEAD_ZONE_PWM,(int)actualPwm);
    publishWarning(w);
    return false;
  }
  return true;
}

static void applyOperatorDirection(int v,const char* what){
  if(!operatorDirectionPermitted(what)) return;
  int restore = commandedPwm;
  commandedPwm=0; actualPwm=0; pwmWriteCompat(0);   // instant, like E-stop
  motorDirection=v;
  applyDirection();                                 // navDir follows the motor
  if(restore>0) requestPwm(restore,NORMAL_STEP_MS); // give the operator it back
  navPublishState("DIRECTION",nullptr);
}

static void stationSetPhase(StationPhase p){ stPhase=p; stPhaseEnteredMs=millis(); }

static void stationReset(const char* note){
  stPhase=ST_IDLE; stIndex=-1; stPhaseEnteredMs=millis();
  stationPublish("RESET",999,note);
}

static void serviceStations(){
  // The absolute escape runs FIRST, before any state gate. Codex found it sat
  // below the NAV_TRACKING return, so losing position mid-approach disabled
  // the only autonomous exit and left the phase wedged until an external
  // command arrived. "Every state has an exit" has to mean unconditionally.
  // ST_FINAL has its own physical fallback, ST_DWELL has its own timer, and
  // ST_DEPART is deliberately exempt. This guards ST_APPROACH and ST_RAMP.
  if((stPhase==ST_APPROACH || stPhase==ST_RAMP) &&
     millis()-stPhaseEnteredMs > STATION_MAX_PHASE_MS){
    stationReset("PHASE_TIMEOUT");
    // Only return to cruise if the navigator's position is usable. A timeout
    // firing in NAV_NO_QUORUM must not promote a navigation failure straight
    // back to full speed (the stop request has already been issued there).
    if(autoRunning && navPositionUsable()) requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
    return;
  }

  // The M+1 fallback runs regardless of NAVIGATION state — a loco that loses
  // position during final approach still needs to stop — but it is
  // AUTO-chamber station automation, gated per §0.2 (v1.4, CODEX
  // reclassification): the constitution admits no navigation-originated
  // ungated motor write except E-stop. Today the gate changes no reachable
  // behaviour — a non-IDLE station phase already implies AUTO, since every
  // path that clears autoRunning also resets the station machine — but the
  // explicit gate means a future station-reset regression cannot violate
  // §0.2 silently.
  if(autoRunning &&
     stPhase==ST_FINAL && stMPlus1AtMs &&
     millis()-stMPlus1AtMs >= FINAL_M1_TIMEOUT_MS){
    stationSetPhase(ST_RAMP);
    requestPwm(0,STATION_DOWN_STEP_MS);
    stationPublish("ZERO_RAMP",1,"TRIGGER_M1_TIMEOUT_DID_NOT_REACH_M2");
    return;
  }

  // §6.1: stations must keep working during NAV_EVALUATING — position is held
  // and probably correct; suspending arming for 3-12 markers would drive past
  // a station, the failure the arming comment below warns against.
  if(!autoRunning || !navPositionUsable() || navDir==MAP_UNSET) return;

  if(stPhase==ST_IDLE){
    // Range arming. A skipped marker or a position correction can no longer
    // cause a station to be silently missed.
    //
    // There is deliberately NO confidence threshold here. Refusing to arm on a
    // shaky fix meant the locomotive drove past the station rather than
    // stopping in a slightly wrong place -- suppressing the behaviour the
    // sketch exists to produce, and hiding the sensor problem behind it.
    // The asymmetry runs the other way: a stop made on poor evidence is
    // visible and informative, a station silently skipped is neither.
    // (The confidence tally is gone with QUORUM; the nav state — NORMAL vs
    // EVALUATING — is the arming-time evidence quality now.)
    for(uint8_t i=0;i<STATION_COUNT;i++){
      if(!stationEnabled(i)) continue;   // Station Stop v1: mission filter
      int16_t o=offsetToCentre(navMm,navDir,STATIONS[i].centerMm);
      if(o<=APPROACH_START && o>APPROACH_START-3){
        // CE EXPRESS: skip every third station MET, so the skipped platform
        // rotates — with four stations the pattern is Arches, then Grillers,
        // then Patio, then Bamboo, repeating every three laps rather than
        // punishing one platform every time.
        //
        // The latch makes the decision once per encounter. Arming is naturally
        // once-per-station because it changes phase out of ST_IDLE, but a SKIP
        // does not, so without the latch this polling loop would re-decide (and
        // re-count) every pass. Comparing against the station INDEX is enough:
        // the next station met is always a different one on a loop.
        if(ceSkipLatch != (int8_t)i){
          ceSkipLatch = (int8_t)i;
          ceSkipNow   = false;
          if(ceMission==CE_EXPRESS){
            // Counter advances ONCE PER STATION MET, and only under the express
            // mission, so the rotation is a property of this CE run. Written out
            // rather than folded into the condition: an increment hidden behind
            // && stops happening the moment the left operand goes false, which
            // is precisely the sort of quiet dependency that survives review.
            ceSkipNow = ((ceStationSeq % CE_SKIP_EVERY) == (CE_SKIP_EVERY-1));
            ceStationSeq++;
            if(ceSkipNow) stationPublish("SKIPPED",o,"CE_EXPRESS_ROTATING_SKIP");
          }
        }
        if(ceSkipNow) continue;   // run through: no arm, no phase change
        stIndex=(int8_t)i; stationSetPhase(ST_APPROACH);
        requestPwmOver(approachTargetForOffset(o,STATIONS[i].zonePwm),APPROACH_RAMP_MS);
        stationPublish("ARMED",o,(navState==NAV_NORMAL)?"RANGE_ARM_NORMAL":"RANGE_ARM_EVALUATING");
        return;
      }
    }
    // No station armed: keep cruise following the section map through the
    // normal path. requestPwmOver() remains the sole writer of commandedPwm.
    int want = cruiseForPosition();
    // Compare against the DESIRED (uncapped) target: under a standing CTO
    // cap, comparing against commandedPwm re-requested every pass and
    // stretched the ramp by recomputing pwmStepMs from a shrinking delta
    // (round 2 finding 3). Enforcement of the cap itself lives in
    // ctoService(), not here.
    if(ctoDesiredPwm != want) requestPwmOver(want, APPROACH_RAMP_MS);
    return;
  }

  const int16_t o=offsetToCentre(navMm,navDir,STATIONS[stIndex].centerMm);

  // Overshoot escape, checked in every non-idle phase before anything else.
  if(o > OVERSHOOT_ABANDON && stPhase!=ST_DWELL && stPhase!=ST_DEPART){
    stationPublish("MISSED",o,"OVERSHOT_CENTRE_RETURNING_TO_IDLE");
    stationReset("MISSED");
    requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
    return;
  }

  switch(stPhase){
    case ST_APPROACH:
      if(o>=APPROACH_START && o<ZONE_START){
        requestPwmOver(approachTargetForOffset(o,STATIONS[stIndex].zonePwm),APPROACH_RAMP_MS);
        stationPublish("APPROACH",o,"DERIVED_CRUISE_TO_ZONE");
      }else if(o>=ZONE_START && o<finalEntryOffset(stIndex)){
        requestPwmOver(STATIONS[stIndex].zonePwm,APPROACH_RAMP_MS);
        stationPublish("ZONE_HOLD",o,"HOLD_60");
      }else if(o>=finalEntryOffset(stIndex)){
        stationSetPhase(ST_FINAL);
        stMPlus1AtMs=0;
        // Hold approach speed THROUGH the centre. The stop is made after the
        // sensor has passed the station, not while arriving at it.
        requestPwmOver(STATIONS[stIndex].zonePwm,FINAL_RAMP_MS);
        stationPublish("FINAL_APPROACH",o,"LOCATION_TRIGGERED_TARGETS");
      }
      break;

    case ST_FINAL: {
      // Braces required: stopAt is declared here, and without a scope block
      // the later case labels jump past its initialisation, which C++ forbids.
      //
      // Location-triggered, one target per marker. Not a timed interpolation
      // to a single distant target — that is what left the loco at PWM 25 a
      // marker early and out of tractive effort.
      const int8_t stopAt = effStopOffset(stIndex);
      if(o>=0 && o<stopAt){
        // M   : still at zone speed, passing the station
        // M+1 : ease to finalPwm -- the speed the stop is made from
        requestPwmOver(o==0?STATIONS[stIndex].zonePwm:STATIONS[stIndex].finalPwm,
                       FINAL_RAMP_MS);
        // v1.12C: with stopAt down to 1 (or 0) the fallback clock must arm
        // at the CENTRE marker — o==1 is now at/after the stop trigger for
        // every station, so the old arming point would never fire.
        if(o==0 && stMPlus1AtMs==0) stMPlus1AtMs=millis();  // start the fallback clock
        stationPublish("FINAL_TARGET",o,o==0?"AT_CENTRE_ZONE_SPEED":"M_PLUS_1_FINAL_SPEED");
      }else if(o>=stopAt){
        stationSetPhase(ST_RAMP);
        requestPwm(0,STATION_DOWN_STEP_MS);
        // Which trigger fired is a fact worth logging: repeated M1_TIMEOUT at
        // one station means that approach profile is too aggressive there.
        stationPublish("ZERO_RAMP",o,"TRIGGER_M2_REACHED");
      }
      break;
    }

    case ST_RAMP:
      if(actualPwm<=0){
        stationSetPhase(ST_DWELL);
        stDwellStartedMs=millis();
        stationPublish("DWELL_BEGIN",o,"FIXED_DWELL");
      }
      break;

    case ST_DWELL:
      // Operator ruling 2026-08-14: the leader's departure does not depend on
      // the follower's position AT ALL.
      //
      // What was here held the leader at the platform until the follower had
      // arrived within a window and a 10 s release dwell had run. On
      // 2026-08-14 that deadlocked the railway at Arches: the follower stopped
      // at bound gap 11 -- exactly where CTO_STOP_GAP_MARKERS(12) told it to --
      // while the arrival test demanded <= CLEAR(6)+TOL(4) = 10, so the release
      // timer reset every pass and both locomotives sat indefinitely.
      //
      // The threshold was only the symptom. The logic ran backwards: a leader
      // with clear track ahead cannot improve separation by standing still,
      // and departing is precisely what opens the gap. Spacing is the
      // FOLLOWER's constraint, carried by ctoLimitPwm(). A stopped follower
      // must never inhibit the leader.
      //
      // Fleet stop and role conflict still stop this locomotive -- through the
      // limiter's continuous enforcement, not through the station machine.
      if(millis()-stDwellStartedMs>=ctoDwellMs()){
        stationSetPhase(ST_DEPART);
        stDepartBeganMs=millis(); stDepartWarned=false;
        // Straight to cruise. Station speed exists to arrive accurately; once
        // stopped there is nothing left to be careful about. 2_13 departed at
        // the GLOBAL station speed of 60 -- not even the station's own -- and
        // held it for three markers, which on the Grillers climb with cars
        // attached was not enough to pull the grade. The locomotive crawled,
        // magnet events stretched to four seconds, occupancy of the median
        // window reached 90%, the baseline was corrupted and navigation was
        // lost. A throttle number caused a navigation failure.
        requestPwm(cruiseForPosition(),STATION_UP_STEP_MS);
        stationPublish("DWELL_COMPLETE",o,"DEPART_TO_CRUISE");
      }
      break;

    case ST_DEPART:
      // No timeout here. A loco accelerating from a stop on clear track has
      // nothing to be rescued from, and a phase reset would only hide a real
      // mechanical fault. If departure is slow, TELL THE OPERATOR and keep
      // trying -- notification, not intervention.
      if(!stDepartWarned && millis()-stDepartBeganMs > 15000UL){
        stDepartWarned=true;
        stationPublish("DEPARTURE_SLOW",o,"NOT_CLEARED_ZONE_IN_15S_CHECK_LOCO");
      }
      if(o>=effStopOffset(stIndex)+3){
        // Already at cruise; this only releases the station machine so the
        // next one can arm.
        stationPublish("DEPARTURE_COMPLETE",o,"CLEARED_ZONE");
        stationReset("DEPARTED");
      }
      break;

    default: break;
  }
}

// ===========================================================================
// MQTT
// ===========================================================================
static WiFiClient   espClient;
static PubSubClient mqtt(espClient);

// The two doors between loop() and the network task. pub() enqueues onto
// pubQueue (drained and published by networkTask); the MQTT callback enqueues
// onto cmdQueue (drained by serviceCommands() on the loop thread, which owns all
// locomotive state). pubDrops is cumulative; pubQueueHw is the windowed max
// occupancy, reset on each loopstat publish.
static QueueHandle_t pubQueue=nullptr, cmdQueue=nullptr;
static uint32_t      pubDrops=0;
static uint16_t      pubQueueHw=0;
// CHANGE 1 (v2.21) — markers get their OWN publish queue. On 2026-07-30 the
// single 32-slot pubQueue, evicting its OLDEST entry when full, dropped 22,774
// telemetry messages and tore visible holes in the marker stream (MM154->MM145
// in one step). Status is re-sent within a second and its stale value is worth
// evicting; a marker event happens once and cannot be re-derived. They must not
// share a queue. markerPubDrops is cumulative; markerPubHw is the windowed max
// occupancy, both reported in loopstat and reset like their pubQueue twins.
static QueueHandle_t markerPubQueue=nullptr;
static uint32_t      markerPubDrops=0;
static uint16_t      markerPubHw=0;
// Change 4 (v2.20). cmdDrops: inbound commands lost to a full cmdQueue. Written
// on the network task (onMqttEnqueue), read on the loop thread (publishStat) --
// volatile. A dropped command must never be silent; v2.19 ignored the send
// result entirely. pubWindowCount: publish calls in the current loopstat window,
// bumped in pub() and reset each publishStat, reported as pub_per_s. If Change 3
// works this drops from ~12/s to ~1/s parked.
static volatile uint32_t cmdDrops=0;
static volatile uint32_t pubWindowCount=0;

static char T_ONLINE[64],T_NAV[64],T_MARKER[64],T_STATION[64],T_STAT[64],T_BOOT[64],T_ALERT[64];
#ifdef TEMPLATES_ADMISSION
static char T_ADMIT_REJECT[64];   // TEMPLATES audit trail; diagnostic only
static char T_R3_ADMIT[64];       // 0.3 R3 identity-test decision record (spec §10)
#endif
static char T_NO_QUORUM[64];   // §2.5: served ONLY by the desired-retained-state mechanism

// ---------------------------------------------------------------------------
// DASHBOARD STATE TOPICS
//
// The Flask console does not merely send commands, it waits for the firmware
// to acknowledge them on these topics before unlocking its controls. 2_8
// dropped them, so the session-direction badge stayed red on "SET SESSION
// DIRECTION TO ENABLE THROTTLE" and the dashboard correctly refused to drive a
// locomotive that had not confirmed it was ready. The firmware was at fault,
// not the dashboard.
//
// Names and payload shapes match SOLONAV_1_x exactly. Fields this sketch no
// longer has (brake, start interval, must-hold-eligible) are published as
// inert constants so the console's parsing does not break.
// ---------------------------------------------------------------------------
static char T_ST_THROTTLE[64],T_ST_DIRECTION[64],T_ST_BRAKE[64],T_ST_ESTOP[64],
            T_ST_AUTO[64],T_ST_SESSDIR[64],T_ST_STARTINT[64],T_ST_STARTMM[64],
            T_ST_MHE[64],T_ST_NAVREADY[64],T_ST_WARNING[64];
// v1.7 (decision 0012) — the three r12 telemetry topics and the low-voltage
// flag, restored under their original names so the console's power tile binds
// without a dashboard change.
static char T_TELEM_V[64],T_TELEM_A[64],T_TELEM_W[64],T_ST_LOWVOLT[64];
static char T_CMD_AUTO[64],T_CMD_GO[64],T_CMD_STOP[64],T_CMD_DIR[64],T_CMD_SESSDIR[64];
static char T_CMD_CE[64];
static char T_CMD_STARTMM[64],T_CMD_ESTOP[64],T_CMD_ESTOP_ALL[64],T_CMD_THROTTLE[64],T_CMD_STARTINT[64],
            T_CMD_RELEASE[64],T_CMD_FORCELOST[64],T_CMD_CTO[64];
static char T_ST_CTO[64];   // LAYER 5 state topic: role, partner, gaps, holds

static void buildTopics(){
  const char* id=LOCO_NAME;
  snprintf(T_ONLINE ,64,"ngr/loco/%s/online"        ,id);
#ifdef TEMPLATES_ADMISSION
  snprintf(T_ADMIT_REJECT,64,"ngr/loco/%s/diag/admit_reject",id);
  snprintf(T_R3_ADMIT    ,64,"ngr/loco/%s/diag/r3_admit"    ,id);
#endif
  snprintf(T_NAV    ,64,"ngr/loco/%s/state/nav"     ,id);
  snprintf(T_MARKER ,64,"ngr/loco/%s/mm/marker"     ,id);
  snprintf(T_STATION,64,"ngr/loco/%s/state/station" ,id);
  snprintf(T_STAT   ,64,"ngr/loco/%s/state/loopstat",id);
  snprintf(T_BOOT   ,64,"ngr/loco/%s/state/bootid"  ,id);
  snprintf(T_ALERT  ,64,"ngr/loco/%s/alert"         ,id);
  snprintf(T_NO_QUORUM,64,"ngr/loco/%s/mm/no_quorum",id);
  snprintf(T_ST_THROTTLE ,64,"ngr/loco/%s/state/throttle"         ,id);
  snprintf(T_ST_DIRECTION,64,"ngr/loco/%s/state/direction"        ,id);
  snprintf(T_ST_BRAKE    ,64,"ngr/loco/%s/state/brake"            ,id);
  snprintf(T_ST_ESTOP    ,64,"ngr/loco/%s/state/estop"            ,id);
  snprintf(T_ST_AUTO     ,64,"ngr/loco/%s/state/auto"             ,id);
  snprintf(T_ST_SESSDIR  ,64,"ngr/loco/%s/state/session_direction",id);
  snprintf(T_ST_STARTINT ,64,"ngr/loco/%s/state/start_interval"   ,id);
  snprintf(T_ST_STARTMM  ,64,"ngr/loco/%s/state/start_mm"         ,id);
  snprintf(T_ST_MHE      ,64,"ngr/loco/%s/state/must_hold_eligible",id);
  snprintf(T_ST_NAVREADY ,64,"ngr/loco/%s/state/nav_ready"        ,id);
  snprintf(T_ST_WARNING  ,64,"ngr/loco/%s/state/warning"          ,id);
#if IR_TEST_A_ON
  // IR TEST A topics (§9). New topics only — no existing payload changes.
  snprintf(T_IR_SPEED_T  ,64,"ngr/loco/%s/telem/ir_speed"         ,id);
  snprintf(T_IR_STATUS   ,64,"ngr/loco/%s/telem/ir_status"        ,id);
  snprintf(T_SPEED_VIEW  ,64,"ngr/loco/%s/telem/speed"            ,id);
  snprintf(T_MM_SPEED    ,64,"ngr/loco/%s/mm/speed"               ,id);
#endif
  snprintf(T_TELEM_V     ,64,"ngr/loco/%s/telem/voltage"          ,id);
  snprintf(T_TELEM_A     ,64,"ngr/loco/%s/telem/current"          ,id);
  snprintf(T_TELEM_W     ,64,"ngr/loco/%s/telem/power"            ,id);
  snprintf(T_ST_LOWVOLT  ,64,"ngr/loco/%s/state/lowvolt"          ,id);
  snprintf(T_CMD_AUTO    ,64,"ngr/loco/%s/cmd/auto"             ,id);
  snprintf(T_CMD_DIR     ,64,"ngr/loco/%s/cmd/direction"        ,id);
  snprintf(T_CMD_SESSDIR ,64,"ngr/loco/%s/cmd/session_direction",id);
  snprintf(T_CMD_STARTMM ,64,"ngr/loco/%s/cmd/start_mm"         ,id);
  snprintf(T_CMD_STARTINT,64,"ngr/loco/%s/cmd/start_interval"   ,id);
  snprintf(T_CMD_RELEASE ,64,"ngr/loco/%s/cmd/dispatcher_release",id);
  snprintf(T_CMD_FORCELOST,64,"ngr/loco/%s/cmd/force_lost"       ,id);
  snprintf(T_CMD_CTO     ,64,"ngr/loco/%s/cmd/cto"              ,id);
  snprintf(T_ST_CTO      ,64,"ngr/loco/%s/state/cto"            ,id);
  snprintf(T_CMD_ESTOP   ,64,"ngr/loco/%s/cmd/estop"            ,id);
  // E-STOP IS THE ONE CROSSING THAT IS ALWAYS OPEN. The dispatcher console's
  // E-STOP publishes to this BROADCAST topic, not a per-locomotive one, and
  // nothing in the lineage had ever subscribed to it — so the big red button
  // on the console page had never worked, on SOLONAV or QUORUM. Every other
  // dispatcher command is auto-chamber only; this one reaches both.
  snprintf(T_CMD_ESTOP_ALL,64,"ngr/dispatcher/cmd/estop");
  // CE is fleet-wide: one press, both locomotives take the mission their own
  // role implies. The console has published this since v1.9.5 with nothing
  // listening (server/ngr_app_v1_11_2.py:2381).
  snprintf(T_CMD_CE      ,64,"ngr/dispatcher/cmd/ce");
  snprintf(T_CMD_THROTTLE,64,"ngr/loco/%s/cmd/throttle"         ,id);
  snprintf(T_CMD_GO      ,64,"ngr/dispatcher/cmd/go/%s"         ,id);
  snprintf(T_CMD_STOP    ,64,"ngr/dispatcher/cmd/stop/%s"       ,id);
}

// pub() ENQUEUES and returns immediately -- it must not reference mqtt at all,
// so no publish site on the loop thread can block on the network. networkTask is
// the only place a message actually leaves the radio. When the link degrades the
// queue fills and we drop the OLDEST message: stale telemetry is worth less than
// fresh, and losing some log is the correct failure -- losing navigation never
// was.
static void pub(const char* t,const char* m,bool retain=false){
  if(!pubQueue) return;
  pubWindowCount++;                        // Change 4: counted for pub_per_s
  PubMsg msg; msg.topic=t; msg.retain=retain;
  strlcpy(msg.payload,m,sizeof(msg.payload));
  if(xQueueSend(pubQueue,&msg,0)!=pdTRUE){
    PubMsg discard;                          // queue full: drop the OLDEST
    xQueueReceive(pubQueue,&discard,0);
    xQueueSend(pubQueue,&msg,0);
    pubDrops++;
  }
  UBaseType_t w=uxQueueMessagesWaiting(pubQueue);
  if(w>pubQueueHw) pubQueueHw=(uint16_t)w;
}

// The marker-only door. Same enqueue-and-return contract as pub(), but onto
// markerPubQueue and NEVER retained -- mm/marker is an event stream.
//
// DROP-NEWEST, not drop-oldest, and the asymmetry with pub() is deliberate. A
// contiguous run of markers with a gap only at the END is reconstructable from
// dead reckoning; a run with holes scattered through it is not. So if this queue
// ever fills we refuse the newcomer rather than evict a delivered-in-order
// backlog, and markerPubDrops records exactly how many are missing.
static void pubMarker(const char* t,const char* m){
  if(!markerPubQueue) return;
  pubWindowCount++;                        // counts toward pub_per_s like pub()
  PubMsg msg; msg.topic=t; msg.retain=false;
  strlcpy(msg.payload,m,sizeof(msg.payload));
  if(xQueueSend(markerPubQueue,&msg,0)!=pdTRUE) markerPubDrops++;   // drop NEWEST
  UBaseType_t w=uxQueueMessagesWaiting(markerPubQueue);
  if(w>markerPubHw) markerPubHw=(uint16_t)w;
}

// F6 (CODEX review of 1.4): the nav payloads reported NEUTRAL as "REV",
// because they tested only for FORWARD. publishAlert() already had the
// three-way form; these did not. A logging defect, not a control one — the
// dashboard binds its direction display to the state/direction integer — but
// a replay reading motor_dir would have been misled.
static const char* motorDirName(){
  return motorDirection==DIRECTION_FORWARD ? "FWD"
       : (motorDirection==DIRECTION_REVERSE ? "REV" : "NEU");
}

static void navPublishState(const char* ev,const MarkerEvent* e){
  // §0.1/§5: every decision reconstructable from the log — nav_state,
  // miss_streak, the full score vector, the leading offset and its margin.
  // The confidence tally is gone and is not reintroduced as telemetry.
  char b[512];
  char sc[8*QUORUM_CANDIDATES], ex[8*QUORUM_CANDIDATES];
  jsonScores(sc,sizeof(sc));
  jsonExcluded(ex,sizeof(ex),false);
  char ld[8], ru[8];
  if(leaderIdx>=0)   snprintf(ld,sizeof(ld),"%d",(int)QUORUM_OFFSETS[leaderIdx]);   else strlcpy(ld,"null",sizeof(ld));
  if(runnerUpIdx>=0) snprintf(ru,sizeof(ru),"%d",(int)QUORUM_OFFSETS[runnerUpIdx]); else strlcpy(ru,"null",sizeof(ru));
  if(e){
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,"
      "\"landmark\":\"%s\",\"dir\":\"%s\",\"miss_streak\":%u,"
      "\"obs\":\"%c\",\"expected\":\"%c\",\"peak\":%d,\"ms\":%u,"
      "\"drift\":%d,\"dt\":%u,\"agree\":%lu,\"disagree\":%lu,\"lost\":%lu,"
      "\"scores\":%s,\"excluded\":%s,\"leader\":%s,\"runner_up\":%s,\"margin\":%d,"
      "\"motor_dir\":\"%s\"}",
      ev, navStateName(), navStateName(), navMm,
      landmarkAt(navMm), dirName(navDir), (unsigned)missStreak,
      polChar(e->polarity), polChar(dnaAt(navMm)), e->peak, e->durationMs,
      e->baselineDrift, lastSegmentDt, navAgree, navDisagree, navLostCount,
      sc, ex, ld, ru, (int)quorumMargin,
      motorDirName());
  }else{
    // The short payload is what DIRECTION and SESSION_DIRECTION publish, so it
    // has to carry the direction fields — those are the events a consumer most
    // wants them on. (Codex)
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,\"dir\":\"%s\","
      "\"motor_dir\":\"%s\",\"session_dir\":\"%s\",\"miss_streak\":%u}",
      ev, navStateName(), navStateName(), navMm, dirName(navDir),
      motorDirName(), dirName(sessionDir),
      (unsigned)missStreak);
  }
  // F5 (CODEX findings 5/6): event-bearing publishes — AGREE/DISAGREE, one
  // observation per marker, not re-derivable — ride the durable in-order
  // marker path. Current-value state publishes (the no-event form: DECLARED,
  // DIRECTION, SESSION_DIRECTION, ...) stay on pub(), where eviction of a
  // stale copy is correct.
  if(e) pubMarker(T_NAV,b);
  else  pub(T_NAV,b);
  Serial.printf("[NAV] %s\n",b);
}

// §5.1: QUORUM decision events — adoption, incident open/close, phantom
// rejection, fixture — are one-time and non-re-derivable, so they ride
// pubMarker(): a replay with the marker stream intact but the adoption event
// evicted is unreadable. Carries the decision payload contract: state,
// streak, score vector, exclusions, leader, runner-up, margin (+viable, §4).
static void publishQuorumDecision(const char* ev,const char* extra){
  char b[512];
  char sc[8*QUORUM_CANDIDATES], ex[8*QUORUM_CANDIDATES], vb[48];
  jsonScores(sc,sizeof(sc));
  jsonExcluded(ex,sizeof(ex),false);
  jsonViable(vb,sizeof(vb));
  char ld[8], ru[8];
  if(leaderIdx>=0)   snprintf(ld,sizeof(ld),"%d",(int)QUORUM_OFFSETS[leaderIdx]);   else strlcpy(ld,"null",sizeof(ld));
  if(runnerUpIdx>=0) snprintf(ru,sizeof(ru),"%d",(int)QUORUM_OFFSETS[runnerUpIdx]); else strlcpy(ru,"null",sizeof(ru));
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"state\":\"%s\",\"mm\":%u,\"streak\":%u,"
    "\"scores\":%s,\"excluded\":%s,\"leader\":%s,\"runner_up\":%s,"
    "\"margin\":%d,\"eval\":%u,\"viable\":%s%s}",
    ev, navStateName(), navMm, (unsigned)missStreak,
    sc, ex, ld, ru,
    (int)quorumMargin, (unsigned)evalCount, vb, extra);
  pubMarker(T_NAV,b);
  Serial.printf("[NAV] %s\n",b);
}

// The console shows state/warning to the operator. Without it a refusal is
// published, logged, and invisible -- which on 2026-07-27 meant a backwards
// start interval was rejected, GO was then refused for having no position, and
// from the garden it looked like the locomotive had simply stopped responding.
// Anything ending in _REFUSED, plus MISSED and DEPARTURE_SLOW, goes here.
static unsigned long warningSetMs=0;
static void publishWarning(const char* text){
  pub(T_ST_WARNING,text);
  warningSetMs = text[0] ? millis() : 0;
}

// CODEX 1.16 review, finding 1 — THE RESUME INTERLOCK, enforced not promised.
// enterNoQuorum() never drops autoRunning (it stops via a zero request), so
// restoring NAV_NORMAL in nqLearn() would let serviceStations()'s idle branch
// re-request cruise on the next pass and RESTART a lost-and-found locomotive
// without an operator. Knowledge recovery is not motion recovery: AUTO drops
// here, explicitly, and resuming requires a deliberate GO. (The harness never
// calls serviceStations(), which is why no fixture could catch this — the
// reviewer read the loop instead.)
static void nqDropAutoInterlock(){
  if(!autoRunning) return;
  autoRunning=false;                       // the interlock itself
  requestPwm(0,NORMAL_STEP_MS);            // already zero in NO_QUORUM; belt and braces
  stationReset("SELF_RESOLVED");           // clear any armed station phase
  publishWarning("SELF_RESOLVED: position recovered - BEGIN to resume");
}
// Clear it after a while so a stale warning does not sit on the dashboard.
static void serviceWarningExpiry(){
  if(warningSetMs && millis()-warningSetMs > 20000UL){ warningSetMs=0; pub(T_ST_WARNING,""); }
}

// P13 (ruling R14): monotonic sequence for operator-command RESPONSES only.
// Counts only the bypass set below, so a repeated refusal is visibly a NEW
// response, not a replay.
static uint32_t stationRespSeq=0;

static void stationPublish(const char* ev,int16_t off,const char* note){
  // Publish on TRANSITION only. serviceStations() runs every loop pass, so
  // 2_10 republished the same line continuously -- APPROACH at offset -10 went
  // out 79 times, 2381 station messages in one lap. Flooding the broker and
  // burying the transitions that matter.
  //
  // P13 (ruling R14; console-authority spec Draft 5.1): COMMAND RESPONSES
  // are exempt from that dedup. The dedup compares event+offset and ignores
  // the note, so it was built for state-machine transitions — applied to
  // responses it let one refusal reason suppress a different one, and made
  // a repeated command produce silence (CODEX Draft-3 F2). The bypass set
  // is exactly R14's: *_REFUSED plus STOP_IGNORED — the events that answer
  // an operator command. Each carries a monotonic "seq" so repeats are
  // distinguishable. Routine transitions keep the dedup it was built for.
  const bool isResponse = (strstr(ev,"_REFUSED")!=nullptr) || (strcmp(ev,"STOP_IGNORED")==0);
  static const char* lastEv=nullptr;
  static int16_t     lastOff=32767;
  if(!isResponse){
    if(ev==lastEv && off==lastOff) return;
    lastEv=ev; lastOff=off;
  }

  char b[288];
  const char* nm = (stIndex>=0 && stIndex<(int8_t)STATION_COUNT) ? STATIONS[stIndex].name : "NONE";
  if(isResponse){
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"phase\":%u,\"station\":\"%s\",\"offset\":%d,"
      "\"commanded_pwm\":%d,\"actual_pwm\":%d,\"note\":\"%s\",\"seq\":%lu}",
      ev,(unsigned)stPhase,nm,off,commandedPwm,actualPwm,note,
      (unsigned long)++stationRespSeq);
  }else{
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"phase\":%u,\"station\":\"%s\",\"offset\":%d,"
      "\"commanded_pwm\":%d,\"actual_pwm\":%d,\"note\":\"%s\"}",
      ev,(unsigned)stPhase,nm,off,commandedPwm,actualPwm,note);
  }
  pub(T_STATION,b);
  Serial.printf("[STN] %s\n",b);

  // Surface anything the operator needs to act on.
  if(strstr(ev,"REFUSED") || !strcmp(ev,"MISSED") || !strcmp(ev,"DEPARTURE_SLOW")){
    char w[160]; snprintf(w,sizeof(w),"%s: %s",ev,note);
    publishWarning(w);
  }
}

// ---------------------------------------------------------------------------
// ALERT — retained, so a follower or the dispatcher sees it even on late
// connect. This is the message that has to be actionable by another train:
// not "something is wrong" but where this locomotive was last certain, how far
// it may have travelled since, and whether it is still moving.
// ---------------------------------------------------------------------------
static unsigned long lastAlertMs=0;

static void publishAlert(const char* level,const char* reason){
  lastAlertMs=millis();
  char b[640];

  // OCCUPANCY BOUND — the actionable part for a following locomotive.
  //   rear  : the rearmost point this loco can possibly occupy. It cannot be
  //           behind the last marker it actually confirmed.
  //   front : dead reckoning plus a margin, because a loco that is misreading
  //           markers may also be MISSING them and running ahead of its own
  //           odometer.
  // A follower should hold clear of `rear`, not of `dead_reckoned_mm`.
  uint8_t  rear  = haveConfirmed ? lastConfirmedMm : navMm;
  uint8_t  front = (navDir==MAP_UNSET) ? navMm
                 : routeMod((int32_t)navMm + (int32_t)navDir*LOST_FRONT_MARGIN_MARKERS);
  uint32_t envelope = (navDir==MAP_UNSET) ? 0
                    : spanMm(rear,navDir,(uint16_t)(markersSinceConfirmed+LOST_FRONT_MARGIN_MARKERS));

  // Rough ground speed from the last segment, for a follower estimating how
  // fast the envelope is growing. Diagnostic quality only.
  uint32_t mmPerSec = 0;
  if(lastSegmentDt>0 && navDir!=MAP_UNSET)
    mmPerSec = (uint32_t)spanMm(routeMod((int32_t)navMm-navDir),navDir,1)*1000UL/lastSegmentDt;

  // candidate_mm: the leading QUORUM candidate's implied position while one
  // exists (EVALUATING / NO_QUORUM), else -1. NOTE: "mm" fields carry marker
  // indices — "mm" in this codebase means MILE MARKER, not millimetres.
  int candMm = -1;
  if(leaderIdx>=0 && navState!=NAV_NORMAL && navState!=NAV_UNSET && navDir!=MAP_UNSET)
    candMm = (int)routeMod((int32_t)navMm + navDir*QUORUM_OFFSETS[leaderIdx]);
  // §2.5 step 3: the list of viable candidate offsets — not a computed
  // occupancy bound, which is M5.
  char vb[48]; jsonViable(vb,sizeof(vb));

  // F4 (CODEX finding 4) — the alert must fit PubMsg::payload (512).
  // Compacted names (none read by the dashboard, which keeps its bindings):
  //   motor_dir->mdir, rear_bound_mm->rear, front_bound_mm->front,
  //   envelope_mm->env, last_confirmed_mm->lc_mm,
  //   markers_since_confirmed->since, lost_markers->lostm, lost_count->losts.
  // est_mm_s is clamped to 99999 (5 chars) — a garbage dt could otherwise
  // print 10 digits of nonsense speed.
  //
  // Worst-case arithmetic, field by field ("name":value incl. quotes/colon):
  //   level:"EVALUATING" 20   reason:"SECOND_ADOPTION_FAILED" 33
  //   loco:"9950011" 16       uptime_ms:u32 22    nav:"EVALUATING" 18
  //   moving 10  pwm 9        est_mm_s:99999 16   dir:"UNSET" 13
  //   session_dir:"UNSET" 21  mdir:"NEU" 12       rear 10  front 11
  //   env:u32 16  lc_mm 11    last_confirmed_landmark:"Southpoint" 38
  //   age_ms:u32 19  since:65535 13  dead_reckoned_mm 22  candidate_mm 18
  //   viable:[-1,0,1,2,3,4] 23  lostm:65535 13  lost_ms:u32 20
  //   agree:u32 18  disagree:u32 21  losts:u32 18  auto 8
  //   + 26 commas + 2 braces = 497, + NUL = 498 <= 512 (headroom 14).
  if(mmPerSec>99999UL) mmPerSec=99999UL;
  int w=snprintf(b,sizeof(b),
    "{\"level\":\"%s\",\"reason\":\"%s\",\"loco\":\"%s\",\"uptime_ms\":%lu,"
    "\"nav\":\"%s\",\"moving\":%d,\"pwm\":%d,\"est_mm_s\":%lu,"
    "\"dir\":\"%s\",\"session_dir\":\"%s\",\"mdir\":\"%s\","
    "\"rear\":%u,\"front\":%u,\"env\":%lu,"
    "\"lc_mm\":%d,\"last_confirmed_landmark\":\"%s\",\"age_ms\":%lu,"
    "\"since\":%u,\"dead_reckoned_mm\":%u,"
    "\"candidate_mm\":%d,\"viable\":%s,\"lostm\":%u,\"lost_ms\":%lu,"
    "\"agree\":%lu,\"disagree\":%lu,\"losts\":%lu,\"auto\":%d}",
    level,reason,LOCO_NAME,(unsigned long)millis(),
    navStateName(),
    (actualPwm>0)?1:0,(int)actualPwm,(unsigned long)mmPerSec,
    dirName(navDir),dirName(sessionDir),
    motorDirection==DIRECTION_FORWARD?"FWD":(motorDirection==DIRECTION_REVERSE?"REV":"NEU"),
    rear,front,(unsigned long)envelope,
    haveConfirmed?(int)lastConfirmedMm:-1,
    haveConfirmed?landmarkAt(lastConfirmedMm):"",
    (unsigned long)(haveConfirmed?(millis()-lastConfirmedMs):0UL),
    (unsigned)markersSinceConfirmed,navMm,
    candMm,vb,
    (unsigned)lostMarkers,
    (unsigned long)(navState==NAV_NO_QUORUM?(millis()-lostSinceMs):0UL),
    navAgree,navDisagree,navLostCount,autoRunning?1:0);
  // Never enqueue truncated JSON: an oversize build (should be impossible by
  // the arithmetic above) publishes a minimal record naming the alert type.
  if(w>=(int)sizeof(PubMsg::payload)){
    Serial.printf("[ALERT] OVERSIZE %d bytes, level=%s\n",w,level);
    snprintf(b,sizeof(b),"{\"level\":\"%s\",\"reason\":\"ALERT_OVERSIZE\"}",level);
  }

  pub(T_ALERT,b,true);          // retained: a late subscriber still learns of it
  Serial.printf("[ALERT] %s\n",b);
}

// Every locomotive broadcasts its occupancy bound once a second, ALWAYS --
// not only when in trouble. A peer cannot decide whether it is safe to keep
// moving unless the others are continuously saying where they are.
static void serviceStatusBroadcast(){
  if(millis()-lastAlertMs < STATUS_BROADCAST_MS) return;
  publishAlert(navAlertLevel(),"STATUS");   // §6.1: one helper, both sites
}

static uint8_t startIntervalA=0, startIntervalB=0;
static bool     haveStartInterval=false;

// The current start_interval string ("AAA-BBB" or "000-000"), written by both
// the on-change publisher and the connect-time republish.
static void formatStartInterval(char* out,size_t n){
  if(haveStartInterval) snprintf(out,n,"%03u-%03u",startIntervalA,startIntervalB);
  else                  snprintf(out,n,"000-000");
}

// ---------------------------------------------------------------------------
// CHANGE 3 (v2.20) — DASHBOARD STATE IS PUBLISHED ON CHANGE, RETAINED.
//
// v2.19's publishSimpleStates() emitted ten messages every second regardless of
// whether anything moved. That is what saturated pubQueue on 2026-07-30 and left
// the E-stop unread in a socket buffer. Now each topic is published only when its
// value changes, with the retain flag set, so a subscriber -- including a
// dashboard refreshed mid-run -- gets the current value from the broker on
// connect. The per-second broadcast was standing in for exactly that.
//
// The 1000 ms gate below stays, as a CEILING on values that change fast:
// start_mm ticks every marker and throttle every ramp step, and neither needs
// sub-second resolution on a status topic.
//
// RETAINED-STATE HAZARD: these retained values OUTLIVE the locomotive -- the
// broker serves the last one to any late subscriber even after the loco is off.
// The retained `online` flag, driven to "0" by the MQTT last will, is what makes
// them interpretable: any consumer MUST treat all of this state as stale when
// online is 0.
// ---------------------------------------------------------------------------

// Publish an int state topic retained, but only when it differs from last time.
// `*last` starts at -1, which none of these states ever takes (all are >=0), so
// the first call after boot publishes once.
static void pubStateIntChanged(const char* t,int v,int* last){
  if(v==*last) return;
  *last=v; char b[12]; snprintf(b,sizeof(b),"%d",v); pub(t,b,true);
}
// Same, for a string state topic.
static void pubStateStrChanged(const char* t,const char* v,char* last,size_t lastSz){
  if(!strcmp(v,last)) return;
  strlcpy(last,v,lastSz); pub(t,v,true);
}

// ===========================================================================
// INA219 — v1.7, decision 0012. EVIDENCE ONLY.
// ===========================================================================
// This is a SENSOR, not an authority. Nothing here writes PWM, requests a
// stop, or gates a command, in either chamber. state/lowvolt is a published
// FACT; converting it into an action is a separate decision that must answer
// the bicameral question (0002) first — r12's habit of letting a telemetry
// value reach into control is exactly what is NOT being restored.
//
// Threading: I2C is touched ONLY from the loop thread — ina219Setup() in
// setup(), serviceInaTelemetry() and drainMarkers() in loop(). hallTask (the
// navigation-critical one) and networkTask never call Wire, so a slow or
// faulted I2C transaction cannot delay marker detection or MQTT service.
//
// A missing or faulted sensor is a first-class case, not an error path:
// begin() failing leaves ina219Available false, the service returns
// immediately, the marker line reports "v":null, and the locomotive runs
// exactly as it did in 1.6. Otto's open INA219 fault lives here until the
// hardware is repaired.
static Adafruit_INA219 ina219;
static bool          ina219Available=false;
static unsigned long lastInaTelemMs=0;
static float         lastBusVoltageV=0.0f, lastCurrentA=0.0f, lastPowerW=0.0f;
static bool          lastLowVolt=false;
// A sensor that is PRESENT but has not been read yet is not 0.00 V. The first
// service pass is up to INA219_TELEM_INTERVAL_MS after boot, and markers can
// be detected in that window (a locomotive can be pushed, or driven, straight
// off the bench). Without this the §9 table would be seeded with a handful of
// intervals normalised against a zero denominator.
static bool          inaHaveReading=false;

static void ina219Setup(){
  Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN);
  ina219Available = ina219.begin();
  Serial.printf("[INA] %s\n", ina219Available ? "ready" : "unavailable — telemetry disabled, run unaffected");
}

// Bus voltage for the marker line, and whether there is one to report. The
// value is the most recent 5 s sample, not a per-event read: a blocking I2C
// transaction inside hallTask — the one task that must never be late — was
// rejected outright, and §9 bins a whole segment, so a sample a few seconds
// either side of the magnet is well inside the resolution that matters.
static bool  inaVoltageKnown(){ return ina219Available && inaHaveReading; }
static float inaBusVoltage()  { return lastBusVoltageV; }

static void serviceInaTelemetry(){
  if(!ina219Available) return;
  unsigned long now=millis();
  if(now-lastInaTelemMs < INA219_TELEM_INTERVAL_MS) return;
  lastInaTelemMs=now;
  lastBusVoltageV = ina219.getBusVoltage_V();
  lastCurrentA    = ina219.getCurrent_mA()/1000.0f;
  lastPowerW      = ina219.getPower_mW()/1000.0f;
  inaHaveReading  = true;
  char b[24];
  // Retained, as in r12: the console's power tile is a current-value display,
  // and a retained publish every 5 s is what overwrites the stale ghosts the
  // broker has been serving since SOLONAV dropped the channel.
  snprintf(b,sizeof(b),"%.2f",lastBusVoltageV); pub(T_TELEM_V,b,true);
  snprintf(b,sizeof(b),"%.2f",lastCurrentA);    pub(T_TELEM_A,b,true);
  snprintf(b,sizeof(b),"%.2f",lastPowerW);      pub(T_TELEM_W,b,true);
  // The 0.1 V floor is r12's: a disconnected or unpowered sensor reads ~0,
  // and calling that "low battery" would raise the flag on hardware that is
  // simply absent. Published on CHANGE only, so the flag is an event.
  bool low = (lastBusVoltageV > 0.1f && lastBusVoltageV < LOW_VOLTAGE_THRESHOLD_V);
  if(low!=lastLowVolt){
    lastLowVolt=low;
    snprintf(b,sizeof(b),"%d",low?1:0); pub(T_ST_LOWVOLT,b,true);
  }
}

// Last-published values for the eight changing states. Ints seeded to -1 (an
// impossible value) so each publishes once at boot; strings seeded empty.
static int  lastThrottle=-1,lastDirection=-1,lastEstop=-1,
            lastAuto=-1,lastStartMm=-1,lastNavReady=-1;
static char lastSessDir[8]="", lastStartInt[12]="";

static unsigned long lastSimpleMs=0;
static void publishSimpleStates(){
  unsigned long now=millis();
  if(now-lastSimpleMs < 1000UL) return;   // ceiling on the fast-changing values
  lastSimpleMs=now;
  pubStateIntChanged(T_ST_THROTTLE ,commandedPwm,      &lastThrottle);
  pubStateIntChanged(T_ST_DIRECTION,motorDirection,    &lastDirection);
  pubStateIntChanged(T_ST_ESTOP    ,estopped?1:0,      &lastEstop);
  pubStateIntChanged(T_ST_AUTO     ,autoEnrolled?1:0,  &lastAuto);
  pubStateStrChanged(T_ST_SESSDIR  ,dirName(sessionDir),lastSessDir,sizeof(lastSessDir));
  { char si[12]; formatStartInterval(si,sizeof(si));
    pubStateStrChanged(T_ST_STARTINT,si,lastStartInt,sizeof(lastStartInt)); }
  pubStateIntChanged(T_ST_STARTMM  ,navMm,             &lastStartMm);
  // The console unlocks the throttle on this: a declared direction and a
  // position it can name. Usable includes EVALUATING (§6.1).
  pubStateIntChanged(T_ST_NAVREADY ,(sessionDir!=MAP_UNSET && navPositionUsable())?1:0,&lastNavReady);
}

// CHANGE 3c (v2.20) — republish ALL ten state topics, retained, on every
// successful MQTT connect (called from attemptReconnect right after online=1).
// Ten writes per reconnect instead of ten per second, and it reseeds a broker
// that restarted without persisting its retained store. The two inert channels
// (brake, must_hold_eligible) live ONLY here now -- one retained zero at connect
// rather than the 86,400 a day v2.19 sent for hardware that does not exist.
static void publishAllStatesRetained(){
  char b[12];
  snprintf(b,sizeof(b),"%d",commandedPwm);     pub(T_ST_THROTTLE ,b,true);
  snprintf(b,sizeof(b),"%d",motorDirection);   pub(T_ST_DIRECTION,b,true);
  pub(T_ST_BRAKE,"0",true);                     // no brake channel in SOLONAV
  snprintf(b,sizeof(b),"%d",estopped?1:0);     pub(T_ST_ESTOP    ,b,true);
  snprintf(b,sizeof(b),"%d",autoEnrolled?1:0); pub(T_ST_AUTO     ,b,true);
  pub(T_ST_SESSDIR,dirName(sessionDir),true);
  { char si[12]; formatStartInterval(si,sizeof(si)); pub(T_ST_STARTINT,si,true); }
  snprintf(b,sizeof(b),"%d",navMm);            pub(T_ST_STARTMM  ,b,true);
  pub(T_ST_MHE,"0",true);                       // no CTO hold eligibility yet
  snprintf(b,sizeof(b),"%d",(sessionDir!=MAP_UNSET && navPositionUsable())?1:0);
  pub(T_ST_NAVREADY,b,true);
  // v1.7: lowvolt joins the reseed so a broker that restarted (or one holding
  // a ghost from the r12 era) is corrected on every connect rather than
  // waiting for the next threshold CROSSING, which may never come. The three
  // telem topics are not reseeded here — they are measurements, and
  // serviceInaTelemetry republishes them within 5 s of the link returning.
  //
  // ONLY when a sensor has actually answered. Publishing lowvolt=0 from a
  // locomotive with no working INA219 would state "voltage is fine" on the
  // authority of nothing — the unknown-reported-as-clear inversion CTO3 §0
  // and §14 name as the one durable lesson of the CTO2 failure. With no
  // sensor the topic is left as it is: absent, or visibly stale. That is a
  // reason to look, which is what it should be. Otto, with its open INA219
  // fault, is exactly this case today.
  if(ina219Available){ snprintf(b,sizeof(b),"%d",lastLowVolt?1:0); pub(T_ST_LOWVOLT,b,true); }
}

static unsigned long loopMaxGapMs=0, lastStatMs=0;

// Change 4 — turn "did connect stall" and "did we come close to overflowing"
// into numbers. mqttConnectMs is the duration of the LAST mqtt.connect() call;
// mqttAttempts is the running count of connect attempts since boot; both
// persist across loopstat windows. queueHighWater is the max queue occupancy
// seen at drainMarkers() entry within the current loopstat window, reset on
// publish like loop_max_gap_ms -- so a normal lap reads low single digits and a
// stall spikes it.
static volatile unsigned long mqttConnectMs=0;
static uint32_t               mqttAttempts=0;
static uint16_t               queueHighWater=0;

static void publishStat(){
  unsigned long now=millis();
  if(now-lastStatMs<1000UL) return;
  lastStatMs=now;
  // Change 4 (v2.19) — pub_drops is cumulative; pub_queue_hw is the windowed
  // max occupancy of the outbound queue, reset below like loop_max_gap_ms. The
  // whole test of this version is that loop_max_gap_ms stays tiny even with the
  // broker down; pub_drops/pub_queue_hw show the network backing up behind the
  // queue while the loop keeps running.
  // Change 4 (v2.20). cmd_drops: cumulative inbound commands lost to a full
  // cmdQueue -- must never be silent. pub_per_s: publish calls in the window just
  // ended, read before the reset below (the T_STAT publish that follows counts
  // toward the next window). Expected ~12/s before Change 3, ~1/s parked after.
  uint32_t pubPerS=pubWindowCount;
  char b[512];
  snprintf(b,sizeof(b),
    "{\"loop_max_gap_ms\":%lu,\"hall_task_max_gap_ms\":%lu,\"hall_task_age_ms\":%lu,"
    "\"baseline\":%d,\"raw\":%d,\"delta\":%d,\"queue_drops\":%lu,\"floor_rejects\":%lu,"
    "\"queue_high_water\":%u,\"mqtt_connect_ms\":%lu,\"mqtt_attempts\":%lu,"
    "\"pub_drops\":%lu,\"pub_queue_hw\":%u,\"cmd_drops\":%lu,\"pub_per_s\":%lu,"
    "\"marker_pub_drops\":%lu,\"marker_pub_hw\":%u,"
    "\"nav\":\"%s\",\"mm\":%u,\"miss_streak\":%u,\"pwm\":%d,"
    "\"lost_markers\":%u,\"lost_ms\":%lu,\"motor_dir\":\"%s\"}",
    loopMaxGapMs,(unsigned long)taskMaxGapMs,(unsigned long)(now-taskLastRunMs),
    baselineCounts,(int)lastRaw,(int)lastRaw-baselineCounts,
    queueDrops,floorRejects,
    (unsigned)queueHighWater,(unsigned long)mqttConnectMs,(unsigned long)mqttAttempts,
    (unsigned long)pubDrops,(unsigned)pubQueueHw,(unsigned long)cmdDrops,(unsigned long)pubPerS,
    (unsigned long)markerPubDrops,(unsigned)markerPubHw,
    navStateName(),
    navMm,(unsigned)missStreak,(int)actualPwm,
    (unsigned)lostMarkers,
    (unsigned long)(navState==NAV_NO_QUORUM?(now-lostSinceMs):0UL),
    motorDirection==DIRECTION_FORWARD?"FWD":(motorDirection==DIRECTION_REVERSE?"REV":"NEU"));
  pub(T_STAT,b);
  loopMaxGapMs=0; taskMaxGapMs=0; queueHighWater=0; pubQueueHw=0; markerPubHw=0; pubWindowCount=0;
}

static void publishBootId(){
  char b[320];
  // conf_max is gone with navConfidence; the QUORUM constants are the
  // navigator identity now.
  snprintf(b,sizeof(b),
    "{\"sketch\":\"%s\",\"loco\":\"%s\",\"deadband\":%d,\"entry_margin\":%d,"
    "\"min_peak\":%d,\"floor_ms\":%lu,\"q_floor_ms\":%u,\"baseline\":\"median_%d_at_%lums\","
    "\"quorum_trigger\":%d,\"quorum_margin\":%d,\"quorum_max\":%d}",
    SKETCH_NAME,LOCO_NAME,(int)HALL_DEADBAND_COUNTS,(int)HALL_ENTRY_MARGIN_COUNTS,
    (int)HALL_MIN_PEAK_DELTA,EVENT_FLOOR_MS,(unsigned)Q_FLOOR_MS,MEDIAN_WINDOW,MEDIAN_SAMPLE_MS,
    QUORUM_TRIGGER,QUORUM_MARGIN,QUORUM_MAX);
  pub(T_BOOT,b,true);          // retained: kills the stale-identity ghost
}

// ===========================================================================
// COMMANDS
// ===========================================================================
// Runs on the LOOP thread (via serviceCommands), so every handler below still
// touches navMm, commandedPwm, sessionDir and the station machine on the thread
// that owns them -- exactly as when onMqtt ran inside loop(). The only change is
// where it is called from; the handler bodies are unchanged. This is why no
// mutex is needed once MQTT moves to its own task.
static void handleCommand(const char* topic,const char* msg){
  if(!strcmp(topic,T_CMD_SESSDIR)){
    // Same movement guard as cmd/direction: 2_1 flipped the pin here with no
    // check at all, so a manually reversing loco could be thrown over under
    // power. (Codex)
    // F4 (CODEX review of 1.4): cmd/direction refuses under AUTO but this did
    // not, and during a station DWELL both PWM values are zero — so a raw MQTT
    // session_direction was accepted mid-session, reset the station machine and
    // left autoRunning true, after which the idle branch could request cruise
    // and end the dwell early. The console blocks it; the broker did not.
    if(autoRunning){ stationPublish("SESSION_DIR_REFUSED",0,"AUTO_IN_CONTROL"); return; }
    // Anything unparseable used to become MAP_UNSET, which left the navigator
    // TRACKING with no direction: nextMm(mm,MAP_UNSET) returns mm, so the
    // odometer silently stopped advancing and judged every further reading
    // against the same marker. Refuse instead of accepting a broken state.
    int8_t req = (!strcasecmp(msg,"CW"))?MAP_CW:((!strcasecmp(msg,"CCW"))?MAP_CCW:MAP_UNSET);
    if(req==MAP_UNSET){ stationPublish("SESSION_DIR_REFUSED",0,"INVALID_MUST_BE_CW_OR_CCW"); return; }
    // Same pin, same hazard, same rule as cmd/direction (v1.6).
    if(!operatorDirectionPermitted("SESSION_DIR")) return;
    sessionDir = req;
    applyOperatorDirection(DIRECTION_FORWARD, "SESSION_DIR");
    stationReset("SESSION_DIRECTION_SET");
    navPublishState("SESSION_DIRECTION",nullptr);
    // Operator ruling 2026-08-14: complete a held declaration now that the
    // direction is known. Which end of the interval the locomotive occupies
    // depends on the direction, which is exactly why this had to wait — but
    // waiting is the firmware's job, not the operator's.
    if(havePendingInterval && navDir!=MAP_UNSET){
      const uint8_t a=pendingIntervalA, b=pendingIntervalB;
      havePendingInterval=false;
      uint8_t behind; bool ok=true;
      if(nextMm(a,MAP_CW)==b)       behind = (navDir==MAP_CW)?a:b;
      else if(nextMm(b,MAP_CW)==a)  behind = (navDir==MAP_CW)?b:a;
      else { ok=false; behind=0; }
      if(!ok){
        stationPublish("START_INTERVAL_REFUSED",0,"MARKERS_NOT_ADJACENT");
      }else{
        startIntervalA=a; startIntervalB=b; haveStartInterval=true;
        navDeclare(behind);
        stationPublish("START_INTERVAL_SET",0,
                       (navDir==MAP_CW)?"CW_DECLARED_AT_LOWER_DEFERRED"
                                       :"CCW_DECLARED_AT_UPPER_DEFERRED");
      }
    }
  }
  else if(!strcmp(topic,T_CMD_STARTINT)){
    // "AAA-BBB" — the two magnets the locomotive is standing between. This is
    // a GEOMETRIC interval, always ascending (085-086), because that is what
    // the console's slider produces and what the operator can see on the
    // ground. It is NOT a travel-order pair.
    //
    // 2_14 required travel order, so it demanded 086-085 when running CCW and
    // rejected every interval the console is capable of sending. The start
    // interval was unusable in one direction, which is why CCW sessions could
    // not be launched at all.
    //
    // Which end the locomotive is leaving depends on which way it faces:
    //     CW  : next marker is B, so position is declared at A
    //     CCW : next marker is A, so position is declared at B
    int a=-1,b=-1;
    if(sscanf(msg,"%d-%d",&a,&b)!=2){
      stationPublish("START_INTERVAL_REFUSED",0,"FORMAT_MUST_BE_AAA-BBB"); return;
    }
    if(a<0||a>=DNA_N||b<0||b>=DNA_N){
      stationPublish("START_INTERVAL_REFUSED",0,"MARKER_OUT_OF_RANGE"); return;
    }
    // Operator ruling 2026-08-14: THE ORDER MUST NOT MATTER. This used to
    // refuse outright when the direction was not yet set, which punished the
    // operator for sending two correct commands one second apart in the
    // "wrong" sequence — and on 2026-08-14 it silently cost a session: Toby's
    // interval arrived one second before his direction, was refused, and the
    // downstream BEGIN then reported NOT_ENROLLED_IN_AUTO, two steps removed
    // from the cause. Otto got the same pair in the opposite order and worked
    // first time.
    //
    // Now the interval is REMEMBERED and declared the moment the direction
    // arrives (see T_CMD_SESSDIR). Nothing is lost and nothing is refused; the
    // declaration simply completes when it has everything it needs.
    if(navDir==MAP_UNSET){
      pendingIntervalA=(uint8_t)a; pendingIntervalB=(uint8_t)b; havePendingInterval=true;
      stationPublish("START_INTERVAL_PENDING",0,"HELD_UNTIL_SESSION_DIRECTION_SET");
      return;
    }
    // Adjacency is checked geometrically, in either order, so the console's
    // ascending pair is accepted whichever way the session runs.
    uint8_t behind;
    if(nextMm((uint8_t)a,MAP_CW)==(uint8_t)b)       behind = (navDir==MAP_CW)?(uint8_t)a:(uint8_t)b;
    else if(nextMm((uint8_t)b,MAP_CW)==(uint8_t)a)  behind = (navDir==MAP_CW)?(uint8_t)b:(uint8_t)a;
    else { stationPublish("START_INTERVAL_REFUSED",0,"MARKERS_NOT_ADJACENT"); return; }

    startIntervalA=(uint8_t)a; startIntervalB=(uint8_t)b; haveStartInterval=true;
    navDeclare(behind);
    stationPublish("START_INTERVAL_SET",0,
                   (navDir==MAP_CW)?"CW_DECLARED_AT_LOWER":"CCW_DECLARED_AT_UPPER");
  }
  else if(!strcmp(topic,T_CMD_STARTMM)){
    int mm=atoi(msg);
    if(mm>=0 && mm<DNA_N && navDir!=MAP_UNSET) navDeclare((uint8_t)mm);
  }
  else if(!strcmp(topic,T_CMD_AUTO)){
    if(atoi(msg)!=0){
      // P11 (rulings R12/R15/H2; console-authority spec Draft 5.1):
      // ENLISTMENT GUARDS. Enlistment is the crossing into the AUTO chamber
      // and must not be casual — a manual operator has few rules, auto
      // operations have many. The rule lives where the truth lives: these
      // refusals hold whatever the command source (console, phone, second
      // tab, future voice). Each names its reason on state/station, using
      // the same vocabulary as the BEGIN gates.
      //
      // Order: motion first (safety), then setup. The energisation test
      // proves DE-ENERGISED, not physical rest — a pushed or coasting
      // locomotive passes it; the honest physical-stop witness is decision
      // 0005's motion witness (recorded residual).
      if(motorIsMoving())                 { stationPublish("ENLIST_REFUSED",0,"WAIT_FOR_STOP"); return; }
      if(sessionDir==MAP_UNSET)           { stationPublish("ENLIST_REFUSED",0,"NO_SESSION_DIRECTION"); return; }
      // H2: NEUTRAL passes the energisation guard but BEGIN refuses it
      // forever while the DIRECTION control is withdrawn — the trap the
      // BEGIN-gate invariant (spec §8) exists to catch.
      if(motorDirection==DIRECTION_NEUTRAL){ stationPublish("ENLIST_REFUSED",0,"NEUTRAL_SELECT_DIRECTION"); return; }
      // CODEX-C3: the two navigation refusal states keep their distinct
      // GO-vocabulary reasons; EVALUATING remains usable (position is held
      // and probably correct), matching the existing BEGIN contract.
      if(navState==NAV_UNSET)             { stationPublish("ENLIST_REFUSED",0,"NO_POSITION_DECLARE_START_MM"); return; }
      if(navState==NAV_NO_QUORUM)         { stationPublish("ENLIST_REFUSED",0,"NO_QUORUM_DECLARE_POSITION"); return; }
      autoEnrolled=true;
      stationReset("AUTO_CHANGED");
    }else{
      // INVARIANT (Claude-G3): cmd/auto 0 is NEVER refused. Disenrollment
      // is a safety action — it zeroes PWM and returns the locomotive to
      // the operator. A guard written as "refuse cmd/auto while moving"
      // would trap a rolling enlisted locomotive in AUTO; this branch must
      // stay unconditional.
      autoEnrolled=false; autoRunning=false; requestPwm(0,NORMAL_STEP_MS);
      stationReset("AUTO_CHANGED");
    }
  }
  else if(!strcmp(topic,T_CMD_GO)){
    // E-stop must be cleared explicitly. GO is not an implicit reset.
    if(estopped){ stationPublish("GO_REFUSED",0,"ESTOP_ACTIVE"); return; }
    // GO forces direction to FORWARD, so it must not run while the loco is
    // already rolling manually in reverse. Same guard as cmd/direction and
    // session_direction; this was the one path still missing it.
    if(motorIsMoving()){ stationPublish("GO_REFUSED",0,"WAIT_FOR_STOP"); return; }
    if(motorDirection==DIRECTION_NEUTRAL){ stationPublish("GO_REFUSED",0,"NEUTRAL_SELECT_DIRECTION"); return; }
    // Every refusal says why. This one used to fall through silently: a lost
    // loco could be sent GO all night and neither move nor explain itself.
    if(!autoEnrolled){ stationPublish("GO_REFUSED",0,"NOT_ENROLLED_IN_AUTO"); return; }
    // §6.1: refuse on NAV_NO_QUORUM and NAV_UNSET; ALLOW on NAV_EVALUATING —
    // position is held and probably correct while evaluating.
    if(navState==NAV_NO_QUORUM){ stationPublish("GO_REFUSED",0,"NO_QUORUM_DECLARE_POSITION"); return; }
    if(navState==NAV_UNSET){ stationPublish("GO_REFUSED",0,"NO_POSITION_DECLARE_START_MM"); return; }
    if(navDir==MAP_UNSET){ stationPublish("GO_REFUSED",0,"NO_SESSION_DIRECTION"); return; }
    if(autoRunning){ stationPublish("GO_REFUSED",0,"ALREADY_RUNNING"); return; }
    // Operator ruling 2026-08-14: IF BEGIN IS ACCEPTED, THE LOCOMOTIVE MOVES.
    // On 2026-08-14 BEGIN was accepted and published "GO / LAUNCH" while the
    // CTO limiter capped the request to zero five milliseconds later — the
    // locomotives sat still, the console said they had launched, and nothing
    // said why. Silent acceptance that produces no movement is worse than a
    // refusal: it tells the operator the railway is running when it is not.
    //
    // So the CTO cap is evaluated BEFORE committing. If it would hold this
    // locomotive at zero, BEGIN is REFUSED and says which condition did it.
    // Nothing here caps or moves anything — ctoLimitPwm() is a pure function
    // of peer state — and every one of these conditions clears by itself when
    // the railway allows movement, so BEGIN then simply works.
    {
      const int want = cruiseForPosition();
      if(ctoLimitPwm(want) <= 0){
        stationPublish("GO_REFUSED",0,ctoRefusalReason());
        return;
      }
      autoRunning=true;
      motorDirection=DIRECTION_FORWARD; applyDirection();
      requestPwm(want,NORMAL_STEP_MS);
      stationPublish("GO",0,"LAUNCH");
    }
  }
  else if(!strcmp(topic,T_CMD_STOP)){
    // CHAMBER BOUNDARY (v1.6): the dispatcher may stop an AUTO session. It
    // may NOT reach into MANUAL. Without this test, a dispatcher STOP zeroed
    // the throttle of a locomotive that had never enlisted — the auto side
    // driving the manual side, which is the one thing the two chambers must
    // never do to each other. Enlistment is the locomotive's own act; absent
    // it, this command is not addressed to us.
    if(!autoEnrolled){ stationPublish("STOP_IGNORED",0,"NOT_ENLISTED_IN_AUTO"); return; }
    autoRunning=false; requestPwm(0,NORMAL_STEP_MS);
    stationReset("DISPATCHER_STOP");
  }
  else if(!strcmp(topic,T_CMD_DIR)){
    // AUTO owns the motor once the locomotive has enlisted; otherwise the
    // operator's command is obeyed wherever the hardware allows it (v1.6).
    if(autoRunning){ stationPublish("DIR_REFUSED",0,"AUTO_IN_CONTROL"); return; }
    applyOperatorDirection(constrain(atoi(msg),0,2), "DIR");
  }
  else if(!strcmp(topic,T_CMD_RELEASE)){
    // END CTO — hand the locomotive back to the operator. Drops AUTO
    // enrolment as well as AUTO running, so the console's manual controls
    // ungrey and a stray GO cannot restart it.
    autoRunning=false; autoEnrolled=false;
    requestPwm(0,NORMAL_STEP_MS);
    ceEnd("DISPATCHER_RELEASE");   // a mission must not outlive the session
    stationReset("DISPATCHER_RELEASE");
    publishWarning("RELEASED: dispatcher control ended");
    navPublishState("DISPATCHER_RELEASE",nullptr);
  }
  else if(!strcmp(topic,T_CMD_CE)){
    // Bicameral: a mission is allowed from the dispatcher in either chamber
    // (BUBBLE_V1_SPEC §9). It changes service pattern only -- it cannot start
    // a stopped locomotive, cannot clear an E-stop, and cannot raise any cap
    // the traffic layer has applied.
    if(atoi(msg)!=0) ceBegin();
  }
  else if(!strcmp(topic,T_CMD_ESTOP) || !strcmp(topic,T_CMD_ESTOP_ALL)){
    estopped=(atoi(msg)!=0);
    // P14 (operator ruling R13; console-authority spec Draft 5): E-STOP no
    // longer touches DIRECTION on EITHER branch. The interlock is `estopped`
    // itself — servicePwmRamp() clamps PWM to zero every pass while it is
    // set, and BEGIN is separately refused with ESTOP_ACTIVE — so the old
    // NEUTRAL writes were belt-and-braces, not the protection. Preserving
    // DIRECTION is what lets the dispatcher restart an enlisted, E-stopped
    // locomotive with BEGIN alone (the loco-page DIRECTION control is
    // withdrawn while enlisted, and the dispatcher console has none).
    // Motion after a clear still requires a deliberate act: a throttle
    // advance in MANUAL (the console zeroes its slider on E-STOP, spec P5)
    // or BEGIN in AUTO. CODEX-C1/Claude-H1: the Draft-4 version of this
    // change removed only the clear-path write, which was worthless — the
    // assert path had already forced NEUTRAL before the clear arrived.
    if(!estopped){
      navPublishState("ESTOP_CLEARED",nullptr);   // direction preserved (P14)
    }
    if(estopped){
      autoRunning=false; commandedPwm=0; actualPwm=0; pwmWriteCompat(0);
      // The loco has stopped somewhere unplanned; a half-finished station
      // approach must not resume against it after E-stop is cleared. (Codex)
      stationReset("ESTOP");
    }
  }
  else if(!strcmp(topic,T_CMD_THROTTLE)){
    if(!autoRunning && !estopped) requestPwm(atoi(msg),NORMAL_STEP_MS);   // manual only
  }
  else if(!strcmp(topic,T_CMD_CTO)){
    // LAYER 5 operator command. "clear" empties the peer registry, dissolves
    // any pair and disarms the fleet stop — the LBO CMD_CLEAR_ALL lesson: a
    // removed locomotive must not hold the survivors hostage. "off"/"on"
    // gate the whole layer. Anything else is refused loudly.
    ctoHandleClear(msg);
  }
  else if(!strcmp(topic,T_CMD_FORCELOST)){
    // TEST FIXTURE (§6.5). Topic name kept for existing scripts. Payload is a
    // signed integer n: displace navMm by n event-steps and change NOTHING
    // else — the locomotive does not know it has been moved, and discovers
    // the error the way it would a real one. n=-4 reproduces a queue-drop
    // burst; n=+1 a phantom. Payload "NOQUORUM" forces the terminal state
    // directly, for testing the snapshot. Every rejection publishes
    // FIXTURE_REJECTED with the offending payload — a fixture that fails
    // silently is worse than one that does not exist. atoi() is NOT used: it
    // returns 0 for garbage, which would silently become offset 0.
    const char* p=msg;
    while(*p==' '||*p=='\t') p++;                 // trim leading whitespace
    size_t len=strlen(p);
    while(len && (p[len-1]==' '||p[len-1]=='\t'||p[len-1]=='\r'||p[len-1]=='\n')) len--;
    char pay[32];
    if(len>=sizeof(pay)) len=sizeof(pay)-1;
    memcpy(pay,p,len); pay[len]=0;
    char extra[80];
    if(pay[0]==0){
      publishQuorumDecision("FIXTURE_REJECTED",",\"payload\":\"\",\"why\":\"EMPTY\"");
      return;
    }
    if(!strcmp(pay,"NOQUORUM")){                  // exact, case-sensitive, before numeric
      // Permitted from NAV_UNSET: a state fixture, not a position operation.
      enterNoQuorum("FORCED_BY_FIXTURE");
      return;
    }
    char* endp=nullptr;
    long n=strtol(pay,&endp,10);
    if(endp==pay || *endp!=0 || n<-8 || n>8){
      snprintf(extra,sizeof(extra),",\"payload\":\"%s\",\"why\":\"PARSE_OR_RANGE\"",pay);
      publishQuorumDecision("FIXTURE_REJECTED",extra);
      return;
    }
    if(navState==NAV_UNSET || navDir==MAP_UNSET){ // no position/direction to displace
      snprintf(extra,sizeof(extra),",\"payload\":\"%s\",\"why\":\"NO_POSITION_OR_DIRECTION\"",pay);
      publishQuorumDecision("FIXTURE_REJECTED",extra);
      return;
    }
    uint8_t oldMm=navMm;
    navMm=routeMod((int32_t)navMm + navDir*(int32_t)n);
    // change NOTHING else — not navState, not missStreak, not the ring, not
    // the scores. NAV_EVALUATING is entered later, by ordinary disagreement.
    snprintf(extra,sizeof(extra),",\"n\":%ld,\"old_mm\":%u,\"new_mm\":%u",n,oldMm,navMm);
    publishQuorumDecision("FORCED_OFFSET",extra);
  }
}

static void subscribeAll(){
  mqtt.subscribe(T_CMD_AUTO);     mqtt.subscribe(T_CMD_GO);
  mqtt.subscribe(T_CMD_STOP);     mqtt.subscribe(T_CMD_DIR);
  mqtt.subscribe(T_CMD_SESSDIR);  mqtt.subscribe(T_CMD_STARTMM);
  mqtt.subscribe(T_CMD_ESTOP);    mqtt.subscribe(T_CMD_ESTOP_ALL);
  mqtt.subscribe(T_CMD_CE);
  mqtt.subscribe(T_CMD_THROTTLE);
  mqtt.subscribe(T_CMD_STARTINT);
  mqtt.subscribe(T_CMD_RELEASE);
  mqtt.subscribe(T_CMD_FORCELOST);
  mqtt.subscribe(T_CMD_CTO);
}

// The MQTT callback runs on the NETWORK task. It must not touch locomotive
// state, so it only copies the command onto cmdQueue and returns; loop()'s
// serviceCommands() runs the actual handler on the thread that owns the state.
// Dropped if full -- commands are rare, and a backed-up command queue would mean
// loop() itself is wedged, which is exactly what this change prevents.
static void onMqttEnqueue(char* topic,byte* payload,unsigned int len){
  CmdMsg c;
  strlcpy(c.topic,topic,sizeof(c.topic));
  unsigned n=min(len,(unsigned)(sizeof(c.payload)-1));
  memcpy(c.payload,payload,n); c.payload[n]=0;

  // CHANGE 1 (v2.20) — ENGAGING E-stop must not depend on the command queue.
  // servicePwmRamp() clamps the motor to zero every loop() pass while `estopped`
  // is set, so raising the flag HERE -- on the network task, before anything is
  // enqueued -- stops the locomotive within ~35 ms even if cmdQueue is full and
  // the queued handler below is lost. This is the single most important change in
  // this version: on 2026-07-30 the E-stop sat unread and Otto could not be
  // stopped. Only ENGAGING bypasses; CLEARING stays queued (below) so resuming
  // motion is deliberate and runs the full handler.
  if((!strcmp(c.topic,T_CMD_ESTOP) || !strcmp(c.topic,T_CMD_ESTOP_ALL)) && atoi(c.payload)!=0) estopped=true;

  // Fall through and ALSO enqueue, so the full handler still runs (NEUTRAL,
  // autoRunning=false, stationReset, the alert). If this send is dropped the
  // locomotive has still stopped. CHANGE 4: count the loss -- v2.19 ignored the
  // send result, so a dropped command was silent.
  if(cmdQueue && xQueueSend(cmdQueue,&c,0)!=pdTRUE) cmdDrops++;
}

// Drained on the LOOP thread, symmetric with drainMarkers(). Runs the existing
// command handlers unchanged, so all locomotive state stays owned by loop().
static void serviceCommands(){
  CmdMsg c;
  while(cmdQueue && xQueueReceive(cmdQueue,&c,0)==pdTRUE)
    handleCommand(c.topic,c.payload);
}

// Reconnect, throttled to one attempt per 5 s. Behaviour is unchanged from the
// v2.18 serviceNetwork(): time the connect, publish online/boot/alert, and
// resubscribe. It runs ONLY on networkTask, so mqtt.connect() blocking here can
// no longer stall loop(). (The online/boot/alert publishes go through pub(),
// i.e. onto pubQueue, and are flushed by networkTask on its next pass.)
static unsigned long nextMqttTryMs=0;
// 0.3a: WiFi and MQTT connection CONFIRMATION, logged on every transition —
// not just the boot-time snapshot above. This is what makes a live drop
// (the broker's "exceeded timeout, disconnecting") visible on serial at
// all; previously only a SUCCESSFUL reconnect printed anything.
static bool wifiWasUp=false;
static void logWifiTransition(bool up){
  if(up==wifiWasUp) return;
  wifiWasUp=up;
  if(up) Serial.printf("[NET] WiFi UP ip=%s rssi=%d ch=%d\n",
           WiFi.localIP().toString().c_str(),WiFi.RSSI(),WiFi.channel());
  else   Serial.println("[NET] WiFi DOWN");
}
static void attemptReconnect(){
  unsigned long now=millis();
  if(now<nextMqttTryMs) return;
  nextMqttTryMs=now+5000UL;
  unsigned long t0=millis();
  mqttAttempts++;
  bool ok=mqtt.connect(LOCO_NAME,T_ONLINE,0,true,"0");
  mqttConnectMs=millis()-t0;
  if(!ok){
    // mqtt.state(): PubSubClient's own reason code, -4..-1 = client/transport
    // side (no TCP reach, socket lost, refused, or plain not-connected-yet),
    // 1..5 = the BROKER rejected the CONNECT (bad protocol, id, credentials,
    // server unavailable, not authorized) — a materially different failure
    // than "never reached the broker at all", and until now indistinguishable
    // from serial output.
    Serial.printf("[NET] MQTT connect FAILED state=%d wifi=%d rssi=%d (%lu ms)\n",
      mqtt.state(),(int)WiFi.status(),WiFi.RSSI(),mqttConnectMs);
  }
  if(ok){
    pub(T_ONLINE,"1",true);
    publishAllStatesRetained();        // Change 3c: reseed all ten state topics
    publishBootId();
    publishAlert(navAlertLevel(),"MQTT_CONNECT");   // §6.1: mid-evaluation reconnect must not report CLEAR
    subscribeAll();                    // mqtt.subscribe; reached only from here
    // F3: every successful reconnect re-arms reconciliation, so the broker
    // (which may have restarted without its retained store) is re-synced to
    // the persistent desired state of mm/no_quorum. Under the mux (v1.2, per
    // CODEX) for consistency with every other writer of the slot.
    portENTER_CRITICAL(&noQuorumMux);
    noQuorumNeedsReconcile=true;
    portEXIT_CRITICAL(&noQuorumMux);
    Serial.println("[NET] MQTT connected");
  }
}

// The network task owns the radio exclusively: it is the ONLY place mqtt.loop()
// and mqtt.publish() run, and it calls attemptReconnect() (mqtt.connect/
// subscribe). Pinned to core 0 alongside the WiFi stack at priority 1 -- below
// hallTask's 2, so magnet sampling always wins. 8192 stack: PubSubClient plus
// WiFi needs more than hallTask's 4096.
static void networkTask(void*){
  for(;;){
    logWifiTransition(WiFi.status()==WL_CONNECTED);
    if(WiFi.status()==WL_CONNECTED){
      // LAYER 5: ESP-NOW must init after the STA has a channel; once, guarded.
      // Runs on the network task — esp_now_init/register are its only calls
      // here, and the recv callback only ever copies into ctoRxQueue.
      ctoRadioInit();   // self-guarded; returns immediately once up
      if(!mqtt.connected()){
        attemptReconnect();            // may block; that is now harmless
      }else{
        // CHANGE 2 (v2.20) — inbound FIRST, every pass, and the outbound drain is
        // BOUNDED. v2.19 read inbound once, then drained up to 32 blocking
        // publishes through a degraded socket before reading again; on 2026-07-30
        // that left cmd/estop unread while the queue stayed pinned full. At most 4
        // publishes per pass with a 5 ms tick gives ~200 inbound polls/s that
        // outbound congestion can no longer starve.
        //
        // Do NOT raise the 4 to clear a backlog faster: a persistent backlog means
        // the link cannot carry the traffic, and the fix for that is publish-on-
        // change (Change 3), not a bigger gulp that re-creates the starvation.
        mqtt.loop();
        // §2.5 desired-retained-state reconciliation — the ONLY publisher of
        // mm/no_quorum. In no queue, so routine telemetry can neither
        // overwrite nor evict it. F3: success clears only the per-connection
        // reconcile flag — NEVER the desired state — and attemptReconnect()
        // re-arms the flag, so the broker is re-synchronized to the desired
        // state after every outage, indefinitely. F2: state and snapshot are
        // copied out under the mux into a task-local buffer; the mux is never
        // held across mqtt.publish().
        if(noQuorumNeedsReconcile){
          static char snapCopy[512];
          portENTER_CRITICAL(&noQuorumMux);
          uint8_t  want=desiredRetainedNoQuorum;
          uint32_t gen =noQuorumGeneration;      // v1.2: copied WITH the state
          if(want==DRS_SNAPSHOT) memcpy(snapCopy,noQuorumSnapshot,sizeof(snapCopy));
          portEXIT_CRITICAL(&noQuorumMux);
          bool ok;
          if(want==DRS_NONE)          ok=true;   // nothing owed to the broker
          else if(want==DRS_SNAPSHOT) ok=mqtt.publish(T_NO_QUORUM,snapCopy,true);
          else                        ok=mqtt.publish(T_NO_QUORUM,"",true);   // empty retained = clear
          if(ok){
            portENTER_CRITICAL(&noQuorumMux);
            // v1.2 (CODEX 1.1 review): the enum value alone misses a NEWER
            // commit with the SAME value — snapshot B landing while snapshot
            // A publishes, or SNAPSHOT->CLEAR->SNAPSHOT during a slow
            // publish. Reconciliation completes only if the state AND the
            // generation both still match what was copied out above.
            if(desiredRetainedNoQuorum==want && noQuorumGeneration==gen)
              noQuorumNeedsReconcile=false;
            portEXIT_CRITICAL(&noQuorumMux);
          }
        }
        PubMsg m;
        // CHANGE F1/F2 (v2.22, CODEX R19 findings 3+4) — markers drain FIRST via
        // PEEK-PUBLISH-REMOVE, capped at 8. v2.21 removed the marker from the
        // queue and then ignored whether mqtt.publish() succeeded, so a publish
        // that failed locally deleted the evidence with marker_pub_drops still 0
        // -- the exact signature of this morning's outage test, where markers 24
        // and 23 vanished at the seam. Now the marker leaves the queue ONLY after
        // mqtt.publish() reports success; a local failure leaves it queued, stops
        // this pass's drain, and retries next pass. QoS 0 still cannot prove
        // broker receipt (the socket-buffer seam at outage onset can still eat
        // ~2 markers), but every locally DETECTABLE failure now retries instead
        // of silently discarding.
        //
        // The cap of 8 replaces v2.21's uncapped drain: 64 sequential socket
        // writes on a degraded link could hold this task away from mqtt.loop(),
        // delaying inbound E-stop reception. 8 per pass clears a full 64-marker
        // backlog in 8 passes of this ~200 Hz task while bounding the gap
        // between mqtt.loop() calls. Order unchanged: inbound first, markers
        // before status, status keeps its cap of 4.
        uint8_t nm=0;
        while(nm<8 && xQueuePeek(markerPubQueue,&m,0)==pdTRUE){
          if(!mqtt.publish(m.topic,m.payload,false)) break;
              // publish failed locally: leave the marker queued,
              // stop draining this pass, retry next pass
          xQueueReceive(markerPubQueue,&m,0);   // remove ONLY after success
          nm++;
        }
        // Status keeps its bounded drain (Change 2, v2.20): at most 4 per pass so
        // a congested outbound queue cannot starve inbound mqtt.loop().
        uint8_t n=0;
        while(n<4 && xQueueReceive(pubQueue,&m,0)==pdTRUE){
          mqtt.publish(m.topic,m.payload,m.retain);
          n++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ===========================================================================
// ===========================================================================
// TEMPLATES 0.3 — R3 TARGET ACQUISITION: the identity test (LOOP thread)
// ---------------------------------------------------------------------------
// docs/TEMPLATES_REVISION_3_TARGET_ACQUISITION.md; decision 0044.
//
// Governing change: stop asking "does this clear a universal floor" and start
// asking "does this match the specific target we are already looking for."
// The design hierarchy (operator + CODEX, 2026-08-27):
//   1. Dead reckoning predicts where the locomotive is and when the next
//      landmark should arrive.
//   2. The map supplies the expected magnet's polarity, relative strength,
//      morphology (amplitude + duration), and surrounding sequence.
//   3. Hall and IR observations are compared with that expected landmark.
//   4. QUORUM pattern recognition checks whether the continuing sequence
//      supports the identification.
//   5. Agreement confirms and advances position.
//   6. A mismatch causes consideration of missed magnets and alternative
//      positions (bounded, forward-only — direction is operator-declared
//      and does not drift; §3.2 of the spec).
//   7. If no likely mapped candidate exists, hold or reject the signal and
//      preserve only its diagnostic record.
//   8. Later sequence evidence may correct position, but it NEVER converts
//      a rejected signal into a magnet.
// A landmark is identified as a member of a mapped magnet sequence.
// Individual attributes are evidence about an expected identity, not
// universal admission requirements.
//
// DIVISION OF LABOUR. The hallTask gate above is now the MAGNET TEST only —
// a permissive spike/noise filter (SIGNAL_REJECTED, terminal, audit-only
// afterlife, exactly as before). THIS section is the IDENTITY TEST: it runs
// on the loop thread, where navMm/navDir/gain/history natively live, between
// the event queue and navOnMarker(). Three outcomes (spec §9):
//   TARGET_CONFIRMED  — matches the expected marker; hand to the navigator,
//                       which advances and compares exactly as today.
//   R3 CORRECTION     — a farther candidate positively identifies at or
//                       above R3_CORRECT_PCT: adopt the missed markers, then
//                       hand to the navigator. Forward-only, bounded.
//   MAGNET_UNRESOLVED — magnet-like but not confidently assignable. HELD:
//                       never reaches the navigator, never advances
//                       position, is never reconsidered — but its COUNT
//                       seeds the sequence attribute so a later confirmed
//                       passage can close the gap (the MM140-142 offset-lag
//                       failure this revision exists to fix). This is NOT a
//                       purgatory: nothing held is ever resurrected.
//
// WHEN R3 STANDS ASIDE. The identity test runs only in NAV_NORMAL with a
// known direction, in sync with the navigator, with no adoption validation
// pending and no quarantined event awaiting arbitration. In every other
// regime (EVALUATING, NO_QUORUM, §3Q arbitration, post-adoption validation)
// events pass straight through: QUORUM's recovery machinery owns those
// regimes and R3 must not withhold its evidence (spec §3.4).
//
// EVERY VALUE HERE IS AN EXPERIMENTAL TEST SETTING, not an approved safety
// boundary (operator, 2026-08-27). Thresholds and weights are deliberately
// flat #defines so field results can retune them without restructuring.
// ===========================================================================
#ifdef TEMPLATES_ADMISSION

// Per-marker expected duration at the PWM-90 reference, ms. Built from 2,210
// confirmed crossings on 2026-08-27 (Toby; sessions 121418 CW-partial,
// 123552 CCW, cw_full CW; all 171 markers covered, >=6 samples each, median
// per-marker CV 0.049). Construction mirrors strengthPct[]: observed ms is
// normalized as ms*pwm/90, per-marker median taken. HONEST LIMIT: one
// locomotive, one day, tested against its own derivation session — this has
// NOT had strengthPct[]'s cross-locomotive validation. Ships static (spec
// §12 open question resolved for the first build: static, like
// strengthPct[]; calibration establishes session GAIN, it does not rewrite
// tables — spec §7).
static const uint16_t durationMs90[DNA_N] PROGMEM = {
  138, 186, 158, 150, 149, 145, 153, 147, 148, 146,  // MM000-MM009
  139, 150, 155, 208, 252, 179, 165, 145, 150, 157,  // MM010-MM019
  159, 172, 154, 148, 150, 147, 148, 175, 169, 166,  // MM020-MM029
  185, 165, 158, 159, 147, 136, 133, 137, 146, 143,  // MM030-MM039
  142, 153, 138, 146, 141, 146, 144, 141, 140, 147,  // MM040-MM049
  140, 129, 140, 134, 165, 136, 139, 144, 151, 184,  // MM050-MM059
  210, 199, 141, 187, 182, 131, 170, 135, 138, 137,  // MM060-MM069
  127, 123, 131, 127, 123, 121, 129, 123, 127, 140,  // MM070-MM079
  137, 136, 141, 143, 150, 149, 155, 152, 160, 156,  // MM080-MM089
  161, 153, 168, 153, 147, 145, 145, 149, 146, 169,  // MM090-MM099
  176, 168, 189, 174, 180, 256, 176, 163, 149, 151,  // MM100-MM109
  145, 187, 159, 145, 144, 142, 142, 144, 140, 147,  // MM110-MM119
  153, 146, 145, 141, 142, 148, 148, 146, 147, 167,  // MM120-MM129
  167, 166, 152, 148, 151, 153, 148, 147, 154, 151,  // MM130-MM139
  147, 148, 144, 142, 143, 142, 139, 142, 149, 153,  // MM140-MM149
  156, 149, 155, 148, 156, 195, 224, 148, 157, 149,  // MM150-MM159
  145, 147, 169, 155, 151, 151, 169, 148, 155, 141,  // MM160-MM169
  140,  // MM170-MM170
};
static inline uint16_t durationAt(uint8_t mm){ return pgm_read_word(&durationMs90[mm%DNA_N]); }

// ---- weights and thresholds — EXPERIMENTAL TEST SETTINGS (spec §6) --------
// The weights sum to 100 for readability only; the denominator is always the
// sum of the AVAILABLE attributes' weights, so absence reduces the evidence
// pool and never counts against a candidate (spec §10).
#define R3_W_POL   32   // polarity: dnaAt(cand) — the strongest single bit
#define R3_W_STR   18   // strength: session gain x strengthAt(cand)
#define R3_W_DUR   18   // duration: PWM-normalized vs durationAt(cand)
#define R3_W_TIM   16   // timing: dt vs span/velocity (Hall+PWM chain)
#define R3_W_SEQ    8   // sequence: does the unresolved run fill the gap
#define R3_W_IR     8   // IR odometry: the independent movement witness
#define R3_CONFIRM_PCT 50   // confirm the EXPECTED target at/above this
#define R3_CORRECT_PCT 67   // adopt a DIFFERENT position at/above this
                            // (operator: "a guess. A starting point. If it
                            // lets spurious signals through, it is too low.")
// Closeness tolerances: score decays linearly from 100 at exact match to 0
// at this fractional deviation. Timing is loosest (ramps, grade, stiction).
#define R3_TOL_STR 0.50f
#define R3_TOL_DUR 0.50f
#define R3_TOL_TIM 0.60f
#define R3_TOL_IR  0.50f
#define R3_MISS_SEARCH     3    // candidates past the expected marker, minimum
#define R3_CAND_MAX        6    // hard cap on candidates scored per event
#define R3_UNRESOLVED_CAP 12    // streak counter ceiling (diagnostic sanity)
#define R3_IR_FRESH_MS  1500UL  // IR witness must be at most this stale
// Velocity sanity band, mm/s: outside it the estimate is discarded (a
// station dwell inside the measuring interval, or a crawl below the model's
// validity). Fleet p99.9 max is 441 mm/s; 600 leaves margin without
// admitting nonsense.
#define R3_VEL_MIN_MMPS   50.0f
#define R3_VEL_MAX_MMPS  600.0f
// If elapsed time exceeds every candidate's expectation by this factor, the
// locomotive dwelt or crawled between markers and elapsed time measures
// waiting, not travel: the timing attribute goes ABSENT for the event
// (absent, never contradicting — spec §10). IR is immune to this by
// construction: a stopped unpowered wheel accrues no pulses, so IR distance
// stays truthful across a dwell. That asymmetry is why IR is worth carrying.
#define R3_DWELL_ABSENT_FACTOR 3.0f
// Physical envelope VETO for candidates (not a weighted attribute): the same
// 800 mm/s bound Q_FLOOR_MS derives from (fleet p99.9 max 441 mm/s, ~1.8x
// margin). A candidate whose implied speed exceeds it is excluded outright.
#define R3_MAX_MMPS 800.0f

// ---- state ----------------------------------------------------------------
static uint16_t r3UnresolvedStreak=0;
static uint8_t  r3LastAcceptMm=0;          // navMm after the last accepted event
static bool     r3Synced=false;            // false until first acceptance, and
                                           // after any external relocation
static unsigned long r3LastAcceptMs=0;
static float    r3VelMmPerMs=0.0f;
static bool     r3VelValid=false;
static uint32_t r3IrPulseSnap=0;
static bool     r3IrSnapValid=false;
static uint32_t r3Confirmed=0, r3Unresolved=0, r3CorrectedN=0, r3Bypassed=0;
static uint32_t r3NavRefused=0;   // 0.3B: proposals the navigator refused

// Linear closeness: 100 at obs==exp, 0 at fractional deviation >= tol.
static int r3Closeness(float obs, float exp, float tol){
  if(exp<=0.0f) return -1;
  float dev=fabsf(obs-exp)/exp;
  if(dev>=tol) return 0;
  return (int)(100.0f*(1.0f-dev/tol)+0.5f);
}

// Score ONE candidate marker against the observed passage. off = number of
// mapped markers between navMm's successor and the candidate (0 = the
// expected next marker; off missed markers if the candidate is real).
static void r3ScoreCandidate(const MarkerEvent& e, uint8_t candMm, uint8_t off,
                             unsigned long dtAcc, bool timAbsent,
                             bool irAvail, float irDistMm, R3Score& sc){
  uint32_t num=0; uint16_t den=0;
  sc.mapSpanMm = spanMm(navMm,navDir,(uint16_t)off+1);

  // Polarity — always available. Deliberately NOT a veto: a wrong map bit or
  // a marginal read is outvoted only by a strong conjunction elsewhere, and
  // the navigator downstream still publishes its own AGREE/DISAGREE.
  sc.sPol = (dnaAt(candMm)==e.polarity) ? 100 : 0;
  num += (uint32_t)R3_W_POL*(uint32_t)sc.sPol; den += R3_W_POL;

  // Strength — needs the session gain (trailing median of accepted peaks,
  // the same gain §3Q already uses). Warmed by the calibration lap (§7).
  sc.expPeak=0; sc.sStr=-1;
  if(qHistLen>=8){
    uint16_t gain=qMedian(qPkHist,qHistLen);
    sc.expPeak=(uint16_t)(((uint32_t)gain*(uint32_t)strengthAt(candMm))/100u);
    sc.sStr=r3Closeness((float)e.peak,(float)sc.expPeak,R3_TOL_STR);
  }
  if(sc.sStr>=0){ num+=(uint32_t)R3_W_STR*(uint32_t)sc.sStr; den+=R3_W_STR; }

  // Duration — PWM-normalized against the per-marker table. This is the
  // measure that separated today's weak-but-real magnets (ratios 0.83-1.03)
  // from genuine noise (0.09-0.14) — spec §2.4.
  sc.expDur=durationAt(candMm); sc.durN90=0; sc.sDur=-1;
  if(e.pwmActualAtDetect>0 && sc.expDur>0){
    uint32_t n90=((uint32_t)e.durationMs*(uint32_t)e.pwmActualAtDetect)/90u;
    sc.durN90=(n90>65535u)?65535u:(uint16_t)n90;
    sc.sDur=r3Closeness((float)sc.durN90,(float)sc.expDur,R3_TOL_DUR);
  }
  if(sc.sDur>=0){ num+=(uint32_t)R3_W_DUR*(uint32_t)sc.sDur; den+=R3_W_DUR; }

  // Timing — elapsed since the last ACCEPTED event vs map distance over the
  // measured velocity. Correlated with the Hall/PWM chain; provenance is
  // published, not hidden (spec §10.1).
  sc.expDt=0; sc.sTim=-1;
  if(!timAbsent && r3VelValid && dtAcc>0){
    sc.expDt=(uint32_t)((float)sc.mapSpanMm/r3VelMmPerMs);
    sc.sTim=r3Closeness((float)dtAcc,(float)sc.expDt,R3_TOL_TIM);
  }
  if(sc.sTim>=0){ num+=(uint32_t)R3_W_TIM*(uint32_t)sc.sTim; den+=R3_W_TIM; }

  // Sequence — does the run of held (magnet-like, unassigned) passages since
  // the last acceptance exactly fill the gap this candidate implies? off ==
  // streak: every held passage was one of the missed markers — the
  // MM140-142 signature. off < streak: some held passages were noise (half
  // credit). off > streak: the candidate claims silent misses beyond what
  // was seen (no credit, but not vetoed — magnets can genuinely miss).
  sc.sSeq = (off==r3UnresolvedStreak)?100:((off<r3UnresolvedStreak)?50:0);
  num += (uint32_t)R3_W_SEQ*(uint32_t)sc.sSeq; den += R3_W_SEQ;

  // IR — pulses on the unpowered wheel since the last acceptance, as track
  // distance. The only witness independent of the Hall/PWM chain.
  sc.sIr=-1;
  if(irAvail){
    sc.sIr=r3Closeness(irDistMm,(float)sc.mapSpanMm,R3_TOL_IR);
    if(sc.sIr>=0){ num+=(uint32_t)R3_W_IR*(uint32_t)sc.sIr; den+=R3_W_IR; }
  }

  sc.wAvail=den;
  sc.conf=(uint8_t)(num/(uint32_t)den);   // den >= R3_W_POL+R3_W_SEQ always
}

// §10: every decision must be reconstructible offline without inference.
// 0.3B: ONE publish per identity decision, AFTER the navigator has ruled, so
// the record carries BOTH halves — R3's proposal and the committed outcome.
// The 512-byte PubMsg bound forces short keys (same doctrine as IR §9.3's
// key-shortening); fixed mapping, schema stable:
//   t=detect ms  prop=proposed outcome  nav=navigator disposition
//   cm=committed 0/1  mm=navMm at proposal  mmf=final navMm
//   exp/dec=expected/decided marker  off=decided offset
//   pol/pole=observed/expected polarity  pk/epk=peak obs/expected
//   du/dn/edu=duration raw/normalized/expected  dt/edt=elapsed/expected ms
//   map=map span mm  ir=IR distance mm or null
//   s=[pol,str,dur,tim,seq,ir] scores, -1 unavailable  w=avail denominator
//   cf/cfe=confidence decided/expected  cmin/xmin=confirm/correct thresholds
//   un=unresolved streak  ex=candidates vetoed physically unreachable
//   nc/nu/nx/nr/nb=counters confirmed/unresolved/corrections-committed/
//   navigator-refusals/bypassed
// (R3Proposal is declared with the early types — prototype-generator trap.)

static void r3PublishDecision(const R3Proposal& p, const MarkerEvent& e,
                              uint8_t navDisp){
  static char m[512];   // loop thread only
  char irBuf[16];
  if(p.irAvail) snprintf(irBuf,sizeof(irBuf),"%ld",(long)(p.irDistMm+0.5f));
  else          strlcpy(irBuf,"null",sizeof(irBuf));
  bool committed = (navDisp==NAV_D_ACCEPTED);
  snprintf(m,sizeof(m),
    "{\"t\":%lu,\"prop\":\"%s\",\"nav\":\"%s\",\"cm\":%u,"
    "\"mm\":%u,\"mmf\":%u,\"dir\":\"%s\",\"exp\":%u,\"dec\":%u,\"off\":%u,"
    "\"pol\":\"%c\",\"pole\":\"%c\",\"pk\":%d,\"epk\":%u,"
    "\"du\":%u,\"dn\":%u,\"edu\":%u,\"dt\":%lu,\"edt\":%lu,\"map\":%lu,\"ir\":%s,"
    "\"s\":[%d,%d,%d,%d,%d,%d],\"w\":%u,\"cf\":%u,\"cfe\":%u,"
    "\"cmin\":%u,\"xmin\":%u,\"un\":%u,\"ex\":%u,"
    "\"nc\":%lu,\"nu\":%lu,\"nx\":%lu,\"nr\":%lu,\"nb\":%lu}",
    (unsigned long)e.detectedAtMs, p.proposed, navDispName(navDisp),
    committed?1u:0u,
    p.mmAtProposal, navMm, dirName(navDir), p.expectMm, p.decidedMm,
    (unsigned)p.bestOff,
    polChar(e.polarity), dnaAt(p.decidedMm)?'N':'S', e.peak,
    (unsigned)p.dec.expPeak,
    (unsigned)e.durationMs, (unsigned)p.dec.durN90, (unsigned)p.dec.expDur,
    (unsigned long)p.dtAcc, (unsigned long)p.dec.expDt,
    (unsigned long)p.dec.mapSpanMm, irBuf,
    p.dec.sPol,p.dec.sStr,p.dec.sDur,p.dec.sTim,p.dec.sSeq,p.dec.sIr,
    (unsigned)p.dec.wAvail,(unsigned)p.dec.conf,(unsigned)p.confExp,
    (unsigned)R3_CONFIRM_PCT,(unsigned)R3_CORRECT_PCT,
    (unsigned)r3UnresolvedStreak,(unsigned)p.excludedN,
    (unsigned long)r3Confirmed,(unsigned long)r3Unresolved,
    (unsigned long)r3CorrectedN,(unsigned long)r3NavRefused,
    (unsigned long)r3Bypassed);
  pub(T_R3_ADMIT,m,false);
}

// The identity test — 0.3B: PROPOSES, never commits. navMm is not touched
// here (CODEX req 1); a correction rides the event into the ladder and lands
// only inside acceptEvent(), atomically with acceptance (req 3).
static void r3Evaluate(const MarkerEvent& e, R3Proposal& p){
  p.present=false; p.toNav=true; p.corrOff=0;
  // Stand aside wherever QUORUM's machinery owns the event (see header) —
  // and also when the shadow anchor disagrees with navMm: the navigator
  // moved position without an R3-witnessed acceptance (relocation,
  // quarantine commit), so dtAcc/IR spans measured from the shadow would be
  // wrong. The next ACCEPTED event resyncs the anchor.
  if(navState!=NAV_NORMAL || navDir==MAP_UNSET || !r3Synced
     || r3LastAcceptMm!=navMm
     || adoptionPendingValidation || qPendingValid){
    r3Bypassed++;
    return;
  }

  p.present=true;
  p.mmAtProposal=navMm;
  p.dtAcc = e.detectedAtMs - r3LastAcceptMs;

  // IR availability: snapshot valid, link fresh, counter monotonic.
  p.irAvail=false; p.irDistMm=0.0f;
#if IR_TEST_A_ON
  if(r3IrSnapValid && irHaveLatest
     && (millis()-irLatestRxMs)<=R3_IR_FRESH_MS
     && (int32_t)(irLastPulses-r3IrPulseSnap)>=0){
    p.irDistMm=(float)(irLastPulses-r3IrPulseSnap)*IR_MM_PER_PULSE;
    p.irAvail=true;
  }
#endif

  // Candidate set: the expected next marker plus a bounded forward search —
  // far enough to cover every held passage plus R3_MISS_SEARCH true misses.
  uint8_t nCand = 1 + (uint8_t)min((unsigned)(R3_MISS_SEARCH + r3UnresolvedStreak),
                                   (unsigned)(R3_CAND_MAX-1));

  // Timing globally ABSENT (not contradicting) when mid-ramp, or when
  // elapsed time exceeds even the farthest candidate's expectation by the
  // dwell factor — the interval then measures waiting, not travel.
  bool timAbsent = (abs((int)e.pwmActualAtDetect-(int)e.pwmCommandedAtDetect) > GATE_RAMP_DELTA)
                || (e.pwmActualAtDetect < GATE_LOW_PWM_FLOOR);
  if(!timAbsent && r3VelValid){
    float maxExp=(float)spanMm(navMm,navDir,nCand)/r3VelMmPerMs;
    if((float)p.dtAcc > R3_DWELL_ABSENT_FACTOR*maxExp) timAbsent=true;
  }

  // Score every candidate; VETO the physically unreachable ones outright.
  // The veto is the Q_FLOOR_MS doctrine applied to identity: a candidate
  // whose map distance demands more than R3_MAX_MMPS over the measured
  // elapsed time CANNOT be this passage, whatever its other attributes say.
  // Physical impossibility is decisive, never merely one weighted opinion
  // (operator question, 2026-08-27; the run log showed off=3 corrections
  // implying ~1,090 mm/s adopted at 76-83% confidence).
  R3Score sc[R3_CAND_MAX];
  bool excl[R3_CAND_MAX];
  uint8_t excludedN=0;
  int bestIdx=-1;
  uint8_t expectMm=nextMm(navMm,navDir);
  for(uint8_t j=0;j<nCand;j++){
    uint8_t candMm=routeMod((int32_t)navMm + (int32_t)navDir*(int32_t)(1+j));
    r3ScoreCandidate(e,candMm,j,p.dtAcc,timAbsent,p.irAvail,p.irDistMm,sc[j]);
    excl[j] = ((float)sc[j].mapSpanMm*1000.0f > R3_MAX_MMPS*(float)p.dtAcc);
    if(excl[j]){ excludedN++; continue; }
    if(bestIdx<0 || sc[j].conf>sc[bestIdx].conf) bestIdx=j;  // ties -> NEARER
  }
  p.excludedN=excludedN;
  p.expectMm=expectMm;
  p.confExp=excl[0]?0:sc[0].conf;

  // Decision order: correction authority first (it must BEAT the expected
  // candidate and clear its own, higher bar), then ordinary confirmation,
  // then hold. A strong-but-unexpected read is a warning, not automatic
  // confirmation (spec §1). All outcomes are PROPOSALS until the ladder
  // accepts; counters are committed in drainMarkers on the disposition.
  if(bestIdx>0 && sc[bestIdx].conf>=R3_CORRECT_PCT
     && sc[bestIdx].conf>p.confExp){
    p.proposed="R3_CORRECTED";
    p.bestOff=(uint8_t)bestIdx;
    p.corrOff=(uint8_t)bestIdx;
    p.decidedMm=routeMod((int32_t)navMm + (int32_t)navDir*(int32_t)(1+bestIdx));
    p.dec=sc[bestIdx];
    p.toNav=true;
    return;
  }
  if(bestIdx==0 && sc[0].conf>=R3_CONFIRM_PCT){
    p.proposed="TARGET_CONFIRMED";
    p.bestOff=0; p.corrOff=0;
    p.decidedMm=expectMm;
    p.dec=sc[0];
    p.toNav=true;
    return;
  }
  // MAGNET_UNRESOLVED — held. Never reaches the navigator, never advances
  // position, never reconsidered. Counted, so a later confirmed passage can
  // close the gap through the sequence attribute. Also the outcome when
  // EVERY candidate is physically unreachable (ex carries the count).
  p.proposed="MAGNET_UNRESOLVED";
  p.bestOff=(bestIdx>=0)?(uint8_t)bestIdx:0;
  p.corrOff=0;
  p.decidedMm=expectMm;
  p.dec=sc[0];
  p.toNav=false;
}

// Post-navigator bookkeeping — 0.3B: driven by the DISPOSITION, never by
// inference from navMm. Req 4: only an ACCEPTED event may move the shadow;
// a refusal (QUARANTINED / PHANTOM_REJECTED / NO_*) leaves the last
// confirmed anchor untouched, so repeated refusals can no longer accumulate
// drift — the regression the MM101-104 sequence exposed.
static void r3NoteAfterNav(const MarkerEvent& e, uint8_t navDisp){
  if(navDisp!=NAV_D_ACCEPTED) return;
  if(r3Synced){
    uint32_t span=0; uint8_t cur=r3LastAcceptMm; bool walked=false;
    for(uint8_t i=0;i<8;i++){
      uint8_t nxt=nextMm(cur,navDir);
      span += (navDir==MAP_CW) ? pgm_read_word(&spacingMm[cur])
                               : pgm_read_word(&spacingMm[nxt]);
      cur=nxt;
      if(cur==navMm){ walked=true; break; }
    }
    unsigned long dtAcc=e.detectedAtMs-r3LastAcceptMs;
    r3VelValid=false;
    if(walked && dtAcc>0 && navDir!=MAP_UNSET){
      float v=(float)span/(float)dtAcc;              // mm per ms
      if(v>=R3_VEL_MIN_MMPS/1000.0f && v<=R3_VEL_MAX_MMPS/1000.0f){
        r3VelMmPerMs=v; r3VelValid=true;
      }
    }
  }else{
    r3VelValid=false;
  }
  r3LastAcceptMm=navMm;
  r3LastAcceptMs=e.detectedAtMs;
  r3Synced=(navState==NAV_NORMAL||navState==NAV_EVALUATING) && navDir!=MAP_UNSET;
  r3UnresolvedStreak=0;   // a committed acceptance closes any open gap
#if IR_TEST_A_ON
  if(irHaveLatest){ r3IrPulseSnap=irLastPulses; r3IrSnapValid=true; }
  else r3IrSnapValid=false;
#endif
}
#endif  // TEMPLATES_ADMISSION — R3 identity test

static void drainMarkers(){
  MarkerEvent e;
  // Sample occupancy BEFORE draining: this is how deep the queue got while the
  // previous loop pass was busy (e.g. blocked in serviceNetwork). Windowed max,
  // reset by publishStat.
  if(eventQueue){
    UBaseType_t waiting=uxQueueMessagesWaiting(eventQueue);
    if(waiting>queueHighWater) queueHighWater=(uint16_t)waiting;
  }
#ifdef TEMPLATES_ADMISSION
  // TEMPLATES audit drain. Bounded at 4 per pass for the same reason the
  // status drain is: a pulse storm must not starve the control loop. These
  // records are diagnostic ONLY — nothing here touches navigation state, and
  // no rejected event can re-enter navigation by this or any other path.
  {
    AdmitReject r; uint8_t nr=0;
    while(rejectQueue && nr<4 && xQueueReceive(rejectQueue,&r,0)==pdTRUE){
      char m[192];
      snprintf(m,sizeof(m),
        "{\"t_ms\":%lu,\"cause\":\"%s\",\"pol\":\"%c\",\"peak\":%d,"
        "\"dur_ms\":%u,\"floor_ms\":%u,\"pwm\":%u,"
        "\"peak_rejects\":%lu,\"floor_rejects\":%lu,\"audit_drops\":%lu}",
        (unsigned long)r.tMs,
        r.cause==ADMIT_REJECT_CAUSE_PEAK?"AMPLITUDE":"DURATION",
        r.pole?'N':'S', (int)r.peak,
        (unsigned)r.durMs, (unsigned)r.floorMs, (unsigned)r.pwmActual,
        (unsigned long)peakRejects,(unsigned long)floorRejects,
        (unsigned long)rejectQueueDrops);
      pub(T_ADMIT_REJECT,m,false);
      nr++;
    }
  }
#endif
  while(eventQueue && xQueueReceive(eventQueue,&e,0)==pdTRUE){
#ifdef TEMPLATES_ADMISSION
    // TEMPLATES 0.3B: the R3 identity test PROPOSES here; the navigator
    // DISPOSES below, and only its disposition commits anything. A held
    // (MAGNET_UNRESOLVED) event is fully recorded on diag/r3_admit and goes
    // no further — it does not reach the navigator, IR TEST A observation,
    // or the mm/marker publish, and it is never reconsidered. A correction
    // proposal rides INTO the ladder and lands only inside acceptEvent(),
    // atomically with acceptance — navMm is untouched on any refusal.
    R3Proposal r3p;
    r3Evaluate(e,r3p);
    if(r3p.present && !r3p.toNav){
      if(r3UnresolvedStreak<R3_UNRESOLVED_CAP) r3UnresolvedStreak++;
      r3Unresolved++;
      r3PublishDecision(r3p,e,NAV_D_NOT_PRESENTED);
      continue;
    }
#endif
    // IR TEST A §8.1/§9.3: estimate before navOnMarker changes any state,
    // publish the revision-1 record after. Shared with the harness so the
    // replay exercises the same path (inert stubs on Otto).
    irObserveEventPre(e);
#ifdef TEMPLATES_ADMISSION
    NavDisposition navDisp = navOnMarker(e, r3p.present ? r3p.corrOff : 0);
#else
    navOnMarker(e,0);
#endif
    irObserveEventPost(e);
#ifdef TEMPLATES_ADMISSION
    if(r3p.present){
      // Counters record COMMITTED outcomes; a refusal is its own bucket and
      // the shadow does not move (req 4/5).
      if(navDisp==NAV_D_ACCEPTED){
        if(r3p.corrOff) r3CorrectedN++; else r3Confirmed++;
        r3UnresolvedStreak=0;
      }else{
        r3NavRefused++;
      }
      r3PublishDecision(r3p,e,navDisp);
    }
    r3NoteAfterNav(e,navDisp);   // disposition-driven: ACCEPTED only
#endif
    // §5.1 marker payload contract — the raw event fields plus dt,
    // timing_gate, dt_expected, dt_conserve_ratio. Scores, streaks, leaders
    // and margins still ride the QUORUM decision events, and conf is still
    // deleted with navConfidence.
    //
    // v1.7 AMENDS the contract with exactly two fields, for the CTO3 §9
    // segment x direction x PWM -> pKPH table (decision 0012):
    //   "pwm"  pwmActualAtDetect — the PWM at event OPEN, already carried on
    //          the event per §3, so this publishes a value the sketch had and
    //          was throwing away. The table cannot be keyed without it.
    //   "v"    bus voltage, or null when no INA219 answered at boot. §9's
    //          normalisation (table_pKPH x v_now/v_ref) needs the voltage at
    //          the locomotive at the time of the interval; a fraction without
    //          its denominator is not a measurement.
    // Everything a consumer needs to bin one interval is now on one line, so
    // the Pi-side parser never has to join two topics on a timestamp.
    //
    // Worst case recomputed: 174 + "pwm":255 (12) + "v":-13.20 (12) = 198
    // bytes, still inside 320 and inside the 512-byte PubMsg payload. The
    // v field is printed at %.2f from a float that is bounded by the INA219's
    // ~26 V range, so it cannot widen unexpectedly.
    float ratio = (lastDtConserveRatio>99.99f) ? 99.99f : lastDtConserveRatio;
    char v[12];
    // Absent sensor and not-yet-read sensor are both null, never 0.00 V.
    if(inaVoltageKnown()) snprintf(v,sizeof(v),"%.2f",inaBusVoltage());
    else                  strlcpy(v,"null",sizeof(v));
    char b[320];
    snprintf(b,sizeof(b),
      "{\"mm\":%u,\"landmark\":\"%s\",\"obs\":\"%c\",\"peak\":%d,\"ms\":%u,"
      "\"drift\":%d,\"dt\":%u,\"timing_gate\":\"%s\",\"dt_expected\":%lu,"
      "\"dt_conserve_ratio\":%.2f,\"pwm\":%u,\"v\":%s}",
      navMm,landmarkAt(navMm),polChar(e.polarity),e.peak,e.durationMs,
      e.baselineDrift,lastSegmentDt,lastTimingGate,
      (unsigned long)lastDtExpected,(double)ratio,
      (unsigned)e.pwmActualAtDetect,v);
    pubMarker(T_MARKER,b);   // dedicated queue; status can no longer evict it
  }
}

static void calibrate(){
  Serial.println("[CAL] 2 s baseline — keep clear of magnets");
  unsigned long t0=millis(); uint32_t sum=0,n=0;
  while(millis()-t0<CALIBRATION_MS){ sum+=readAveragedADC(); n++; delay(5); }
  baselineCounts = n?(int)(sum/n):readAveragedADC();
  primeMedian(baselineCounts);
  recomputeThresholds();
  Serial.printf("[CAL] baseline=%d  Nent=%d Nex=%d Sex=%d Sent=%d\n",
                baselineCounts,northEnter,northExit,southExit,southEnter);
}

// ===========================================================================
// LAYER 5 — CTO3 PEER COORDINATION  (docs/CTO3/BUBBLE_V1_SPEC.md)
// ---------------------------------------------------------------------------
// Two-train bubble operations: peer truth over ESP-NOW, derived latched
// roles, one deceleration profile with a resume flag, leader hold at
// stations, and the 0031 fleet stop enforced by absence.
//
// Constitutional position (§0.2): this layer NEVER writes the motor. It
// limits what the AUTO station machine may request (ctoLimitPwm), extends a
// dwell (ctoDwellMs), and that is all. In MANUAL it only
// broadcasts self-truth. E-stop and manual authority are untouched.
//
// The wire carries truth, never authority (decision 0032): the peer packet
// is the sender's own operational facts in the FROZEN CtoPeerPacket v3
// layout (docs/CLAUDE.md); the role echo is a separate new packet type that
// r12-era receivers ignore by magic mismatch. Nobody is told what to do.
//
// Every distance here is MARKERS along direction of travel — the control
// frame ruling. Bounds are producer-applied: hall marker composed with the
// consist extents from LL_LocoConfig (decisions 0030/0033). The navigation
// uncertainty term is deliberately absent in v1 and owed at M5 validation.
// ===========================================================================

// ---- geometry (markers, along travel; bounds only — decision 0033) --------
static inline uint8_t ctoMyFrontB(){ return routeMod((int32_t)navMm + navDir*CONSIST_EXTENT_FRONT_MARKERS); }
static inline uint8_t ctoMyRearB(){  return routeMod((int32_t)navMm - navDir*CONSIST_EXTENT_REAR_MARKERS); }
// forward arc a->b in my travel direction, 0..170
static inline uint16_t ctoArc(uint8_t a,uint8_t b){
  return (navDir==MAP_CW) ? routeMod((int32_t)b-(int32_t)a) : routeMod((int32_t)a-(int32_t)b);
}
// bound gap from MY front to PEER rear (peer ahead of me)
static inline uint16_t ctoGapAhead(const CtoPeer& p){ return ctoArc(ctoMyFrontB(),p.rearB); }
// bound gap from PEER front to MY rear (peer behind me)
static inline uint16_t ctoGapBehind(const CtoPeer& p){ return ctoArc(p.frontB,ctoMyRearB()); }

static inline bool ctoPeerFresh(const CtoPeer& p){
  return p.seen && (uint32_t)(millis()-p.rxMs)<=CTO_PEER_STALE_MS;
}
static inline bool ctoPeerNavOk(const CtoPeer& p){ return p.truthSource>=1; }
static inline bool ctoPeerSameDir(const CtoPeer& p){
  return navDir!=MAP_UNSET && p.mapDir==navDir;
}
// A peer counts as stopping/stopped for traffic purposes on POSITIVE evidence
// only: its own declared traffic phase, a station stop in progress, or a
// stationary broadcast. Silence is handled by freshness + fleet stop, never here.
static inline bool ctoPeerStopping(const CtoPeer& p){
  // CODEX 1.14 review, finding 2: a leader in ST_APPROACH/ST_FINAL is
  // already committed to stopping — waiting for ST_RAMP denied the follower
  // its 18 MM pre-slow for the whole approach. A falling ramp is intent too.
  return p.trafficPhase!=CTRAF_CLEAR ||
         (p.stationPhase>=ST_APPROACH && p.stationPhase<=ST_DWELL) ||
         (p.motionState==0 && p.rampPwm==0) ||
         p.rampFalling;
}

static int8_t ctoPeerIdx(uint32_t id){
  for(uint8_t i=0;i<CTO_MAX_PEERS;i++) if(ctoPeers[i].seen && ctoPeers[i].id==id) return (int8_t)i;
  return -1;
}
static int8_t ctoPartnerIdx(){ return ctoPartnerId?ctoPeerIdx(ctoPartnerId):-1; }

// ===========================================================================
// IR TEST A — Stage-A receiver (spec: QUORUM_1_16R_IR_TEST_A). OBSERVATION
// ONLY. Nothing in this section may call requestPwm/requestPwmOver, write
// motor/navigation/station/CTO state, or refuse an operator command (§3).
// The call-site audit in the implementation report enumerates every symbol
// this section touches.
// ===========================================================================
#if IR_TEST_A_ON

static void irEnsureQueue(){ if(!irRxQueue) irRxQueue=xQueueCreate(32,sizeof(IrRxItem)); }


static bool irSensorMacUsable(){
  bool z=true; for(int i=0;i<6;i++) if(IR_SENSOR_MAC[i]) z=false;
  return !z && !(IR_SENSOR_MAC[0]&0x01);   // §2: zero/broadcast/multicast disable
}

// Shared by the real callback and the test harness so replay exercises the
// EXACT validation chain, not a copy of it. Callback context: counters, copy,
// enqueue — nothing else (§5.1).
static void irAcceptFrame(const uint8_t* srcMac,const uint8_t* data,int len,uint32_t rxMs){
  if(len!=(int)sizeof(IrSpeedPacketV1)){
    if(len>0 && data[0]==IR_SPEED_MAGIC) irRejBadLen++;
    return;
  }
  IrSpeedPacketV1 p; memcpy(&p,data,sizeof(p));
  if(p.magic!=IR_SPEED_MAGIC || p.version!=IR_SPEED_VERSION){ irRejBadMagicVer++; return; }
  if(p.bytes!=sizeof(IrSpeedPacketV1) || p.reserved!=0){ irRejBadWire++; return; }
  if(!irSensorMacUsable() || memcmp(srcMac,IR_SENSOR_MAC,6)!=0){ irRejBadSource++; return; }
  if(p.sensorId!=IR_SENSOR_ID_IR01){ irRejBadSensor++; return; }
  if(p.targetLocoId!=LOCO_ID){ irRejBadTarget++; return; }
  if(p.validity>IR_V_MAX){ irRejBadEnum++; return; }
  // canonical encoding (§5.1 step 8): measured for VALID, exactly zero for
  // STOPPED, sentinel otherwise — so an invalid measurement can never be
  // mistaken for a measured stop (0036).
  bool ok = (p.validity==IR_V_VALID)   ? (p.speedMmpsX100!=IR_SPEED_INVALID_X100)
          : (p.validity==IR_V_STOPPED) ? (p.speedMmpsX100==0)
                                       : (p.speedMmpsX100==IR_SPEED_INVALID_X100);
  if(!ok){ irRejBadEncoding++; return; }
  IrRxItem item; item.p=p; memcpy(item.mac,srcMac,6); item.rxMs=rxMs;
  if(irRxQueue && xQueueSend(irRxQueue,&item,0)!=pdTRUE) irRxQueueDrops++;
}


static void irHistPush(const IrSnap& sn){
  uint8_t idx=(uint8_t)((irHistHead+irHistLen)%32);
  if(irHistLen<32) irHistLen++;
  else { idx=irHistHead; irHistHead=(uint8_t)((irHistHead+1)%32); }
  irHist[idx]=sn;
}
static const IrSnap* irHistAt(uint8_t i){ return &irHist[(uint8_t)((irHistHead+i)%32)]; }

static const IrEventEst* irEstFind(uint32_t detMs){
  for(uint8_t i=0;i<16;i++) if(irEstRing[i].detectedAtMs==detMs && detMs) return &irEstRing[i];
  return nullptr;
}


// Every observer reset in one place (§7): declaration, direction change,
// SELF_RESOLVED relabel, sensor reboot. Resets observation ONLY.
static void irObserversReset(const char*){
  mmAnchorValid=false; mmSpeedValid=false;
  irAnchorValid=false;
  irCovReset();   // coverage is not comparable across a declaration, a
                  // direction change, a relabel or a sensor reboot
  for(uint8_t i=0;i<16;i++) irEstRing[i]=IrEventEst{};
}

// route span helper: cumulative spacing over k steps from mm in dir.
static uint32_t irRouteSpanMm(uint8_t fromMm,int8_t dir,uint8_t k){
  uint32_t total=0; uint8_t cur=fromMm;
  for(uint8_t i=0;i<k;i++){
    // interval traversed when stepping cur -> next(cur,dir): CW leaving cur
    // crosses spacing[cur]; CCW crosses spacing[cur-1] (== spacing[next]).
    uint8_t next=nextMm(cur,dir);
    uint8_t idx=(dir==MAP_CW)?cur:next;
    total+=pgm_read_word(&spacingMm[idx]);
    cur=next;
  }
  return total;
}

// ---- §5.3: drain the queue, accept under sequence discipline ---------------
static void serviceIrRx(){
  if(!irRxQueue) return;
  IrRxItem it;
  while(xQueueReceive(irRxQueue,&it,0)==pdTRUE){
    const IrSpeedPacketV1& p=it.p;
    if(!irEpochOpen || p.bootId!=irBootId){
      // New sensor epoch (§5.3): history and distance anchoring die with the
      // old epoch; navigation state is untouched.
      if(irEpochOpen) irSensorReboots++;
      irEpochOpen=true; irBootId=p.bootId;
      irHistLen=0; irHistHead=0;
      irAnchorValid=false;
      irLastSeq=p.txSequence; irLastPulses=p.completedPulses;
    }else{
      int32_t ds=(int32_t)(p.txSequence-irLastSeq);   // wrap-safe
      if(ds==0){ irDups++; continue; }
      if(ds<0){ irOutOfOrder++; continue; }
      if(ds>1) irSeqGaps+=(uint32_t)(ds-1);
      if((int32_t)(p.completedPulses-irLastPulses)<0){
        // §5.3: a pulse regression inside one epoch is rejected and breaks
        // the distance anchor — cumulative counts only ever grow. BUT the
        // adversarial pass proved one canonical-but-wrong frame carrying a
        // huge count would wedge the epoch forever: every honest packet
        // after it reads as a regression against the poisoned baseline.
        // Three CONSECUTIVE regressions therefore rebase — the same
        // confirmation discipline as the channel watch — accepting the
        // incoming count as the new baseline with the anchor invalidated
        // and the rebase counted. One bad frame now costs three reports,
        // not the rest of the epoch.
        irPulseRegressions++; irAnchorValid=false;
        irLastSeq=p.txSequence;
        if(++irRegressStreak>=3){
          irRegressStreak=0; irPulseRebases++;
          irLastPulses=p.completedPulses;
          irHistLen=0; irHistHead=0;   // history predates the rebase
        }
        continue;
      }
      irRegressStreak=0;
      irLastSeq=p.txSequence; irLastPulses=p.completedPulses;
    }
    if(irLastAcceptRxMs){
      irRxGapMs=(uint32_t)(it.rxMs-irLastAcceptRxMs);
      if(irRxGapMs>irRxMaxGapMs) irRxMaxGapMs=irRxGapMs;
    }
    irLastAcceptRxMs=it.rxMs;
    irLatest=p; irLatestRxMs=it.rxMs; irHaveLatest=true;
    IrSnap sn; sn.rxMs=it.rxMs; sn.capturedMs=p.capturedMs; sn.pulses=p.completedPulses;
    sn.speedX100=p.speedMmpsX100; sn.seq=p.txSequence; sn.validity=p.validity;
    irHistPush(sn);
    irAccepted++;
  }
}

// ---- §6: freshness and speed truth -----------------------------------------
// Numeric only for a fresh, canonical VALID or STOPPED from the current
// epoch. Stale-but-valid packets are receiver state LINK_STALE — not sensor
// STALE, and never a current stopped-wheel claim.
static IrView irCurrentView(){
  IrView v{false,0.0f,"NO_SENSOR",0};
  if(!irHaveLatest) return v;
  v.ageMs=(uint32_t)(millis()-irLatestRxMs);
  bool wasNumeric=(irLatest.validity==IR_V_VALID||irLatest.validity==IR_V_STOPPED);
  if(v.ageMs>IR_LINK_STALE_MS){ v.state=wasNumeric?"LINK_STALE":irWireStateName(irLatest.validity); return v; }
  v.state=irWireStateName(irLatest.validity);
  if(wasNumeric){ v.numeric=true; v.mmps=(irLatest.validity==IR_V_STOPPED)?0.0f:(float)irLatest.speedMmpsX100/100.0f; }
  return v;
}
static const char* irWireStateName(uint8_t v){
  switch(v){
    case IR_V_VALID: return "VALID";
    case IR_V_STOPPED: return "STOPPED";
    case IR_V_MARGINAL: return "MARGINAL";
    case IR_V_REACQUIRING: return "REACQUIRING";
    case IR_V_STALE: return "STALE";
    default: return "INVALID_CONTRAST";
  }
}

// ---- §8.1: event-time IR estimate ------------------------------------------
// Latest accepted snapshot at or before the event, valid, within 150 ms;
// project cumulative distance forward at its own speed. Otherwise null —
// never force a fit, never delay Hall processing.
static IrEventEst irEstimateAt(uint32_t detMs){
  IrEventEst est; est.detectedAtMs=detMs; est.anchorWasValid=irAnchorValid;
  const IrSnap* best=nullptr;
  for(uint8_t i=0;i<irHistLen;i++){
    const IrSnap* sn=irHistAt(i);
    if((int32_t)(sn->rxMs-detMs)<=0 && (!best || (int32_t)(sn->rxMs-best->rxMs)>0)) best=sn;
  }
  if(!best) return est;
  uint32_t age=(uint32_t)(detMs-best->rxMs);
  if(age>IR_PROJECT_MAX_MS) return est;
  if(best->validity!=IR_V_VALID && best->validity!=IR_V_STOPPED) return est;
  float speed=(best->validity==IR_V_STOPPED)?0.0f:(float)best->speedX100/100.0f;
  est.valid=true;
  est.irAgeMs=age;
  est.absDistMm=(float)best->pulses*IR_MM_PER_PULSE + speed*(float)age/1000.0f;
  est.deltaMm=irAnchorValid?(est.absDistMm-irAnchorDistMm):0.0f;
  est.deltaPulses=irAnchorValid?(int32_t)((est.absDistMm-irAnchorDistMm)/IR_MM_PER_PULSE):0;
  return est;
}

// ---- anchor + marker-speed hook, called from acceptEvent() ONLY (§7, §8.3) -
static void irOnAccepted(const MarkerEvent& e){
  // marker speed: exactly one accepted step from the previous anchor, same
  // declared direction, forward time. Anything else resets without a sample.
  uint8_t newMm=navMm;   // acceptEvent has already advanced it
  if(mmAnchorValid && navDir!=MAP_UNSET &&
     nextMm(mmAnchorMm,navDir)==newMm &&
     (int32_t)(e.detectedAtMs-mmAnchorDetMs)>0){
    uint32_t dist=irRouteSpanMm(mmAnchorMm,navDir,1);
    uint32_t dt=(uint32_t)(e.detectedAtMs-mmAnchorDetMs);
    mmSpeedValid=true;
    mmSpeedMmps=(float)dist*1000.0f/(float)dt;
    mmSpeedFrom=mmAnchorMm; mmSpeedTo=newMm; mmSpeedAtMs=e.detectedAtMs;
    mmSpeedDtMs=dt;
  }else{
    mmSpeedValid=false;   // reset, no fabricated sample (§7)
  }
  mmAnchorValid=true; mmAnchorMm=newMm; mmAnchorDetMs=e.detectedAtMs;
  // IR distance anchor: original estimate by stable identity, or invalid.
  const IrEventEst* est=irEstFind(e.detectedAtMs);
  if(est && est->valid){ irAnchorValid=true; irAnchorDistMm=est->absDistMm; }
  else irAnchorValid=false;
}

// ---- mm/speed event records (§9.3) ------------------------------------------

static void irPublishMmSpeed(const MarkerEvent& e,const IrEventEst& est,
                             uint8_t fromMm,int revision,const char* finalDisp){
  // Worst case (§9.5), MEASURED against the short-key format below with
  // every field maximal and floats clamped: 405 bytes into 511 usable —
  // includes the pkph field the review restored. The truncation guard
  // remains the structural backstop. (An earlier draft carried two
  // contradictory budgets here, both from the long-key era; the adversarial
  // pass flagged them and this is the only number now.)
  char irdist[16], irpulse[16], irage[8];
  if(est.valid && est.anchorWasValid){
    // Clamped prints: the worst-case arithmetic above is only honest if no
    // field can widen past its budget (the CTO_STATUS lesson, three times).
    float dmm=est.deltaMm; if(dmm>99999.9f)dmm=99999.9f; if(dmm<-99999.9f)dmm=-99999.9f;
    long dp=(long)est.deltaPulses; if(dp>999999L)dp=999999L; if(dp<-999999L)dp=-999999L;
    snprintf(irdist,sizeof(irdist),"%.1f",(double)dmm);
    snprintf(irpulse,sizeof(irpulse),"%ld",dp);
    snprintf(irage,sizeof(irage),"%lu",(unsigned long)est.irAgeMs);
  }else{ strlcpy(irdist,"null",16); strlcpy(irpulse,"null",16); strlcpy(irage,"null",8); }

  // ---- IR ODOMETRY COVERAGE (observation only) -----------------------------
  // 2026-08-19. On the morning of this date the IR layer reported VALID at
  // 240-280 mm/s while its cumulative pulse count stood still for seconds at a
  // time: interval-derived SPEED stayed plausible while cumulative-pulse
  // DISTANCE under-counted to ~53%. Nothing in the system said so. The failure
  // was only found because an off-board script happened to compare irdp against
  // route distance; 355 genuine markers had already been mis-hypothesised by
  // then. This publishes that comparison continuously so the same failure can
  // never again be invisible.
  //
  // Every input already existed — irdp and the route distance are both in this
  // record. Nothing new is measured; the arithmetic is simply made conspicuous.
  //
  // STRICTLY OBSERVATIONAL. It is a ratio, not a verdict: no threshold, no
  // healthy/unhealthy flag, no consumer, and nothing downstream may branch on
  // it. A future governor must not read it as permission (decisions 0005/0036,
  // and the withdrawn bestk=0 veto of 2026-08-18).
  //
  // NULL, NEVER ZERO, wherever the ratio would be meaningless — zero is a
  // measurement and would read as total loss:
  //   * IR anchor unavailable (covers sensor reboot, pulse regression, and
  //     declaration, all of which clear irAnchorValid);
  //   * a station sequence active — approach/ramp/final/dwell/depart are not
  //     ordinary running and their pulse counts are not comparable;
  //   * the event was not accepted, so there is no Hall segment to speak of;
  //   * route distance unknown or non-positive.
  char irexp[12], ircov[12];   // filled below, once the route span is known
  char mmsp[32];
  // Review fixes (observation verifier): every speed field belongs to THIS
  // event, not to the latest global sample — a held event's record was
  // showing acc:1 with the PREVIOUS segment's speed. "This event produced
  // the sample" is exactly mmSpeedAtMs == e.detectedAtMs.
  bool thisEventAccepted = mmSpeedValid && (mmSpeedAtMs==e.detectedAtMs);
  char diststr[16], dtstr[16];
  if(mmSpeedValid && mmSpeedAtMs==e.detectedAtMs){
    snprintf(diststr,sizeof(diststr),"%lu",(unsigned long)irRouteSpanMm(mmSpeedFrom,navDir,1));
    snprintf(dtstr,sizeof(dtstr),"%u",(unsigned)mmSpeedDtMs);
  }else{ strlcpy(diststr,"null",16); strlcpy(dtstr,"null",16); }

  // Coverage ratio — see the note above irexp's declaration. Computed here
  // because it needs both thisEventAccepted and the route span.
  {
    uint32_t routeMm = (mmSpeedValid && mmSpeedAtMs==e.detectedAtMs)
                       ? (uint32_t)irRouteSpanMm(mmSpeedFrom,navDir,1) : 0UL;
    if(thisEventAccepted && stPhase==ST_IDLE &&
       est.valid && est.anchorWasValid && routeMm>0 && est.deltaPulses>=0){
      float expPulses=(float)routeMm/IR_MM_PER_PULSE;
      if(expPulses>0.0f){
        float cov=(float)est.deltaPulses/expPulses;
        if(cov>99.99f) cov=99.99f;
        snprintf(irexp,sizeof(irexp),"%.1f",(double)expPulses);
        snprintf(ircov,sizeof(ircov),"%.2f",(double)cov);
        irCovPulses+=(float)est.deltaPulses;   // rolling summary: convenience
        irCovExpected+=expPulses;              // only, never an input to logic
        if(irCovSegments<0xFFFFFFFFUL) irCovSegments++;
      }else{ strlcpy(irexp,"null",12); strlcpy(ircov,"null",12); }
    }else{ strlcpy(irexp,"null",12); strlcpy(ircov,"null",12); }
  }
  float msc=mmSpeedMmps; if(msc>9999.9f)msc=9999.9f;
  char mmk[16];
  if(thisEventAccepted){
    snprintf(mmsp,sizeof(mmsp),"%.1f",(double)msc);
    snprintf(mmk,sizeof(mmk),"%.2f",(double)(msc*PKPH_PER_MMPS));
  }else{ strlcpy(mmsp,"null",32); strlcpy(mmk,"null",16); }
  // route hypotheses k=0..4 from the PREVIOUS accepted anchor (§8.2)
  uint32_t spans[5]; int nearestK=-1; float bestRes=0; char resbuf[16];
  for(uint8_t k=0;k<5;k++){
    spans[k]=irRouteSpanMm(fromMm,navDir,k);
    if(est.valid && est.anchorWasValid){
      float r=est.deltaMm-(float)spans[k];
      if(r>99999.9f)r=99999.9f; if(r<-99999.9f)r=-99999.9f;
      if(nearestK<0 || fabsf(r)<fabsf(bestRes)){ nearestK=k; bestRes=r; }
    }
  }
  if(nearestK>=0) snprintf(resbuf,sizeof(resbuf),"%.1f",(double)bestRes);
  else strlcpy(resbuf,"null",16);
  char nk[8];
  if(nearestK>=0) snprintf(nk,sizeof(nk),"%d",nearestK); else strlcpy(nk,"null",8);
  char b[512];
  int n=snprintf(b,sizeof(b),
    // SPEC DEVIATION, documented for CODEX: §9.3's long field names plus
    // §9.5's 512-byte PubMsg bound are mathematically incompatible — the
    // skeleton alone measured 438 bytes and the clamped-value worst case
    // 571. §9.5 is the harder constraint (the payload MUST fit the queue),
    // so the keys are shortened with this fixed mapping, schema unchanged:
    //   ev=event_ms rev=revision from/to=from_mm/to_mm pol=hall_pol
    //   dur=duration_ms drift=baseline_drift gate=timing_gate
    //   acc=nav_accepted disp=final_disposition dist=route_distance_mm
    //   dt=hall_dt_ms mmps=mm_speed_mmps irv=ir_valid irage=ir_age_ms
    //   irdp=ir_delta_pulses irmm=ir_distance_mm k0..k4=route_kN_mm
    //   bestk=nearest_k resid=residual_mm agree=agreement
    //   irexp=ir_expected_pulses ircov=ir_coverage_ratio
    // (Budget stated at the top of this function: was 449; irexp adds at most
    // 17 bytes and ircov 14 — both clamped at print — for 480 of 511. The
    // truncation guard below remains the structural backstop.)
    "{\"schema\":\"quorum-mm-speed/1\",\"ev\":%lu,\"rev\":%d,"
    "\"from\":%u,\"to\":%u,\"dir\":\"%s\",\"pol\":\"%c\",\"peak\":%d,"
    "\"dur\":%u,\"drift\":%d,"
    "\"gate\":\"%s\",\"acc\":%d,\"disp\":\"%s\","
    "\"dist\":%s,\"dt\":%s,\"mmps\":%s,\"pkph\":%s,"
    "\"irv\":%d,\"irage\":%s,\"irdp\":%s,\"irmm\":%s,"
    "\"irexp\":%s,\"ircov\":%s,"
    "\"k0\":%lu,\"k1\":%lu,\"k2\":%lu,"
    "\"k3\":%lu,\"k4\":%lu,\"bestk\":%s,\"resid\":%s,"
    "\"agree\":\"OBSERVE_ONLY\"}",
    (unsigned long)e.detectedAtMs,revision,
    thisEventAccepted?mmSpeedFrom:fromMm,
    thisEventAccepted?mmSpeedTo:navMm,
    (navDir==MAP_CW)?"CW":(navDir==MAP_CCW)?"CCW":"UNSET",
    polChar(e.polarity),e.peak,(unsigned)e.durationMs,(int)e.baselineDrift,
    lastTimingGate,thisEventAccepted?1:0,finalDisp,
    diststr,dtstr,
    mmsp,mmk,
    (est.valid&&est.anchorWasValid)?1:0,irage,irpulse,irdist,
    irexp,ircov,
    (unsigned long)spans[0],(unsigned long)spans[1],(unsigned long)spans[2],
    (unsigned long)spans[3],(unsigned long)spans[4],nk,resbuf);
  if(n<0||(size_t)n>=sizeof(b)){
    irOversizePubs++;
    snprintf(b,sizeof(b),"{\"schema\":\"quorum-mm-speed/1\",\"event_ms\":%lu,\"error\":\"OVERSIZE\"}",
             (unsigned long)e.detectedAtMs);
  }
  pubMarker(T_MM_SPEED,b);   // physical event evidence: marker queue (§9.5)
}

// Per-event observation pair (§8.1/§9.3), called around navOnMarker() by
// BOTH the firmware's drainMarkers() and the replay harness — one code path,
// no copy that can drift.
static uint8_t    irObsPrevAnchorMm=0;
static IrEventEst irObsEst;
static void irObserveEventPre(const MarkerEvent& e){
  irObsPrevAnchorMm = mmAnchorValid ? mmAnchorMm : navMm;
  irObsEst = irEstimateAt(e.detectedAtMs);
  irEstRing[irEstNext]=irObsEst; irEstNext=(uint8_t)((irEstNext+1)%16);
}
static void irObserveEventPost(const MarkerEvent& e){
  irPublishMmSpeed(e,irObsEst,irObsPrevAnchorMm,1,lastTimingGate);
}

// revision-2 finalization for held events (§9.3) — void, cannot affect the
// quarantine branch outcome (§8.3).
static void irObsFinalize(const MarkerEvent& e,const char* disp){
  const IrEventEst* est=irEstFind(e.detectedAtMs);
  IrEventEst empty; empty.detectedAtMs=e.detectedAtMs;
  irPublishMmSpeed(e,est?*est:empty,mmAnchorValid?mmAnchorMm:navMm,2,disp);
}

// ---- 1 s / 5 s summaries (§9.1, §9.2, §9.4) ---------------------------------
static void serviceIrTelemetry(){
  uint32_t now=millis();
  if((uint32_t)(now-irLastSpeedPubMs)>=1000){
    irLastSpeedPubMs=now;
    IrView v=irCurrentView();
    // ir_speed worst case (§9.5, corrected in review): literal 237 + seq 10
    // + ms 10 + ids 3x10 + sensor_ms 10 + pulses 10 + interval 10 + span 5
    // + state 16 + valid 1 + speed 11 + pkph 11 + ages 3x10 = 390 into 511.
    char sp[16], kph[16];
    if(v.numeric){ snprintf(sp,sizeof(sp),"%.2f",(double)v.mmps);
                   snprintf(kph,sizeof(kph),"%.2f",(double)(v.mmps*PKPH_PER_MMPS)); }
    else { strlcpy(sp,"null",16); strlcpy(kph,"null",16); }
    char b[512];
    int n=snprintf(b,sizeof(b),
      "{\"schema\":\"quorum-ir-speed/1\",\"report_seq\":%lu,\"report_ms\":%lu,"
      "\"sensor_id\":%lu,\"sensor_boot\":%lu,\"tx_seq\":%lu,\"sensor_ms\":%lu,"
      "\"pulses\":%lu,\"last_interval_ms\":%lu,\"span\":%u,\"state\":\"%s\","
      "\"speed_valid\":%d,\"speed_mmps\":%s,\"pkph\":%s,"
      "\"rx_age_ms\":%lu,\"rx_gap_ms\":%lu,\"rx_max_gap_ms\":%lu}",
      (unsigned long)++irReportSeq,(unsigned long)now,
      (unsigned long)(irHaveLatest?irLatest.sensorId:0),
      (unsigned long)(irHaveLatest?irLatest.bootId:0),
      (unsigned long)(irHaveLatest?irLatest.txSequence:0),
      (unsigned long)(irHaveLatest?irLatest.capturedMs:0),
      (unsigned long)(irHaveLatest?irLatest.completedPulses:0),
      (unsigned long)(irHaveLatest?irLatest.lastIntervalMs:0),
      (unsigned)(irHaveLatest?irLatest.opticalSpan:0),v.state,
      v.numeric?1:0,sp,kph,
      (unsigned long)v.ageMs,(unsigned long)irRxGapMs,(unsigned long)irRxMaxGapMs);
    if(n<0||(size_t)n>=sizeof(b)){ irOversizePubs++; strlcpy(b,"{\"schema\":\"quorum-ir-speed/1\",\"error\":\"OVERSIZE\"}",sizeof(b)); }
    pub(T_IR_SPEED_T,b,false);

    // combined operator view (§9.4). Fixed authority strings are acceptance
    // checks: Test A must never claim IR controls speed.
    // Worst case (§9.5): literal 265 + ir 10+11+3 + mm 11+9+10+3+3 + pwm 2x4
    // = 335 into 511. Guard below is the backstop.
    char msp[16],mkph[16];
    if(mmSpeedValid){ snprintf(msp,sizeof(msp),"%.1f",(double)mmSpeedMmps);
                      snprintf(mkph,sizeof(mkph),"%.2f",(double)(mmSpeedMmps*PKPH_PER_MMPS)); }
    else { strlcpy(msp,"null",16); strlcpy(mkph,"null",16); }
    // Rolling odometry coverage — CONVENIENCE ONLY. The per-segment ratio in
    // mm/speed is the record; this exists so an operator can see the distance
    // channel degrading without reading every event. null until at least one
    // ordinary segment has been measured since the last reset: zero would be a
    // claim of total loss, which is a different statement from "not yet known".
    char covs[16];
    if(irCovSegments>0 && irCovExpected>0.0f){
      float rc=irCovPulses/irCovExpected; if(rc>99.99f) rc=99.99f;
      snprintf(covs,sizeof(covs),"%.2f",(double)rc);
    } else strlcpy(covs,"null",16);
    int n2=snprintf(b,sizeof(b),
      "{\"schema\":\"quorum-speed-view/1\","
      "\"ir_valid\":%d,\"ir_mmps\":%s,\"ir_pkph\":%s,\"ir_age_ms\":%lu,"
      "\"mm_valid\":%d,\"mm_mmps\":%s,\"mm_pkph\":%s,\"mm_age_ms\":%lu,"
      "\"mm_from\":%u,\"mm_to\":%u,"
      "\"ir_cov\":%s,\"ir_cov_n\":%lu,"
      "\"commanded_pwm\":%d,\"actual_pwm\":%d,"
      "\"control_source\":\"PWM_PRESET\",\"authority\":\"OBSERVE_ONLY\"}",
      v.numeric?1:0,sp,kph,(unsigned long)v.ageMs,
      mmSpeedValid?1:0,msp,mkph,
      (unsigned long)(mmSpeedValid?(uint32_t)(now-mmSpeedAtMs):0),
      mmSpeedFrom,mmSpeedTo,
      covs,(unsigned long)irCovSegments,
      (int)commandedPwm,(int)actualPwm);
    if(n2<0||(size_t)n2>=sizeof(b)){ irOversizePubs++; strlcpy(b,"{\"schema\":\"quorum-speed-view/1\",\"error\":\"OVERSIZE\"}",sizeof(b)); }
    pub(T_SPEED_VIEW,b,false);
  }
  if((uint32_t)(now-irLastStatusPubMs)>=5000){
    irLastStatusPubMs=now;
    // Sender counters (latest packet) and receiver totals, separately named
    // (§9.2). FOURTH buffer-arithmetic failure of the week, this one REFUTED
    // by the adversarial pass: the previous comment claimed "measured 505,
    // 20 numeric fields" against a format string carrying 24 conversions —
    // true worst case 532 into 511 usable. Keys are mine (schema unchanged),
    // so they are shortened until the arithmetic holds:
    // literal skeleton measured 247 + 24 u32 x 10 + wire sizes 6 = 493 < 511.
    char b[512];
    int n=snprintf(b,sizeof(b),
      "{\"schema\":\"quorum-ir-status/1\",\"wire\":{\"cto\":%u,\"echo\":%u,\"ir\":%u},"
      "\"snd\":{\"miss\":%lu,\"abrt\":%lu,\"cinv\":%lu,\"sat\":%lu,"
      "\"mgus\":%lu,\"tx\":%lu,\"txe\":%lu,\"txf\":%lu},"
      "\"rcv\":{\"acc\":%lu,\"gaps\":%lu,\"dup\":%lu,\"ooo\":%lu,"
      "\"blen\":%lu,\"bver\":%lu,\"bwire\":%lu,\"bsrc\":%lu,\"bsen\":%lu,"
      "\"btgt\":%lu,\"benum\":%lu,\"benc\":%lu,"
      "\"regr\":%lu,\"boot\":%lu,\"rebase\":%lu,\"qdrop\":%lu,\"ovsz\":%lu,"
      "\"mac_ok\":%d}}",
      (unsigned)sizeof(CtoPeerPacket),(unsigned)sizeof(Cto3RoleEcho),(unsigned)sizeof(IrSpeedPacketV1),
      (unsigned long)(irHaveLatest?irLatest.sampleMissedSlots:0),
      (unsigned long)(irHaveLatest?irLatest.openAborts:0),
      (unsigned long)(irHaveLatest?irLatest.contrastInvalidEpisodes:0),
      (unsigned long)(irHaveLatest?irLatest.saturatedSamples:0),
      (unsigned long)(irHaveLatest?irLatest.sampleMaxGapUs:0),
      (unsigned long)(irHaveLatest?irLatest.txAttempts:0),
      (unsigned long)(irHaveLatest?irLatest.txImmediateErrors:0),
      (unsigned long)(irHaveLatest?irLatest.txDeliveryFailures:0),
      (unsigned long)irAccepted,(unsigned long)irSeqGaps,(unsigned long)irDups,
      (unsigned long)irOutOfOrder,
      (unsigned long)irRejBadLen,(unsigned long)irRejBadMagicVer,
      (unsigned long)irRejBadWire,(unsigned long)irRejBadSource,
      (unsigned long)irRejBadSensor,(unsigned long)irRejBadTarget,
      (unsigned long)irRejBadEnum,(unsigned long)irRejBadEncoding,
      (unsigned long)irPulseRegressions,(unsigned long)irSensorReboots,
      (unsigned long)irPulseRebases,(unsigned long)irRxQueueDrops,
      (unsigned long)irOversizePubs,
      irSensorMacUsable()?1:0);
    if(n<0||(size_t)n>=sizeof(b)){ irOversizePubs++; strlcpy(b,"{\"schema\":\"quorum-ir-status/1\",\"error\":\"OVERSIZE\"}",sizeof(b)); }
    pub(T_IR_STATUS,b,false);
  }
}

#endif  // IR_TEST_A_ON — the disabled-build stubs live with the prototypes above

// ---- radio -----------------------------------------------------------------
static void ctoOnRecv(const esp_now_recv_info_t* info,const uint8_t* data,int len){
  // WiFi-task context: copy and leave, exactly like onMqttEnqueue. All state
  // belongs to the loop thread.
  // IR TEST A router (spec §5.1): exact length is the discriminator — 72 is
  // IR, 45/15 are the frozen CTO packets, everything else was already being
  // dropped silently and still is. The CTO path below is byte-for-byte
  // untouched; the IR path validates fully in-callback and only ever copies
  // into its OWN queue, so a 20 Hz sensor cannot displace peer truth.
  if(len==(int)sizeof(IrSpeedPacketV1) || (len>0 && data[0]==IR_SPEED_MAGIC)){
#if IR_TEST_A_ON
    irAcceptFrame(info && info->src_addr ? info->src_addr : (const uint8_t*)"\0\0\0\0\0\0",
                  data,len,millis());
#endif
    return;
  }
  if(len!=(int)sizeof(CtoPeerPacket) && len!=(int)sizeof(Cto3RoleEcho)) return;
  uint8_t buf[sizeof(CtoPeerPacket)]; // echo is smaller; length rides the queue item
  memcpy(buf,data,len);
  struct { uint8_t b[sizeof(CtoPeerPacket)]; int l; } item;
  memcpy(item.b,buf,len); item.l=len;
  if(ctoRxQueue && xQueueSend(ctoRxQueue,&item,0)!=pdTRUE) ctoRxDropped++;
}
// WiFi-task context, like ctoOnRecv: touch nothing but two counters. No
// publish, no Serial, no navigation. Broadcast is unacknowledged, so SUCCESS
// here means the frame was transmitted, NOT that anyone heard it.
// ESP32 core 3.3.11 signature: the first argument is wifi_tx_info_t*, not the
// bare peer MAC of older cores (the recv callback moved the same way). Pinned
// here because a core downgrade turns this into a silent conversion error.
static void ctoOnSend(const wifi_tx_info_t*, esp_now_send_status_t status){
  if(status==ESP_NOW_SEND_SUCCESS) ctoTxDone++; else ctoTxFailed++;
}
static void ctoRadioInit(){
  if(ctoRadioUp) return;
  if(esp_now_init()!=ESP_OK){ Serial.println("[CTO] esp_now init FAILED"); return; }
  esp_now_peer_info_t pi={}; memcpy(pi.peer_addr,CTO_BCAST,6);
  pi.channel=0; pi.encrypt=false;
  if(!esp_now_is_peer_exist(CTO_BCAST)) esp_now_add_peer(&pi);
  esp_now_register_recv_cb(ctoOnRecv);
  esp_now_register_send_cb(ctoOnSend);       // 1.16Ra: measure, don't assume
  ctoChannel=ctoWifiChannel();
  ctoRadioUp=true;
  Serial.printf("[CTO] radio up (ESP-NOW, broadcast) ch=%u\n",(unsigned)ctoChannel);
}
static void ctoTxStatus(){
  CtoPeerPacket p={};
  p.magic=CTO2_MAGIC; p.version=CTO2_VERSION;
  p.senderId=LOCO_ID; p.sequence=++ctoTxSeq;
  p.hallMm=navMm; p.mapDir=navDir;
  p.frontBoundaryMm=(navDir==MAP_UNSET)?navMm:ctoMyFrontB();
  p.rearBoundaryMm =(navDir==MAP_UNSET)?navMm:ctoMyRearB();
  p.frontOffset=CONSIST_EXTENT_FRONT_MARKERS; p.rearOffset=CONSIST_EXTENT_REAR_MARKERS;
  p.autoMode=autoRunning?1:0;
  p.running=(commandedPwm>0||actualPwm>0)?1:0;
  p.motionState=(actualPwm==0&&commandedPwm==0)?0:3;   // STOPPED_INFERRED / MOVING
  p.rampPwm=(uint8_t)constrain(actualPwm,0,255);
  p.speedValid=0; p.speedX10=0;                        // no independent speed yet
  p.lastMoveAgeDs=0;
  p.truthSource=(navState==NAV_NORMAL)?2:(navState==NAV_EVALUATING)?1:0;
  p.stationPhase=(uint8_t)stPhase;
  p.trafficPhase=(uint8_t)ctoTraffic;
  p.mustHoldEligible=0;
  p.trafficStopForId=ctoTrafficForId;
  p.senderRxAccepted=ctoRxAccepted; p.senderTxAttempts=ctoTxAttempts;
  p.senderTxImmediateErrors=ctoTxErrors;
  ctoTxAttempts++;
  if(esp_now_send(CTO_BCAST,(uint8_t*)&p,sizeof(p))!=ESP_OK) ctoTxErrors++;
}
static void ctoTxEcho(){
  Cto3RoleEcho e={};
  e.magic=CTO3_ECHO_MAGIC; e.version=CTO3_ECHO_VERSION;
  e.senderId=LOCO_ID; e.role=(uint8_t)ctoRole; e.partnerId=ctoPartnerId;
  e.pairEpochMs=ctoPairEpochMs;
  esp_now_send(CTO_BCAST,(uint8_t*)&e,sizeof(e));
}

// ---- registry --------------------------------------------------------------
static void ctoAcceptPeer(const CtoPeerPacket& p){
  if(p.magic!=CTO2_MAGIC || p.version!=CTO2_VERSION) return;   // r9-and-below: reject, never guess
  if(p.senderId==LOCO_ID) return;
  int8_t i=ctoPeerIdx(p.senderId);
  if(i<0){ for(uint8_t k=0;k<CTO_MAX_PEERS;k++) if(!ctoPeers[k].seen){ i=(int8_t)k; break; } }
  if(i<0){ // full: evict stalest — r12 LRU rule
    uint32_t worst=0; for(uint8_t k=0;k<CTO_MAX_PEERS;k++){ uint32_t age=millis()-ctoPeers[k].rxMs; if(age>=worst){worst=age;i=(int8_t)k;} }
  }
  CtoPeer& q=ctoPeers[i];
  // Round 2 finding 7a (and 5-adjacent): a reused slot must not inherit the
  // previous occupant's ramp history or role echo. Reset on identity change.
  if(q.seen && q.id!=p.senderId) q=CtoPeer{};
  const bool sameOcc = q.seen;              // before this packet, same loco
  const uint8_t prevRamp = q.rampPwm;
  q.seen=true; q.id=p.senderId; q.seq=p.sequence; q.rxMs=millis();
  q.hallMm=p.hallMm; q.frontB=p.frontBoundaryMm; q.rearB=p.rearBoundaryMm;
  q.mapDir=p.mapDir; q.autoMode=p.autoMode; q.running=p.running;
  q.motionState=p.motionState;
  // Round 2 finding 7: a fall of >=2 counts arms it; it then PERSISTS while
  // the ramp is non-increasing, so one noisy sample cannot un-detect a real
  // deceleration. sameOcc guards against comparing across occupants.
  q.rampFalling = sameOcc && ((p.rampPwm+2 <= prevRamp) ||
                              (q.rampFalling && p.rampPwm <= prevRamp));
  q.rampPwm=p.rampPwm; q.truthSource=p.truthSource;
  q.stationPhase=p.stationPhase; q.trafficPhase=p.trafficPhase; q.stopForId=p.trafficStopForId;
  ctoRxAccepted++;
  // 0031 membership, v1 lifecycle (recorded in the implementation report as a
  // PROPOSAL): the first fresh same-direction peer seen this boot becomes the
  // expected peer. Cleared only by cmd/cto "clear" or power cycle. A second
  // distinct loco does not replace it in v1 (two-train spec).
  if(ctoEnabled && ctoExpectedId==0 && p.senderId!=0) ctoExpectedId=p.senderId;
}
static void ctoAcceptEcho(const Cto3RoleEcho& e){
  if(e.magic!=CTO3_ECHO_MAGIC || e.version!=CTO3_ECHO_VERSION) return;
  if(e.senderId==LOCO_ID) return;
  int8_t i=ctoPeerIdx(e.senderId); if(i<0) return;   // echo without status: ignore
  ctoPeers[i].echoRole=e.role; ctoPeers[i].echoPartner=e.partnerId;
  ctoPeers[i].echoRxMs=millis();
}

// ---- roles (decision 0032: derived, latched, echoed) -----------------------
// CE assignment and termination. Both publish, because a mission that changes
// speed and skips platforms must never be inferred from behaviour.
static void ceBegin(){
  // Missions come from the CURRENT derived roles. No role means no pairing to
  // sever and no way to say which train is the express, so CE is refused and
  // says why — the alternative is guessing which locomotive runs fast.
  if(ctoRole==CTO_ROLE_LEADER)        ceMission=CE_EXPRESS;
  else if(ctoRole==CTO_ROLE_FOLLOWER) ceMission=CE_LOCAL;
  else{
    char b[96];
    snprintf(b,sizeof(b),"{\"event\":\"CE_REFUSED\",\"why\":\"NO_ROLE_NOTHING_TO_SEVER\"}");
    pub(T_ST_CTO,b,false);
    return;
  }
  ceStationSeq=0; ceSkipLatch=-1; ceSkipNow=false; ceSeparated=false;
  ceClosing=false;
  char b[128];
  snprintf(b,sizeof(b),"{\"event\":\"CE_BEGIN\",\"mission\":\"%s\",\"cruise\":%d}",
           ceMissionName(),ceCruisePwm());
  pub(T_ST_CTO,b,false);
  ctoDissolve("CE_MISSION_ASSIGNED");   // CE severs the pair...
  // ...and ctoEvaluateRoles() would re-latch it on the very next 10 Hz pass,
  // because the two trains are still within CTO_PAIR_RANGE_MARKERS. Severance
  // only holds because an active mission INHIBITS formation (see
  // ctoEvaluateRoles). Without that inhibit this dissolve is decoration.
}

// Is a fresh same-direction peer within the slow threshold, in EITHER
// direction? Deliberately direction-agnostic: the express detects the local it
// has caught from behind, and the local detects the express closing on it, from
// the same test and the same numbers. Neither needs to know its own mission to
// answer it, which is what makes the two locomotives agree.
static bool ceNearPeer(){
  // Gap arithmetic is measured FROM navMm, so it means nothing when this
  // locomotive does not know where it is. ctoLimitPwm and ctoEvaluateRoles
  // both guard the identical arithmetic this way; this function did not, and
  // during NO_QUORUM would have armed or ended CE on a stale odometer.
  //
  // CALLERS MUST NOT READ false AS "SEPARATED". false here means "no peer is
  // near, as far as can be determined", and when position is unusable that is
  // an absence of evidence, not evidence of absence. The CE lifecycle below
  // therefore gates on position ITSELF before consulting this function — an
  // earlier attempt guarded only in here, and since the arming branch reads
  // !ceNearPeer(), a NO_QUORUM locomotive would have armed its own ending on a
  // stale odometer: the precise failure the guard was added to prevent.
  if(navDir==MAP_UNSET || !navPositionUsable()) return false;
  for(uint8_t i=0;i<CTO_MAX_PEERS;i++){
    const CtoPeer& p=ctoPeers[i];
    if(!ctoPeerFresh(p)||!ctoPeerSameDir(p)) continue;
    uint16_t ga=ctoGapAhead(p), gb=ctoGapBehind(p);
    uint16_t g=(ga<gb)?ga:gb;
    if(g<=CTO_SLOW_GAP_MARKERS) return true;
  }
  return false;
}

static void ceEnd(const char* why){
  if(ceMission==CE_NONE) return;
  char b[128];
  snprintf(b,sizeof(b),"{\"event\":\"CE_END\",\"mission\":\"%s\",\"why\":\"%s\",\"stations\":%u}",
           ceMissionName(),why,(unsigned)ceStationSeq);
  pub(T_ST_CTO,b,false);
  ceMission=CE_NONE; ceStationSeq=0; ceSkipLatch=-1; ceSkipNow=false;
  ceSeparated=false; ceClosing=false;
}

static void ctoDissolve(const char* why){
  if(ctoRole!=CTO_ROLE_NONE){
    char b[96]; snprintf(b,sizeof(b),"{\"event\":\"CTO_UNPAIRED\",\"why\":\"%s\",\"partner\":%lu}",why,(unsigned long)ctoPartnerId);
    pub(T_ST_CTO,b,false);
  }
  // A latched role conflict holds the locomotive at PWM 0 through
  // ctoLimitPwm. Clearing it below is therefore a RELEASE OF A FULL STOP,
  // and every release must be visible — the operator has to be able to see
  // why a stopped locomotive is free to move again. Found in review: 0037
  // added a new geometric path into this clear, and it was silent.
  if(ctoEchoConflict){
    pub(T_ST_CTO,"{\"event\":\"CTO_ROLE_CONFLICT_CLEARED\",\"why\":\"UNPAIRED\"}",false);
  }
  // The inversion run belongs to THIS pairing; own the reset here rather
  // than at each call site, so a future dissolution path cannot inherit a
  // partial count (review: it was reset at only two of four sites).
  ctoOrderInvertedFor=0;
  ctoRole=CTO_ROLE_NONE; ctoPartnerId=0; ctoPairEpochMs=0; ctoPairDir=MAP_UNSET;
  ctoEchoConflict=false; ctoEchoConfirmed=false;
}
static void ctoLatch(CtoRole r,uint32_t partner){
  ctoRole=r; ctoPartnerId=partner; ctoPairEpochMs=millis();
  ctoOrderInvertedFor=0;                       // 0037: fresh geometry claim
  ctoPairDir=navDir;
  ctoEchoConflict=false;
  char b[112];
  snprintf(b,sizeof(b),"{\"event\":\"CTO_PAIRED\",\"role\":\"%s\",\"partner\":%lu}",
           r==CTO_ROLE_LEADER?"LEADER":"FOLLOWER",(unsigned long)partner);
  pub(T_ST_CTO,b,false);
  ctoTxEcho();
}
// Q1/Q2 at pairing range; long-range provisional order for who-waits.
// Evaluated every service pass — cheap, and formation while stationary needs
// packet-driven evaluation, not only marker-driven (spec sec.5 note).
static void ctoEvaluateRoles(){
  // Round 2 finding 4: the direction-change dissolution must run BEFORE the
  // usable-navigation gate. A direction change during NAV_NO_QUORUM left the
  // old pairing latched with inverted physical order. Any change from the
  // latch-time direction — including to UNSET — dissolves, in every nav state.
  if(ctoRole!=CTO_ROLE_NONE && navDir!=ctoPairDir){
    ctoDissolve("DIRECTION_CHANGED_SINCE_LATCH");
  }
  if(!ctoEnabled || navDir==MAP_UNSET || !navPositionUsable()){ return; }
  int8_t pi=ctoPartnerIdx();
  if(ctoRole!=CTO_ROLE_NONE){
    // dissolution paths; staleness does NOT silently dissolve — fleet stop
    // owns that (0031). The local direction-change check ran above, before
    // the nav gate. Here: the partner declaring a different direction.
    if(pi>=0 && ctoPeerFresh(ctoPeers[pi]) && !ctoPeerSameDir(ctoPeers[pi])){
      ctoDissolve("PARTNER_DIRECTION_CHANGED");
      ctoOrderInvertedFor=0;
      return;
    }
    // 0037: NOSE-TAIL ORDER INVERSION. The latched role IS a claim about
    // geometry — FOLLOWER means "my partner is ahead of me", LEADER means
    // "my partner is behind me". When the arcs say otherwise the claim is
    // simply false: the short follower dwell (5 s) is applied to the wrong
    // locomotive, my role goes out in every echo and feeds my PARTNER's
    // conflict test — which holds at PWM 0 — and the console tells the
    // operator something untrue. (This comment itself said "20 s" in its
    // first draft, repeating the very stale number the same commit was
    // correcting elsewhere. Review caught it.) Dissolving re-derives on the next pass from the
    // geometry that actually exists; if they are too far apart to pair, no
    // role is claimed at all, which is the honest answer.
    // The gates here must be the SAME ones latching uses. Reviewed and
    // corrected: the first draft tested only freshness, so a partner
    // publishing truthSource 0 — NAV_NO_QUORUM, still transmitting boundaries
    // derived from a position it does not believe — could dissolve a pairing
    // on geometry nobody trusts, and would do it on the very same service
    // pass that fleet stop declared NO_POSITION. A discredited partner is
    // 0031's business, not 0037's; hold the role and let the fleet stop speak.
    if(pi>=0 && ctoPeerFresh(ctoPeers[pi]) && ctoPeerNavOk(ctoPeers[pi])
             && ctoPeerSameDir(ctoPeers[pi])){
      const CtoPeer& p=ctoPeers[pi];
      uint16_t ga=ctoGapAhead(p), gb=ctoGapBehind(p);
      // "Clearly on the other side": the near side must beat the far side by
      // the margin, so a pair sitting near-diametric cannot chatter.
      bool inverted = (ctoRole==CTO_ROLE_FOLLOWER)
                        ? (gb + CTO_ORDER_MARGIN_MARKERS <= ga)   // partner now BEHIND
                        : (ga + CTO_ORDER_MARGIN_MARKERS <= gb);  // partner now AHEAD
      if(inverted){
        if(++ctoOrderInvertedFor>=CTO_ORDER_CONFIRM_N){
          char b[144];
          snprintf(b,sizeof(b),
            "{\"event\":\"CTO_ORDER_INVERTED\",\"partner\":%lu,\"was\":\"%s\","
            "\"gap_ahead\":%u,\"gap_behind\":%u}",
            (unsigned long)ctoPartnerId,
            ctoRole==CTO_ROLE_LEADER?"LEADER":"FOLLOWER",
            (unsigned)ga,(unsigned)gb);
          pub(T_ST_CTO,b,false);
          ctoOrderInvertedFor=0;
          ctoDissolve("ORDER_INVERTED");
          // fall through: re-derive from real geometry on THIS pass
        }else{
          return;                              // not yet confirmed; hold
        }
      }else{
        ctoOrderInvertedFor=0;
        return;                                // latched roles persist (0032)
      }
    }else{
      // No usable partner evidence. The confirmation run is broken here, not
      // merely paused: two inverted samples with an unobserved gap between
      // them are not three consecutive observations, and leaving the counter
      // standing let a single fresh pass after a staleness episode dissolve
      // a pairing (found in review, reproduced).
      ctoOrderInvertedFor=0;
      return;                                  // 0031's job, not 0037's
    }
  }
  // CE INHIBITS FORMATION. Dissolving at ceBegin() is not enough on its own:
  // this function re-derives roles from geometry every 100 ms, and at that
  // moment the two trains are still inside CTO_PAIR_RANGE_MARKERS, so the pair
  // would re-latch immediately and the fleet would hold missions AND a pairing
  // at once. The mission holds them apart until it ends, which is exactly when
  // they have closed up again and paired service is what we want back.
  //
  // Placed AFTER every dissolution path above: a direction change, a partner
  // direction change or an order inversion must still be able to break a
  // pairing that already exists while a mission runs.
  if(ceMission!=CE_NONE && !ceClosing && ctoRole==CTO_ROLE_NONE) return;

  // pick the nearest fresh same-direction quorum-holding peer
  int8_t best=-1; uint16_t bestGap=0xFFFF; bool bestAhead=true;
  for(uint8_t i=0;i<CTO_MAX_PEERS;i++){
    CtoPeer& p=ctoPeers[i];
    if(!ctoPeerFresh(p)||!ctoPeerSameDir(p)||!ctoPeerNavOk(p)) continue;
    uint16_t ga=ctoGapAhead(p), gb=ctoGapBehind(p);
    uint16_t g=(ga<gb)?ga:gb;
    if(g<bestGap){ bestGap=g; best=(int8_t)i; bestAhead=(ga<gb); }
  }
  if(best<0) return;
  CtoPeer& p=ctoPeers[best];
  uint16_t ga=ctoGapAhead(p), gb=ctoGapBehind(p);
  // Q1: peer ahead within pairing range -> I FOLLOW
  if(bestAhead && ga<=CTO_PAIR_RANGE_MARKERS){ ctoLatch(CTO_ROLE_FOLLOWER,p.id); return; }
  // Q2: peer behind within pairing range -> I LEAD
  if(!bestAhead && gb<=CTO_PAIR_RANGE_MARKERS){ ctoLatch(CTO_ROLE_LEADER,p.id); return; }
  // Long range: no latch. Provisional who-waits only (spec sec.6): my
  // behind-arc vs his (= my ahead-arc); smaller behind-arc leads; inside the
  // tie band the lower loco ID leads. The ONLY behaviour this drives is the
}
static uint32_t ctoDwellMs(){
  // A CONFIRMED follower at a platform dwells CTO_FOLLOWER_DWELL_MS (5 s);
  // an unconfirmed or solo locomotive uses the ordinary station dwell (15 s).
  // The follower dwells SHORTER, not longer — 1.14A's ruling, "the leader
  // stops waiting for the follower".
  // NOTE: this comment said "20 s (operator, supersedes 15)" until 2026-08-16.
  // 1.14A corrected it; the 1.14A "rebuilt clean" commit reintroduced the
  // stale wording while keeping the correct 5000 constant, so the file
  // contradicted itself for two revisions. Found by test_cto_roles.py, which
  // asserted the comment and failed against the code.
  // CE takes precedence: a mission severs the pairing, so there is no role left
  // to consult. The EXPRESS dwells 5 s at the platforms it does serve (operator
  // 2026-08-19); the LOCAL keeps the ordinary dwell and stops everywhere.
  if(ceMission==CE_EXPRESS) return CE_EXPRESS_DWELL_MS;
  if(ceMission==CE_LOCAL)   return DWELL_MS;
  return (ctoRole==CTO_ROLE_FOLLOWER && ctoEchoConfirmed)?CTO_FOLLOWER_DWELL_MS:DWELL_MS;
}


// ctoProvisionalLeader() and ctoHoldDeparture() were DELETED 2026-08-14 on the
// operator's ruling above. They implemented station-formation waiting, the
// follower-arrival window and the 10 s release dwell -- the mechanism that
// deadlocked at Arches. Nothing replaced them: spacing belongs to the
// follower's limiter, and the leader simply runs its mission.

static int ctoLimitPwm(int want){
  // Universal traffic protection (spec sec.7): one deceleration profile; the
  // flag that differs is simply WHICH machine resumes — station or cruise —
  // and both already exist above this layer. This function only caps.
  if(!ctoEnabled) return want;
  if(ctoFleetHold || ctoEchoConflict) return 0;   // round 2 finding 2: BOTH
  if(navDir==MAP_UNSET||!navPositionUsable()) return want;   // solo rules apply
  int cap=want;
  CtoTrafficPhase newPhase=CTRAF_CLEAR; uint32_t forId=0;
  for(uint8_t i=0;i<CTO_MAX_PEERS;i++){
    CtoPeer& p=ctoPeers[i];
    if(!ctoPeerFresh(p)||!ctoPeerSameDir(p)) continue;
    // Contact guard FIRST (CODEX 1.14 review, finding 3): once bounds cross,
    // the forward arc wraps toward 170 and the ahead/behind comparator
    // classifies the peer as "behind" — skipping every check below exactly
    // when contact is imminent. Symmetric hall-arc proximity runs before any
    // topology rejection can discard the peer.
    uint16_t hallFwd=ctoArc(navMm,p.hallMm);
    uint16_t hallNear=(hallFwd<=(uint16_t)(DNA_N-hallFwd))?hallFwd:(uint16_t)(DNA_N-hallFwd);
    if(hallNear<=(uint16_t)(CONSIST_EXTENT_FRONT_MARKERS+CONSIST_EXTENT_REAR_MARKERS)){
      cap=0; newPhase=CTRAF_HOLD; forId=p.id; break;
    }
    uint16_t ga=ctoGapAhead(p), gb=ctoGapBehind(p);
    if(ga>=gb) continue;                                     // peer not ahead of me
    if(ga<=CTO_CLEAR_GAP_MARKERS){
      cap=0; newPhase=CTRAF_HOLD; forId=p.id; break;
    }
    if(ctoPeerStopping(p)){
      if(ga<=CTO_STOP_GAP_MARKERS){ if(cap>0){cap=0; newPhase=CTRAF_HOLD; forId=p.id;} }
      else if(ga<=CTO_SLOW_GAP_MARKERS){ if(cap>STATION_ZONE_PWM){cap=STATION_ZONE_PWM; newPhase=CTRAF_DECEL; forId=p.id;} }
    }else if(ga<=CTO_STOP_GAP_MARKERS){
      // moving leader, small gap: shadow its actual PWM rather than close
      int shadow=(int)p.rampPwm; if(cap>shadow){cap=shadow; newPhase=CTRAF_DECEL; forId=p.id;}
    }
  }
  if(newPhase!=ctoTraffic || forId!=ctoTrafficForId){
    ctoTraffic=newPhase; ctoTrafficForId=forId;
    char b[112];
    snprintf(b,sizeof(b),"{\"event\":\"CTO_TRAFFIC\",\"phase\":%u,\"for\":%lu,\"cap\":%d}",
             (unsigned)newPhase,(unsigned long)forId,cap);
    pub(T_ST_CTO,b,false);
  }
  return cap;
}

// Why would ctoLimitPwm() hold this locomotive at zero right now? Used only by
// the BEGIN refusal (operator ruling 2026-08-14) so the console can say which
// condition is in the way instead of accepting a launch that cannot happen.
// Order matches the limiter's own precedence.
static const char* ctoRefusalReason(){
  if(!ctoEnabled) return "CTO_OFF_BUT_HELD";       // unreachable unless the cap came from elsewhere
  if(ctoFleetHold) return "CTO_FLEET_STOP_PEER_STALE_OR_LOST";
  if(ctoEchoConflict) return "CTO_ROLE_CONFLICT";
  // A traffic condition: name the peer geometry the operator can act on.
  for(uint8_t i=0;i<CTO_MAX_PEERS;i++){
    const CtoPeer& p=ctoPeers[i];
    if(!ctoPeerFresh(p)||!ctoPeerSameDir(p)) continue;
    uint16_t hallFwd=ctoArc(navMm,p.hallMm);
    uint16_t hallNear=(hallFwd<=(uint16_t)(DNA_N-hallFwd))?hallFwd:(uint16_t)(DNA_N-hallFwd);
    if(hallNear<=(uint16_t)(CONSIST_EXTENT_FRONT_MARKERS+CONSIST_EXTENT_REAR_MARKERS))
      return "CTO_TOO_CLOSE_TO_PEER_MOVE_APART";
    uint16_t ga=ctoGapAhead(p), gb=ctoGapBehind(p);
    if(ga<gb && ga<=CTO_STOP_GAP_MARKERS) return "CTO_TRAFFIC_AHEAD_HOLDING";
  }
  return "CTO_HOLDING";
}

// ---- fleet stop (decision 0031: by absence, never announcement) ------------
static void ctoServiceFleetStop(){
  if(!ctoEnabled || ctoExpectedId==0){ ctoFleetHold=false; return; }
  int8_t i=ctoPeerIdx(ctoExpectedId);
  bool ok = (i>=0) && ctoPeerFresh(ctoPeers[i]) && ctoPeerNavOk(ctoPeers[i]);
  if(!ok && !ctoFleetHold){
    ctoFleetHold=true;
    // Round 4 (CODEX): NO explicit zero-request here. Requesting 0 directly
    // made requested==capped, so the one-profile branch never fired and the
    // stop braked at 150 ms/PWM — and continuous enforcement could not
    // correct it, because commandedPwm was already 0. The enforcement pass
    // runs later in THIS SAME ctoService() pass, sees cap(desired)=0 <
    // commandedPwm, brakes at the measured 200 ms/PWM, and preserves the
    // uncapped desired intent for the resume.
    char b[128];
    snprintf(b,sizeof(b),"{\"event\":\"CTO_FLEET_STOP\",\"expected\":%lu,\"why\":\"%s\"}",
             (unsigned long)ctoExpectedId,(i<0)?"NEVER_HEARD":!ctoPeerFresh(ctoPeers[i])?"STALE":"NO_POSITION");
    pub(T_ST_CTO,b,false);
    publishAlert("CTO","FLEET_STOP");
  }else if(ok && ctoFleetHold){
    ctoFleetHold=false;
    pub(T_ST_CTO,"{\"event\":\"CTO_FLEET_CLEAR\"}",false);
    // No autonomous surge back to speed: the station machine's own cruise
    // path restores speed through ctoLimitPwm on its next pass.
  }
}

// ---- role echo cross-check (0032) ------------------------------------------
static void ctoServiceEchoCheck(){
  // CODEX 1.14 review, finding 5, and the 0034 ruling: three-valued, and
  // CONFIRMED is the only state that permits formed-bubble choreography.
  //   CONFIRMED   — fresh echo, OPPOSITE role, partnerId == me.
  //   CONFLICT    — fresh echo claiming MY role with partnerId == me.
  //   UNCONFIRMED — everything else: stale, absent, role NONE, wrong or no
  //                 partner, an older peer that cannot echo at all.
  // CORRECTED (review, 2026-08-16): the old wording here — "universal traffic
  // protection never consults this" — was true of ctoEchoConfirmed and FALSE
  // of ctoEchoConflict, which sits three lines below it. ctoLimitPwm returns
  // 0 on a latched conflict, so this function does carry stop authority.
  // CONFIRMED gates the release and the dwell length (ctoDwellMs);
  // CONFLICT holds the locomotive at zero.
  if(ctoRole==CTO_ROLE_NONE){ ctoEchoConflict=false; ctoEchoConfirmed=false; return; }
  int8_t i=ctoPartnerIdx();
  if(i<0){
    // Round 3 residual: a partner evicted from the registry clears BOTH
    // flags — a vanished partner is the fleet stop's jurisdiction (0031),
    // and a conflict latched against nobody is not a clean state.
    ctoEchoConfirmed=false;
    if(ctoEchoConflict){ ctoEchoConflict=false;
      pub(T_ST_CTO,"{\"event\":\"CTO_ROLE_CONFLICT_CLEARED\",\"why\":\"PARTNER_GONE\"}",false); }
    return;
  }
  CtoPeer& p=ctoPeers[i];
  // Round 2 finding 5: an echo only counts if it is fresh AND postdates THIS
  // latch — a still-fresh echo from an earlier pairing with the same
  // locomotive must not confirm (or conflict with) the new one. And a stale
  // echo yields clean UNCONFIRMED: confirmed false, and an existing conflict
  // clears through the normal transition below — the partner going silent is
  // the fleet stop's jurisdiction (0031), not a latched conflict's.
  bool echoValid = p.echoRxMs!=0 &&
                   (uint32_t)(millis()-p.echoRxMs)<=2*CTO_PEER_STALE_MS &&
                   (int32_t)(p.echoRxMs-ctoPairEpochMs)>=0;
  uint8_t expectRole=(ctoRole==CTO_ROLE_LEADER)?(uint8_t)CTO_ROLE_FOLLOWER:(uint8_t)CTO_ROLE_LEADER;
  ctoEchoConfirmed = echoValid && (p.echoRole==expectRole && p.echoPartner==LOCO_ID);
  bool conflict    = echoValid && (p.echoRole==(uint8_t)ctoRole && p.echoPartner==LOCO_ID);
  if(conflict && !ctoEchoConflict){
    ctoEchoConflict=true;
    // Round 4: no explicit zero-request — same reasoning as the fleet stop.
    // ctoLimitPwm() returns 0 on this flag; enforcement applies it at the
    // one braking profile later in this same service pass.
    pub(T_ST_CTO,"{\"event\":\"CTO_ROLE_CONFLICT\",\"action\":\"HOLD\"}",false);
    publishAlert("CTO","ROLE_CONFLICT");
  }else if(!conflict && ctoEchoConflict){
    ctoEchoConflict=false;
    pub(T_ST_CTO,"{\"event\":\"CTO_ROLE_CONFLICT_CLEARED\"}",false);
  }
}

// ---- operator command -------------------------------------------------------
static void ctoHandleClear(const char* msg){
  // CODEX 1.14 review, finding 6: EXACT match after trim. strncmp accepted
  // "clear-anything" — and "clear" disarms fleet protection, so a prefix
  // match is not a convenience, it is a hazard.
  char pay[16]; size_t n=0;
  while(*msg==' '||*msg=='\t') msg++;
  while(msg[n] && msg[n]!='\r' && msg[n]!='\n' && msg[n]!=' ' && msg[n]!='\t' && n<sizeof(pay)-1){ pay[n]=msg[n]; n++; }
  pay[n]=0;
  // Round 2 finding 6: exact means EXACT. After the token, only whitespace
  // may remain — "clear anything" must be refused as firmly as
  // "clear-anything", because "clear" disarms fleet protection.
  const char* rest=msg+n;
  while(*rest==' '||*rest=='\t'||*rest=='\r'||*rest=='\n') rest++;
  if(*rest!=0){
    pub(T_ST_CTO,"{\"event\":\"CTO_CMD_REFUSED\",\"why\":\"TRAILING_CONTENT\"}",false);
    return;
  }
  if(!strcmp(pay,"clear")){
    // CE ends here too. The mission was derived from a pairing and is ended by
    // peer geometry; wiping the peer registry destroys both, so a surviving
    // mission would be one nothing could ever end.
    ceEnd("OPERATOR_CLEAR");
    for(uint8_t i=0;i<CTO_MAX_PEERS;i++) ctoPeers[i]=CtoPeer{};
    ctoDissolve("OPERATOR_CLEAR");
    ctoExpectedId=0; ctoFleetHold=false; ctoTraffic=CTRAF_CLEAR; ctoTrafficForId=0;
    pub(T_ST_CTO,"{\"event\":\"CTO_CLEARED\"}",false);
  }else if(!strcmp(pay,"off")){
    ceEnd("CTO_OFF");   // no CTO, no missions: CE is a CTO routine
    ctoEnabled=false; ctoDissolve("CTO_OFF");
    ctoExpectedId=0; ctoFleetHold=false; ctoTraffic=CTRAF_CLEAR; ctoTrafficForId=0;
    pub(T_ST_CTO,"{\"event\":\"CTO_OFF\"}",false);
  }else if(!strcmp(pay,"on")){
    ctoEnabled=true;
    pub(T_ST_CTO,"{\"event\":\"CTO_ON\"}",false);
  }else{
    pub(T_ST_CTO,"{\"event\":\"CTO_CMD_REFUSED\",\"why\":\"UNKNOWN\"}",false);
  }
}

// ---- service (loop thread, self-rate-limited) -------------------------------
static void ctoService(){
  // ---- CE lifecycle, run identically on BOTH locomotives -------------------
  // The routine ends when the fleet closes back up: the express has caught the
  // local, and ordinary paired service should resume with the roles swapped by
  // geometry (operator 2026-08-19).
  //
  // Symmetry is the point. An earlier version ended CE only for the EXPRESS, on
  // its own traffic phase — so the LOCAL kept its mission forever, running at
  // PWM 75 with 15 s dwells past the encounter, past re-pairing, until reboot.
  // Nothing on the wire carries CE, so a mission the peer cannot see must be
  // one the peer can independently CONCLUDE. Both trains measure the same gap and end
  // together.
  // Position gates the WHOLE lifecycle, not just the gap test: both branches
  // below act on gap evidence, and a locomotive that does not know where it is
  // has none. The mission simply holds — neither arming nor ending — until
  // navigation is trustworthy again.
  if(ceMission!=CE_NONE && navDir!=MAP_UNSET && navPositionUsable()){
    if(!ceSeparated){
      // Arming: the fleet must actually come apart first, otherwise the pairing
      // range they start inside ends CE immediately.
      if(!ceNearPeer()) ceSeparated=true;
    }else{
      // Closing: lift the inhibit so Q1/Q2 can latch. One-way — a fleet that
      // has begun closing is not un-closed by a momentary gap reading, and
      // re-arming the inhibit mid-approach would fight the pairing it exists
      // to allow.
      if(!ceClosing && ceNearPeer()) ceClosing=true;
      // Ended by the pairing itself, evaluated later in this same pass by
      // ctoEvaluateRoles(). Waiting for the ROLE rather than the gap closes the
      // 13..18 dead band: the mission cannot clear until paired service is
      // actually back.
      if(ctoRole!=CTO_ROLE_NONE) ceEnd("REPAIRED_PAIRED_SERVICE_RESUMED");
    }
  }
  if(!ctoRadioUp) return;
  // drain radio queue (recv callback only copies)
  if(ctoRxQueue){
    struct { uint8_t b[sizeof(CtoPeerPacket)]; int l; } item;
    while(xQueueReceive(ctoRxQueue,&item,0)==pdTRUE){
      if(item.l==(int)sizeof(CtoPeerPacket)){ CtoPeerPacket p; memcpy(&p,item.b,sizeof(p)); ctoAcceptPeer(p); }
      else if(item.l==(int)sizeof(Cto3RoleEcho)){ Cto3RoleEcho e; memcpy(&e,item.b,sizeof(e)); ctoAcceptEcho(e); }
    }
  }
  uint32_t now=millis();
  // 1.16Ra: the channel watch. A silent divergence from the peer's channel is
  // undiagnosable after the fact — this makes the locomotive say so at the
  // moment it happens, rather than leaving an unexplained fleet stop behind.
  //
  // Reviewed hard and rebuilt (self-review round, 2026-08-15). Four rules,
  // each earned:
  //   RATE   — 1 Hz, not loop rate. Every other emitter in this function is
  //            gated; an ungated one that can publish on a transition is a
  //            queue-pressure risk against marker evidence, which is sacred.
  //   RANGE  — ctoWifiChannel() rejects anything outside 1..14.
  //   CONFIRM— three consecutive identical readings before believing a change.
  //            A settling value at association, or one transient sample mid-
  //            reassociation, would otherwise raise a boot-time false alarm,
  //            and an alarm that cries wolf on every power-up is one the
  //            operator correctly learns to ignore.
  //   SCOPE  — no publishWarning(). The warning slot is a SINGLE global slot
  //            with a 20 s auto-clear, and stomping it would silently erase a
  //            live operator message — including SELF_RESOLVED's "BEGIN to
  //            resume" prompt, which is the 1.16R finding-1 interlock telling
  //            the operator a locomotive is waiting for them. A diagnostic
  //            must never overwrite a safety prompt. The state/cto event is
  //            the record; the console renders it.
  if(now-ctoLastChanCheckMs>=1000){
    ctoLastChanCheckMs=now;
    uint8_t ch=ctoWifiChannel();
    if(ch==0){
      ctoChannelCand=0; ctoChannelConfirm=0;   // unknown: hold the reference
    }else if(ctoChannel==0){
      ctoChannel=ch;                            // first association: latch
    }else if(ch!=ctoChannel){
      if(ch==ctoChannelCand) ctoChannelConfirm++;
      else { ctoChannelCand=ch; ctoChannelConfirm=1; }
      if(ctoChannelConfirm>=3){
        uint8_t from=ctoChannel;
        ctoChannel=ch; ctoChannelChanges++;
        ctoChannelCand=0; ctoChannelConfirm=0;
        // Only meaningful where peers exist. Running solo with CTO off, a
        // channel move is nobody's business — and the old text asserted
        // "peers cannot hear this locomotive" with no peers to speak of.
        if(ctoEnabled){
          char b[160];
          snprintf(b,sizeof(b),
            "{\"event\":\"CTO_CHANNEL_CHANGED\",\"from\":%u,\"to\":%u,\"changes\":%lu,"
            "\"note\":\"PEERS_ON_OTHER_CHANNEL_CANNOT_HEAR_ME\"}",
            (unsigned)from,(unsigned)ch,(unsigned long)ctoChannelChanges);
          pub(T_ST_CTO,b,false);
        }
      }
    }else{
      ctoChannelCand=0; ctoChannelConfirm=0;    // back to steady state
    }
  }
  if(now-ctoLastTxMs>=CTO_TX_INTERVAL_MS){ ctoLastTxMs=now; ctoTxStatus(); }
  if(now-ctoLastEchoMs>=CTO_ECHO_INTERVAL_MS){ ctoLastEchoMs=now; ctoTxEcho(); }
  ctoEvaluateRoles();
  ctoServiceEchoCheck();
  ctoServiceFleetStop();
  // Round 2 finding 1 — continuous enforcement. Request-time capping cannot
  // see a peer event that arrives during steady cruise (no request fires
  // when commandedPwm already equals the cruise target). Re-derive the cap
  // from the persistent uncapped intent every pass: a new stop condition
  // bites within one loop, and a lifted one restores the desired speed —
  // the traffic-resume behaviour of spec §7. AUTO chamber only; E-stop and
  // NEUTRAL are enforced downstream in servicePwmRamp exactly as before.
  if(autoRunning && !estopped){
    int cap=ctoLimitPwm(ctoDesiredPwm);
    if(cap!=commandedPwm){
      // Braking uses the ONE profile (round 3); a restore is not braking.
      pwmStepMs = (cap<commandedPwm) ? STATION_DOWN_STEP_MS : NORMAL_STEP_MS;
      commandedPwm=cap;
    }
  }
  // 5 s state heartbeat so the console can render the layer without waiting
  // for a transition.
  if(now-ctoLastStateMs>=5000){
    ctoLastStateMs=now;
    int8_t pi=ctoPartnerIdx();
    // SIZE THIS BY ARITHMETIC, NOT BY OBSERVATION. Third time this buffer has
    // been the site of the same bug: CODEX's 1.14A review found 224 was ~14
    // short, and 1.16Ra's first draft appended four keys to the 288 without
    // redoing the sum — measured worst case 301 into 287 usable, truncating
    // mid-key with no closing brace. The failure on the railway is not an
    // error but SILENCE: the console's json.loads sits in a bare try/except,
    // so a truncated payload is dropped and the CTO panel freezes on stale
    // values while the locomotive believes it is reporting. That is the worst
    // possible failure for a diagnostic added to explain an unexplained stop,
    // and it would not appear until counters crossed ~1e9.
    // MEASURED worst case (every field maximal, gap_ahead at INT_MIN, by
    // running this exact format string — not by summing it in my head, which
    // is how it went wrong): 300 bytes, now 320 with the CE mission field
    // (",\"mission\":\"EXPRESS\"" is 20 at its widest). 384 leaves 64 spare.
    // The guard below is the backstop.
    char b[384];
    int n=snprintf(b,sizeof(b),
      "{\"event\":\"CTO_STATUS\",\"on\":%d,\"role\":\"%s\",\"partner\":%lu,"
      "\"expected\":%lu,\"fleet_hold\":%d,\"traffic\":%u,\"gap_ahead\":%d,"
      "\"front_b\":%u,\"rear_b\":%u,\"rx\":%lu,\"tx\":%lu,\"txe\":%lu,\"rxd\":%lu,"
      // 1.16Ra: tx is INTENTIONS, txd/txf are what the MAC layer did, ch is
      // the channel those frames went out on. Comparing my ch against the
      // peer's ch is the whole diagnosis for a silent-partner incident.
      // CE mission rides the status, not only the CE_BEGIN/CE_END events: a
      // console that joins mid-mission, or reconnects, has no event to replay
      // and would otherwise show a locomotive running a mission as "unpaired",
      // which is true of its pairing and useless about what it is doing.
      "\"txd\":%lu,\"txf\":%lu,\"ch\":%u,\"chg\":%lu,\"mission\":\"%s\"}",
      ctoEnabled?1:0,
      ctoRole==CTO_ROLE_LEADER?"LEADER":ctoRole==CTO_ROLE_FOLLOWER?"FOLLOWER":"NONE",
      (unsigned long)ctoPartnerId,(unsigned long)ctoExpectedId,
      ctoFleetHold?1:0,(unsigned)ctoTraffic,
      (pi>=0&&ctoPeerFresh(ctoPeers[pi])&&navDir!=MAP_UNSET)?(int)ctoGapAhead(ctoPeers[pi]):-1,
      (navDir!=MAP_UNSET)?ctoMyFrontB():navMm,(navDir!=MAP_UNSET)?ctoMyRearB():navMm,
      (unsigned long)ctoRxAccepted,(unsigned long)ctoTxAttempts,
      (unsigned long)ctoTxErrors,(unsigned long)ctoRxDropped,
      (unsigned long)ctoTxDone,(unsigned long)ctoTxFailed,
      (unsigned)ctoChannel,(unsigned long)ctoChannelChanges,ceMissionName());
    // Structural guard, so arithmetic can never be the only defence again: a
    // truncated payload is malformed JSON and must not be published at all.
    // Losing one heartbeat is recoverable; poisoning the diagnostic stream
    // with a payload consumers silently drop is not.
    if(n<0 || (size_t)n>=sizeof(b)) ctoStatusTruncated++;
    else pub(T_ST_CTO,b,false);
  }
}

void setup(){
  Serial.begin(115200); delay(300);
  Serial.printf("\n[BOOT] %s — %s\n",SKETCH_NAME,LOCO_NAME);
  irEnsureQueue();        // IR TEST A §5.2: dedicated 32-entry queue; no-op on Otto.
#if IR_TEST_A_ON
  // §2: an unusable sensor MAC disables reception AND IS REPORTED — the
  // review found the only evidence was a counter climbing inside MQTT.
  // And bench pairing step 1 needs Toby's own STA MAC printed somewhere;
  // this is the somewhere (WiFi.macAddress() is valid once the mode is set,
  // which happens below — so the MAC line prints from networkTask's first
  // connect instead; here we report the configured sensor side).
  Serial.printf("[IR] Test A ON  sensor MAC %02X:%02X:%02X:%02X:%02X:%02X  %s\n",
    IR_SENSOR_MAC[0],IR_SENSOR_MAC[1],IR_SENSOR_MAC[2],
    IR_SENSOR_MAC[3],IR_SENSOR_MAC[4],IR_SENSOR_MAC[5],
    irSensorMacUsable()?"(usable)":"UNSET/UNUSABLE - IR RECEPTION DISABLED");
#endif

  analogReadResolution(12);
  pinMode(HALL_PIN,INPUT);
  pinMode(MOTOR_DIR_PIN,OUTPUT);
  applyDirection();          // never leave the pin at its power-up level
  pwmAttachCompat(); pwmWriteCompat(0);

  buildTopics();
  // Before calibrate(), so the two-second baseline window is the last thing
  // between boot and a ready detector. A faulted sensor only prints and moves
  // on — decision 0012 requires boot to tolerate a missing INA219.
  ina219Setup();
  calibrate();

  // 256 slots is ~5 min of headroom at cruise (~1.1 s/marker), up from the 32
  // (~35 s) that overflowed and destroyed 67 marker events on 2026-07-29. This
  // does not fix a stall; it converts a data-destroying failure into a merely
  // delayed one -- survivable, because detectedAtMs is stamped at detection.
  eventQueue=xQueueCreate(256,sizeof(MarkerEvent));
#ifdef TEMPLATES_ADMISSION
  rejectQueue=xQueueCreate(64,sizeof(AdmitReject));   // ~10 B/slot; audit only
#endif
  if(!eventQueue){ Serial.println("[FATAL] queue alloc failed"); while(1) delay(1000); }
  if(xTaskCreatePinnedToCore(hallTask,"hallTask",4096,nullptr,2,nullptr,0)!=pdPASS){
    Serial.println("[FATAL] hall task creation failed"); while(1) delay(1000);
  }

  // The MQTT queues must exist before pub()/pubMarker() or the callback can be
  // reached. markerPubQueue: 128 slots (F5 — the marker path now carries the
  // raw event, its AGREE/DISAGREE, and any decision event, ~2+ messages per
  // marker; 128 at ~520 B is ~66 KB of heap, still a minute of markers at
  // cruise). pubQueue stays 32: it filling under a degraded link is correct
  // behaviour, not the problem.
  pubQueue=xQueueCreate(32,sizeof(PubMsg));         // ~17 KB
  markerPubQueue=xQueueCreate(128,sizeof(PubMsg));  // ~66 KB heap, marker path only
  cmdQueue=xQueueCreate(16,sizeof(CmdMsg));
  { struct CtoRxItem { uint8_t b[sizeof(CtoPeerPacket)]; int l; };
    ctoRxQueue=xQueueCreate(8,sizeof(CtoRxItem)); }   // LAYER 5 radio inbox
  if(!pubQueue||!markerPubQueue||!cmdQueue){ Serial.println("[FATAL] mqtt queue alloc failed"); while(1) delay(1000); }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  // 0.3a: bounded confirmation only — never a precondition for anything
  // downstream. networkTask retries WiFi/MQTT/ESP-NOW forever regardless of
  // what happens in this loop; this just gives the operator a boot-time
  // verdict instead of silence (2026-08-27: TEMPLATES had none, and a WiFi
  // association failure was indistinguishable on serial from a healthy
  // UNSET boot for 200+ seconds).
  {
    unsigned long wt0=millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-wt0<8000UL) delay(200);
    if(WiFi.status()==WL_CONNECTED)
      Serial.printf("[NET] WiFi UP ip=%s rssi=%d ch=%d (%lu ms)\n",
        WiFi.localIP().toString().c_str(),WiFi.RSSI(),WiFi.channel(),
        millis()-wt0);
    else
      Serial.printf("[NET] WiFi NOT YET UP after %lu ms, status=%d — "
        "networkTask keeps trying\n",millis()-wt0,(int)WiFi.status());
  }
#if IR_TEST_A_ON
  // Bench pairing step 1 (sender spec §11.3): Toby's STA MAC, printed where
  // the operator can read it. Valid as soon as the mode is set.
  Serial.printf("[IR] my STA MAC %s — put this in ir_espnow_config.h TOBY_STA_MAC\n",
                WiFi.macAddress().c_str());
#endif
  mqtt.setServer(MQTT_BROKER,MQTT_PORT);
  mqtt.setSocketTimeout(2);
  // Bound the underlying TCP connect. On the installed ESP32 core (3.3.11)
  // WiFiClient is a typedef of NetworkClient, whose connect() timeout is set by
  // setConnectionTimeout(MILLISECONDS) -- NOT by setTimeout(), which on this
  // core is the inherited Stream read timeout and has no effect on connect().
  // The core default is already 3000 ms, so this is belt-and-suspenders and
  // makes the 3 s bound explicit and version-independent. See the v2.18 notes:
  // the 33 s stalls were almost certainly measured on an older 2.x core, where
  // the connect default was ~30 s and setTimeout()'s units were seconds.
  espClient.setConnectionTimeout(3000);   // ms; yields the ~3 s the spec asked for
  mqtt.setCallback(onMqttEnqueue);        // callback only enqueues; loop() runs handlers
  mqtt.setBufferSize(2048);

  // MQTT now lives entirely on its own task. loop() never calls a mqtt.* function
  // again, so a degraded link can no longer stall navigation.
  if(xTaskCreatePinnedToCore(networkTask,"net",8192,nullptr,1,nullptr,0)!=pdPASS){
    Serial.println("[FATAL] network task creation failed"); while(1) delay(1000);
  }

  Serial.println("[BOOT] ready. Set session_direction, then start_mm, then auto, then GO.");
}

static unsigned long loopPrevMs=0;
void loop(){
  unsigned long now=millis();
  unsigned long gap=now-loopPrevMs;
  if(gap>loopMaxGapMs) loopMaxGapMs=gap;
  loopPrevMs=now;

  // Not one of these can block on the network: MQTT runs on networkTask, and
  // every publish site only enqueues. serviceCommands() drains inbound commands
  // first (symmetric with the outbound queue) so a command and the markers it
  // affects are processed in the same pass, on this one thread.
  serviceCommands();
  serviceIrRx();          // IR TEST A §10: accept radio facts before this
                          // pass's Hall drain. Inert stub on Otto.
  drainMarkers();
  serviceIrTelemetry();   // §10: self-rate-limited 1 s / 5 s summaries.
  serviceStatusBroadcast();
  serviceWarningExpiry();
  publishSimpleStates();
  serviceStations();
  ctoService();        // LAYER 5: peer truth, roles, fleet stop. Loop thread.
  servicePwmRamp();
  // After servicePwmRamp, so the 5 s I2C read never sits between a command and
  // the motor write it asks for. Self-rate-limited; a run with no INA219
  // returns on the first line.
  serviceInaTelemetry();
  publishStat();
}
