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
#define SKETCH_NAME "QUORUM_1_14"

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
static const uint8_t  CTO_STOP_GAP_MARKERS    = 12;     // bound gap: begin decel-to-stop
static const uint8_t  CTO_SLOW_GAP_MARKERS    = 18;     // bound gap: pre-slow to zone speed
static const uint8_t  CTO_PAIR_RANGE_MARKERS  = 12;     // bound gap: Q1/Q2 latch range
static const uint8_t  CTO_TIE_BAND_MARKERS    = 12;     // long-range order hysteresis
static const uint8_t  CTO_ARRIVE_TOL_MARKERS  = 4;      // follower "at hold point" window
static const uint32_t CTO_RELEASE_DWELL_MS    = 10000;  // leader, after follower arrives
static const uint32_t CTO_FOLLOWER_DWELL_MS   = 20000;  // follower platform dwell
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
static uint32_t ctoFollowerArrivedMs=0;   // leader side: when follower confirmed at hold
static uint32_t ctoTxSeq=0, ctoLastTxMs=0, ctoLastEchoMs=0, ctoLastStateMs=0;
static uint32_t ctoRxAccepted=0, ctoTxAttempts=0, ctoTxErrors=0, ctoRxDropped=0;
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
      if(dur<EVENT_FLOOR_MS){ floorRejects++; return; }
      MarkerEvent e;
      e.polarity      = evOpenPole;
      e.peak          = evOpenPole?evPeakN:evPeakS;
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
static bool     ctoHoldDeparture();        // leader holds at platform for follower
static uint32_t ctoDwellMs();              // paired follower dwells 20 s, else DWELL_MS
static void     ctoService();              // 10 Hz: registry, roles, gates; loop thread
static void     ctoRadioInit();            // esp_now up, once, after WiFi connects
static void     ctoHandleClear(const char* msg); // cmd/cto — operator clear/off
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
static const int8_t QUORUM_OFFSETS[QUORUM_CANDIDATES] = { -1, 0, +1, +2, +3, +4 };

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
static unsigned long lastMarkerMs=0;             // EVERY received event advances this
static uint16_t lastSegmentDt=0;
// Published on every marker by drainMarkers() (§5): why the gate was inactive
// and what it computed, explicit rather than omitted.
static const char* lastTimingGate="NO_POSITION";
static uint32_t    lastDtExpected=0;
static float       lastDtConserveRatio=-1.0f;

// The last marker whose reading actually AGREED with the map. Everything after
// it is inference; this is the position a following locomotive can trust.
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
static void invalidatePreviousAcceptedDt(){ previousAcceptedDtValid=false; }

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

// One decision function, adoption tested FIRST (§2.4): if reading twelve is
// what finally produces the two-point margin, the locomotive has identified
// its position and must adopt. The hard bound means "twelve readings without
// a margin", not "twelve readings then stop regardless".
static void decideEvaluation(){
  computeLeaderRunnerUpMargin();
  if(leaderIdx>=0 && runnerUpIdx>=0 && quorumMargin>=QUORUM_MARGIN){
    adoptLeader();
  }else if(evalCount>=QUORUM_MAX){
    enterNoQuorum("HARD_BOUND");
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
static void acceptEvent(const MarkerEvent& e){
  navMm=nextMm(navMm,navDir);
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
      // §2.5: deceleration events are real and are not pretended away. The
      // odometer advances and the ring appends — the snapshot already
      // preserved what mattered — but no scoring, no adoption, no
      // last-confirmed update, no state exit, no repeated stop or alert.
      if(lostMarkers<65535) lostMarkers++;
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
  fullRecoveryReset();
  lostMarkers=0;
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
static void navOnMarker(const MarkerEvent& e){
  navMarkers++;
  // Timed from DETECTION, not from when loop() got round to draining the
  // queue. Diagnostic only -- no PWM authority. (Codex)
  uint16_t dt = lastMarkerMs ? (uint16_t)min(e.detectedAtMs-lastMarkerMs,(unsigned long)65535) : 0;
  lastSegmentDt = dt;
  lastMarkerMs  = e.detectedAtMs;

  // §5: when timing values are unavailable they publish as 0 / -1.0, never
  // omitted. Overwritten below on an ACTIVE evaluation.
  lastDtExpected      = 0;
  lastDtConserveRatio = -1.0f;

  // Gate evaluation, in this exact fixed order (§3, §5):
  //   NO_POSITION -> NO_DIR -> LOW_PWM -> RAMP -> NO_PREV -> ACTIVE
  if(navState==NAV_UNSET){
    // Direction may be known; position is not. No advance, no ring, no
    // last-confirmed, no conservation — navMm holds nothing meaningful to
    // index spacingMm[] with — and no timing history to invalidate.
    lastTimingGate="NO_POSITION";
    return;
  }
  if(navDir==MAP_UNSET){
    // Received, NOT accepted: there is no direction to advance in.
    // nextMm(mm,MAP_UNSET) returns mm unchanged — it would silently push a
    // duplicate ring entry, corruption rather than a crash. Defensive, not a
    // reachable operational path: both declaration handlers refuse to declare
    // without a direction. Do not reorder them to make this reachable.
    lastTimingGate="NO_DIR";
    invalidatePreviousAcceptedDt();
    return;
  }
  if(e.pwmActualAtDetect < GATE_LOW_PWM_FLOOR){
    // Below the velocity model's validity: accept the advance, and the
    // interval must not seed a later conservation test.
    lastTimingGate="LOW_PWM";
    invalidatePreviousAcceptedDt();
    acceptEvent(e);
    return;
  }
  if(abs((int)e.pwmActualAtDetect-(int)e.pwmCommandedAtDetect) > GATE_RAMP_DELTA){
    // Mid-ramp — both values from the SAME instant, event open (§3). The
    // event-time pair is what makes this a measurement: a stop request
    // between detection and drain cannot turn a steady 100/100 into "RAMP".
    lastTimingGate="RAMP";
    invalidatePreviousAcceptedDt();
    acceptEvent(e);
    return;
  }
  if(!previousAcceptedDtValid){
    // NO_PREV is a BOOTSTRAP, not a suspension — it ESTABLISHES the
    // predecessor rather than invalidating it (getting this backwards
    // livelocks the gate and conservation never runs). A zero dt — the first
    // event of a session — must never become a predecessor: the next genuine
    // marker would test 0+dt against one expected interval and be rejected.
    lastTimingGate="NO_PREV";
    if(dt>0){ previousAcceptedDt=dt; previousAcceptedDtValid=true; }
    acceptEvent(e);
    return;
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
  float velocityMmPerSec = VEL_MODEL_SLOPE*(float)e.pwmActualAtDetect + VEL_MODEL_INTERCEPT;
  // The 1000 is not optional: the model is mm/s, dt is MILLISECONDS. The unit
  // travels with the name (§3), though the payload field stays dt_expected.
  float expectedDtMs = (1000.0f*(float)pgm_read_word(&spacingMm[conserveIntervalIndex]))
                       / velocityMmPerSec;
  float combinedDtMs = (float)dt + (float)previousAcceptedDt;
  lastDtExpected      = (uint32_t)expectedDtMs;
  lastDtConserveRatio = (expectedDtMs>0.0f) ? (combinedDtMs/expectedDtMs) : -1.0f;
  // fabsf, not abs: the Arduino abs() macro truncates floats (§3).
  float errorMs = fabsf(combinedDtMs-expectedDtMs);
  if(errorMs <= DT_CONSERVE_TOL*expectedDtMs){
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
    return;
  }
  // Every accepted ACTIVE event replaces the predecessor — the variable means
  // "the interval of the LAST ACCEPTED event", so it advances with each
  // acceptance; only PHANTOM_REJECTED leaves it unchanged (§3).
  previousAcceptedDt=dt; previousAcceptedDtValid=true;
  acceptEvent(e);
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
  if(o <  APPROACH_START) return CRUISE_PWM;
  if(o >= ZONE_START)     return zonePwm;
  const int steps = (int)ZONE_START - (int)APPROACH_START;      // 5
  const int idx   = (int)(o - APPROACH_START) + 1;              // 1..5
  return (int)CRUISE_PWM - ((int)CRUISE_PWM - (int)zonePwm)*idx/steps;
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
      int8_t prevDir = navDir;
      navDir=derived;
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
  return CRUISE_PWM;
}

// LAYER 5 (CODEX 1.14 review, finding 1): the CTO limiter lives HERE, inside
// the only two writers of commandedPwm, so no AUTO request path can restore
// speed past an active traffic cap, fleet stop or role conflict — the
// original five call-site wrappers missed ST_APPROACH/ST_FINAL, which
// re-request zone/final speed every pass and were overwriting a CTO stop
// within one loop. AUTO chamber only: MANUAL authority is untouched (§0.2),
// and a cap can only lower a request, so E-stop and NEUTRAL behave exactly
// as before.
static void requestPwm(int target,uint16_t stepMs){
  int t=constrain(target,0,255);
  if(autoRunning) t=ctoLimitPwm(t);
  commandedPwm=t;
  pwmStepMs=stepMs;
}

// Reach the target in roughly durationMs, whatever the current PWM. Derives
// the step rate from the distance still to travel.
static void requestPwmOver(int target,uint16_t durationMs){
  int t=constrain(target,0,255);
  if(autoRunning) t=ctoLimitPwm(t);
  int delta=abs(t-actualPwm);
  commandedPwm=t;
  pwmStepMs = (delta>0) ? (uint16_t)max(5UL,(unsigned long)durationMs/(unsigned long)delta) : 50;
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
        stIndex=(int8_t)i; stationSetPhase(ST_APPROACH);
        requestPwmOver(approachTargetForOffset(o,STATIONS[i].zonePwm),APPROACH_RAMP_MS);
        stationPublish("ARMED",o,(navState==NAV_NORMAL)?"RANGE_ARM_NORMAL":"RANGE_ARM_EVALUATING");
        return;
      }
    }
    // No station armed: keep cruise following the section map through the
    // normal path. requestPwmOver() remains the sole writer of commandedPwm.
    int want = cruiseForPosition();
    if(commandedPwm != want) requestPwmOver(want, APPROACH_RAMP_MS);
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
      }else if(o>=ZONE_START && o<0){
        requestPwmOver(STATIONS[stIndex].zonePwm,APPROACH_RAMP_MS);
        stationPublish("ZONE_HOLD",o,"HOLD_60");
      }else if(o>=0){
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
      const int8_t stopAt = STATIONS[stIndex].stopOffset;
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
      // LAYER 5: a paired LEADER completes its dwell and then HOLDS at the
      // platform until the follower is stopped at its hold point and the
      // 10 s release dwell has run (spec sec.8 step 3). ctoHoldDeparture()
      // is false in every solo/unpaired state.
      if(millis()-stDwellStartedMs>=ctoDwellMs() && ctoHoldDeparture()){
        stationPublish("HOLD_FOR_FOLLOWER",o,"CTO_LEADER_AWAITING_RELEASE");
        stDwellStartedMs=millis()-ctoDwellMs(); // re-arm; publish dedup guards spam
        break;
      }
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
      if(o>=STATIONS[stIndex].stopOffset+3){
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
static char T_CMD_STARTMM[64],T_CMD_ESTOP[64],T_CMD_ESTOP_ALL[64],T_CMD_THROTTLE[64],T_CMD_STARTINT[64],
            T_CMD_RELEASE[64],T_CMD_FORCELOST[64],T_CMD_CTO[64];
static char T_ST_CTO[64];   // LAYER 5 state topic: role, partner, gaps, holds

static void buildTopics(){
  const char* id=LOCO_NAME;
  snprintf(T_ONLINE ,64,"ngr/loco/%s/online"        ,id);
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
  char sc[48], ex[48];
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
  char sc[48], ex[48], vb[48];
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
    "\"min_peak\":%d,\"floor_ms\":%lu,\"baseline\":\"median_%d_at_%lums\","
    "\"quorum_trigger\":%d,\"quorum_margin\":%d,\"quorum_max\":%d}",
    SKETCH_NAME,LOCO_NAME,(int)HALL_DEADBAND_COUNTS,(int)HALL_ENTRY_MARGIN_COUNTS,
    (int)HALL_MIN_PEAK_DELTA,EVENT_FLOOR_MS,MEDIAN_WINDOW,MEDIAN_SAMPLE_MS,
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
    if(navDir==MAP_UNSET){
      stationPublish("START_INTERVAL_REFUSED",0,"SET_SESSION_DIRECTION_FIRST"); return;
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
    {
      autoRunning=true;
      motorDirection=DIRECTION_FORWARD; applyDirection();
      requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
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
    stationReset("DISPATCHER_RELEASE");
    publishWarning("RELEASED: dispatcher control ended");
    navPublishState("DISPATCHER_RELEASE",nullptr);
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
static void attemptReconnect(){
  unsigned long now=millis();
  if(now<nextMqttTryMs) return;
  nextMqttTryMs=now+5000UL;
  unsigned long t0=millis();
  mqttAttempts++;
  bool ok=mqtt.connect(LOCO_NAME,T_ONLINE,0,true,"0");
  mqttConnectMs=millis()-t0;
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
static void drainMarkers(){
  MarkerEvent e;
  // Sample occupancy BEFORE draining: this is how deep the queue got while the
  // previous loop pass was busy (e.g. blocked in serviceNetwork). Windowed max,
  // reset by publishStat.
  if(eventQueue){
    UBaseType_t waiting=uxQueueMessagesWaiting(eventQueue);
    if(waiting>queueHighWater) queueHighWater=(uint16_t)waiting;
  }
  while(eventQueue && xQueueReceive(eventQueue,&e,0)==pdTRUE){
    navOnMarker(e);
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
// dwell (ctoHoldDeparture/ctoDwellMs), and that is all. In MANUAL it only
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

// ---- radio -----------------------------------------------------------------
static void ctoOnRecv(const esp_now_recv_info_t*,const uint8_t* data,int len){
  // WiFi-task context: copy and leave, exactly like onMqttEnqueue. All state
  // belongs to the loop thread.
  if(len!=(int)sizeof(CtoPeerPacket) && len!=(int)sizeof(Cto3RoleEcho)) return;
  uint8_t buf[sizeof(CtoPeerPacket)]; // echo is smaller; length rides the queue item
  memcpy(buf,data,len);
  struct { uint8_t b[sizeof(CtoPeerPacket)]; int l; } item;
  memcpy(item.b,buf,len); item.l=len;
  if(ctoRxQueue && xQueueSend(ctoRxQueue,&item,0)!=pdTRUE) ctoRxDropped++;
}
static void ctoRadioInit(){
  if(ctoRadioUp) return;
  if(esp_now_init()!=ESP_OK){ Serial.println("[CTO] esp_now init FAILED"); return; }
  esp_now_peer_info_t pi={}; memcpy(pi.peer_addr,CTO_BCAST,6);
  pi.channel=0; pi.encrypt=false;
  if(!esp_now_is_peer_exist(CTO_BCAST)) esp_now_add_peer(&pi);
  esp_now_register_recv_cb(ctoOnRecv);
  ctoRadioUp=true;
  Serial.println("[CTO] radio up (ESP-NOW, broadcast)");
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
  q.seen=true; q.id=p.senderId; q.seq=p.sequence; q.rxMs=millis();
  q.hallMm=p.hallMm; q.frontB=p.frontBoundaryMm; q.rearB=p.rearBoundaryMm;
  q.mapDir=p.mapDir; q.autoMode=p.autoMode; q.running=p.running;
  q.motionState=p.motionState;
  q.rampFalling=(q.seen && p.rampPwm+2 < q.rampPwm);   // 2-count hysteresis
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
static void ctoDissolve(const char* why){
  if(ctoRole!=CTO_ROLE_NONE){
    char b[96]; snprintf(b,sizeof(b),"{\"event\":\"CTO_UNPAIRED\",\"why\":\"%s\",\"partner\":%lu}",why,(unsigned long)ctoPartnerId);
    pub(T_ST_CTO,b,false);
  }
  ctoRole=CTO_ROLE_NONE; ctoPartnerId=0; ctoPairEpochMs=0; ctoPairDir=MAP_UNSET;
  ctoFollowerArrivedMs=0; ctoEchoConflict=false; ctoEchoConfirmed=false;
}
static void ctoLatch(CtoRole r,uint32_t partner){
  ctoRole=r; ctoPartnerId=partner; ctoPairEpochMs=millis();
  ctoPairDir=navDir;
  ctoFollowerArrivedMs=0; ctoEchoConflict=false;
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
  if(!ctoEnabled || navDir==MAP_UNSET || !navPositionUsable()){ return; }
  int8_t pi=ctoPartnerIdx();
  if(ctoRole!=CTO_ROLE_NONE){
    // dissolution paths; staleness does NOT silently dissolve — fleet stop
    // owns that (0031). ANY direction change does: comparing only against
    // the partner missed the case where BOTH locomotives reverse — the
    // directions agree again while the physical front/rear order has
    // inverted (CODEX 1.14 review, finding 4). The direction at latch is
    // the reference, so one reversing or both reversing dissolves alike.
    if(navDir!=ctoPairDir)
      ctoDissolve("DIRECTION_CHANGED_SINCE_LATCH");
    else if(pi>=0 && ctoPeerFresh(ctoPeers[pi]) && !ctoPeerSameDir(ctoPeers[pi]))
      ctoDissolve("PARTNER_DIRECTION_CHANGED");
    return;                                    // latched roles persist (0032)
  }
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
  // leader-designate holding at its next station via ctoHoldDeparture().
}
static bool ctoProvisionalLeader(){
  if(ctoRole==CTO_ROLE_LEADER) return true;
  if(ctoRole==CTO_ROLE_FOLLOWER) return false;
  if(!ctoEnabled||navDir==MAP_UNSET||!navPositionUsable()) return false;
  for(uint8_t i=0;i<CTO_MAX_PEERS;i++){
    CtoPeer& p=ctoPeers[i];
    if(!ctoPeerFresh(p)||!ctoPeerSameDir(p)||!ctoPeerNavOk(p)) continue;
    uint16_t myBehind=ctoGapBehind(p);      // arc from his front to my rear
    uint16_t hisBehind=ctoGapAhead(p);      // arc from my front to his rear
    int32_t diff=(int32_t)myBehind-(int32_t)hisBehind;
    if(diff<-(int32_t)CTO_TIE_BAND_MARKERS) return true;    // clearly smaller behind-arc
    if(diff>(int32_t)CTO_TIE_BAND_MARKERS)  return false;
    return LOCO_ID < p.id;                                   // tie band: lower ID leads
  }
  return false;
}

// ---- the three hooks the station machine consults ---------------------------
static uint32_t ctoDwellMs(){
  // Paired follower at a platform dwells 20 s (operator, supersedes 15) —
  // but only on a CONFIRMED pairing (finding 5); unconfirmed runs solo rules.
  return (ctoRole==CTO_ROLE_FOLLOWER && ctoEchoConfirmed)?CTO_FOLLOWER_DWELL_MS:DWELL_MS;
}
static bool ctoHoldDeparture(){
  if(!ctoEnabled) return false;
  if(ctoFleetHold) return true;              // never depart into a fleet stop
  if(ctoEchoConflict) return true;           // 0032: disagreement -> both hold
  // Formation: an unpaired provisional leader waits at its station for the
  // follower to close (spec sec.6 step 2). Worst mis-tie: both wait. Safe.
  if(ctoRole==CTO_ROLE_NONE) return ctoProvisionalLeader();
  if(ctoRole!=CTO_ROLE_LEADER) return false;
  // Finding 5 / 0034 as revised: the RELEASE choreography — departing a
  // platform with a follower parked behind — runs only on a CONFIRMED
  // reciprocal pairing. Unconfirmed (older peer, stale echo) means keep
  // holding: safe, visible, and it degrades a mixed-version bubble to
  // operator-supervised running instead of unconfirmed automation.
  if(!ctoEchoConfirmed) return true;
  int8_t pi=ctoPartnerIdx();
  if(pi<0||!ctoPeerFresh(ctoPeers[pi])) return true;   // no fresh partner: hold (0031 will also fire)
  CtoPeer& p=ctoPeers[pi];
  // follower "arrived": stopped, at the hold gap behind me (positive evidence)
  bool arrived = ctoPeerStopping(p) && p.motionState==0 &&
                 ctoGapBehind(p) <= (uint16_t)(CTO_CLEAR_GAP_MARKERS+CTO_ARRIVE_TOL_MARKERS);
  if(!arrived){ ctoFollowerArrivedMs=0; return true; }
  if(ctoFollowerArrivedMs==0){
    ctoFollowerArrivedMs=millis();
    pub(T_ST_CTO,"{\"event\":\"CTO_FOLLOWER_ARRIVED\"}",false);
  }
  return (millis()-ctoFollowerArrivedMs) < CTO_RELEASE_DWELL_MS;   // 10 s release dwell
}
static int ctoLimitPwm(int want){
  // Universal traffic protection (spec sec.7): one deceleration profile; the
  // flag that differs is simply WHICH machine resumes — station or cruise —
  // and both already exist above this layer. This function only caps.
  if(!ctoEnabled) return want;
  if(ctoFleetHold) return 0;
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

// ---- fleet stop (decision 0031: by absence, never announcement) ------------
static void ctoServiceFleetStop(){
  if(!ctoEnabled || ctoExpectedId==0){ ctoFleetHold=false; return; }
  int8_t i=ctoPeerIdx(ctoExpectedId);
  bool ok = (i>=0) && ctoPeerFresh(ctoPeers[i]) && ctoPeerNavOk(ctoPeers[i]);
  if(!ok && !ctoFleetHold){
    ctoFleetHold=true;
    if(autoRunning) requestPwm(0,NORMAL_STEP_MS);   // AUTO chamber only (§0.2)
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
  // Universal traffic protection never consults this; only the release and
  // dwell choreography does (ctoHoldDeparture, ctoDwellMs).
  if(ctoRole==CTO_ROLE_NONE){ ctoEchoConflict=false; ctoEchoConfirmed=false; return; }
  int8_t i=ctoPartnerIdx();
  if(i<0){ ctoEchoConfirmed=false; return; }
  CtoPeer& p=ctoPeers[i];
  bool echoFresh = p.echoRxMs!=0 && (uint32_t)(millis()-p.echoRxMs)<=2*CTO_PEER_STALE_MS;
  if(!echoFresh){ ctoEchoConfirmed=false; return; }          // no verdict, and not confirmed
  uint8_t expectRole=(ctoRole==CTO_ROLE_LEADER)?(uint8_t)CTO_ROLE_FOLLOWER:(uint8_t)CTO_ROLE_LEADER;
  ctoEchoConfirmed = (p.echoRole==expectRole && p.echoPartner==LOCO_ID);
  bool conflict = (p.echoRole==(uint8_t)ctoRole && p.echoPartner==LOCO_ID);
  if(conflict && !ctoEchoConflict){
    ctoEchoConflict=true;
    if(autoRunning) requestPwm(0,NORMAL_STEP_MS);
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
  while(msg[n] && msg[n]!='\r' && msg[n]!='\n' && msg[n]!=' ' && n<sizeof(pay)-1){ pay[n]=msg[n]; n++; }
  pay[n]=0;
  if(!strcmp(pay,"clear")){
    for(uint8_t i=0;i<CTO_MAX_PEERS;i++) ctoPeers[i]=CtoPeer{};
    ctoDissolve("OPERATOR_CLEAR");
    ctoExpectedId=0; ctoFleetHold=false; ctoTraffic=CTRAF_CLEAR; ctoTrafficForId=0;
    pub(T_ST_CTO,"{\"event\":\"CTO_CLEARED\"}",false);
  }else if(!strcmp(pay,"off")){
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
  if(now-ctoLastTxMs>=CTO_TX_INTERVAL_MS){ ctoLastTxMs=now; ctoTxStatus(); }
  if(now-ctoLastEchoMs>=CTO_ECHO_INTERVAL_MS){ ctoLastEchoMs=now; ctoTxEcho(); }
  ctoEvaluateRoles();
  ctoServiceEchoCheck();
  ctoServiceFleetStop();
  // 5 s state heartbeat so the console can render the layer without waiting
  // for a transition.
  if(now-ctoLastStateMs>=5000){
    ctoLastStateMs=now;
    int8_t pi=ctoPartnerIdx();
    char b[224];
    snprintf(b,sizeof(b),
      "{\"event\":\"CTO_STATUS\",\"on\":%d,\"role\":\"%s\",\"partner\":%lu,"
      "\"expected\":%lu,\"fleet_hold\":%d,\"traffic\":%u,\"gap_ahead\":%d,"
      "\"front_b\":%u,\"rear_b\":%u,\"rx\":%lu,\"tx\":%lu,\"txe\":%lu,\"rxd\":%lu}",
      ctoEnabled?1:0,
      ctoRole==CTO_ROLE_LEADER?"LEADER":ctoRole==CTO_ROLE_FOLLOWER?"FOLLOWER":"NONE",
      (unsigned long)ctoPartnerId,(unsigned long)ctoExpectedId,
      ctoFleetHold?1:0,(unsigned)ctoTraffic,
      (pi>=0&&ctoPeerFresh(ctoPeers[pi])&&navDir!=MAP_UNSET)?(int)ctoGapAhead(ctoPeers[pi]):-1,
      (navDir!=MAP_UNSET)?ctoMyFrontB():navMm,(navDir!=MAP_UNSET)?ctoMyRearB():navMm,
      (unsigned long)ctoRxAccepted,(unsigned long)ctoTxAttempts,
      (unsigned long)ctoTxErrors,(unsigned long)ctoRxDropped);
    pub(T_ST_CTO,b,false);
  }
}

void setup(){
  Serial.begin(115200); delay(300);
  Serial.printf("\n[BOOT] %s — %s\n",SKETCH_NAME,LOCO_NAME);

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
  drainMarkers();
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
