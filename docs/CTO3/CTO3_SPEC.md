# CTO3 Implementation Specification — Draft 1

Status: draft specification for review (Sam, CODEX)
Scope: NGR Lowline only
Baseline: `docs/CTO3/CTO3_INTENT_BASELINE.md` (2026-08-05) governs; this spec
implements it and must not contradict it. Where they differ, the baseline and
the operator's clarifications win, and the difference is recorded.
Foundation: QUORUM 1.x navigation. CTO3 consumes QUORUM position/belief; it does
not reimplement marker interpretation.

---

## §0 Purpose and stance

CTO3 makes NGR Lowline locomotives autonomous, position-aware participants in
shared-track operation. It replaces block-era CTO and the failed CTO2 pairing
experiments with **traffic control between moving physical envelopes**, built on
trustworthy QUORUM position.

Operating stance (from CTO Operational Principles, July 2026): **built for
operational success, not self-defeating refusal.** A locomotive does not earn
the right to exist through a fresh Hall hit or a confidence threshold; dispatcher
setup is valid operational truth, and evidence refines it rather than erasing it.
The one inviolable inversion of this (from the CTO2 failure): **a failure to
recognize or qualify traffic is never converted into clearance.** Missing, stale,
or rejected peer information means "unknown," which is treated conservatively —
never "clear."

CTO3 is the **turntable, not the record.** The expanding/contracting bubble is
the first routine it plays. No bubble-specific choreography may be embedded in
navigation, communications, movement, or safety such that other routines cannot
reuse those services.

---

## §1 Architectural layers (normative)

Seven layers. Dependencies flow downward; higher layers request outcomes, lower
layers retain authority over truth and safety. A higher layer may request
movement; it may never override navigation validity, physical-envelope
protection, or E-stop.

1. **Evidence acquisition** — Hall, IR wheel sensor, INA219, future sensors.
2. **Navigation truth** — QUORUM belief + future Hall/IR fusion. Owns position,
   direction, confidence.
3. **Self-truth & peer registry** — ESP-NOW publication of own state; receipt
   and freshness-tracking of peer state.
4. **Safety authority** — envelope separation, route conflicts, confidence and
   freshness gating, stopping constraints. Holds veto over all movement.
5. **Movement services** — speed governor and stopping primitives (the
   staircase). Executes targets; owns PWM actuation within safety limits.
6. **Mission engine** — replaceable routines: destinations, waits, dependencies,
   skips, route requests. The bubble is one mission.
7. **Command adapters** — dispatcher controls now; richer missions and voice
   later. All translate to the same validated structured missions.

**Layer boundary rule:** each layer reads the one persistent operational state
below it. Layers do not each invent their own eligibility gate (July principle
2). One navigation truth, read by all.

---

## §2 Bicameral authority (constitutional — inherits QUORUM §0.2)

Two propulsion authorities on each locomotive:

- **MANUAL** — the operator controls propulsion directly. Sovereign. No CTO3
  layer commands PWM in manual.
- **AUTO** — onboard software controls propulsion while enrolled and running.

Crossings between chambers — **exactly four doors, no others:**

1. **E-STOP** — from either chamber, always, overrides everything.
2. **Enrollment (manual → auto)** — the locomotive's own act (`cmd/auto`), then
   dispatcher GO launches. Auto cannot seize an un-enrolled locomotive.
3. **Release (auto → manual)** — dispatcher END/RELEASE drops enrollment and
   running deterministically, returns propulsion to the operator.
4. **Dispatcher STOP** — halts an *enrolled* auto locomotive only; ignored by a
   manual locomotive (a dispatcher STOP must never zero a manual throttle).

**Navigation observes in both chambers.** Manual is navigation-*aware*, not
navigation-*dependent*: manual throttle/direction always work; if nav is set up,
MM counting, speedometer, and position stay live; if not, the loco still drives.

**Visibility is not a chamber privilege (July principle 4):** every powered
locomotive broadcasts self-truth regardless of chamber, motion, or station
state. A manually driven, stopped, dispatcher-stopped, or e-stopped locomotive
**must remain visible to traffic.** Manual authority must never make a physical
train disappear from other locomotives' awareness.

QUORUM 1.6 already implements the four doors and the STOP chamber-boundary; CTO3
builds on that and must not regress it.

---

## §3 Self-truth broadcast (ESP-NOW)

Every powered locomotive broadcasts self-truth continuously, independent of mode
and motion. Broadcasting is not a pairing service.

**Broadcast content (self-truth only):**

- identity;
- monotonic sequence number;
- navigation validity + truth source (dispatcher-declared / Hall-confirmed /
  fused);
- marker reference (last marker passed in the actual travel direction);
- **actual map travel direction** (see §7 — session direction XOR reverse), not
  the AUTO-assumed direction;
- running / stopped / motion state;
- measured speed when available, plus speed source (§8);
- **front and rear physical-envelope boundaries** (§4);
- consist configuration + version;
- navigation confidence and, as diagnostic, bounded position uncertainty.

**Reference-frame honesty (July principle 3):** a position between magnets is
reported as a *marker reference with a stated truth source*, never falsely
claimed as an exact Hall-on-magnet coordinate.

**Freshness contract:** each broadcast is timestamped/sequenced so receivers can
age it. Fields, exact packet layout, versioning, and freshness periods are
**unresolved** (§11) and require their own field-test-backed sub-spec.

---

## §4 Physical-envelope safety (the core constraint)

Traffic reasoning is about the **gap between occupied physical envelopes along a
valid route**, not point-to-point Hall distance.

**Envelope, measured from the Hall sensor (current CTO3 design values):**

- **18 inches forward**
- **48 inches behind**

These supersede all earlier symmetric 2 ft / 5 ft bubbles and marker-count
approximations. **They are configuration, not constants scattered through
routine logic**, and are keyed by consist configuration + version so different
train lengths coexist without hard-coded assumptions.

**Universal collision awareness (from NEW_PARADIGM):** every autonomous
locomotive — not just a designated pair — computes its own front boundary
against the rear boundary of any train ahead, and determines whether the gap is
opening, stable, or closing. Any number of same-direction trains can share track
and self-separate, like cars in traffic. Collision control is a property of
each locomotive, not of a pairing.

**Safety decisions must account for:** direction, speed, stopping behavior
(the staircase length, §6), navigation confidence, peer freshness, and the
relevant route.

**Conservative-by-default:** peer loss, loss of navigation confidence, or
inability to establish adequate separation causes deterministic conservative
behavior (slow/stop). The exact degradation state machine and stopping policy
are **unresolved** (§11). The invariant that *is* fixed: unknown traffic is
never treated as clear.

---

## §5 Peer registry

Each locomotive maintains a registry of every fresh broadcaster it hears.

- **Universal, not pairing-scoped:** it answers who exists, where, which way,
  and what space they occupy — for all peers, including locomotives outside any
  active routine.
- **No overwrite:** a report from a third locomotive must never overwrite
  another locomotive's stored record. One slot per identity.
- **Freshness-aged:** each record carries the age of its last update; stale
  records are marked stale, and stale ≠ absent ≠ clear.

Whether the initial two-train routine uses pairing, enrollment, a session
relationship, or a derived front/back topology is **unresolved** (§11). CTO2's
"eliminate pairing entirely" and "pairing is the only source of relevance" are
both retained as alternatives, neither adopted. **Permanent leader/follower
identity must not be a prerequisite for universal collision avoidance.**

---

## §6 Movement services — the staircase

Movement is expressed as **measured positional deceleration**, not "cut power
and coast." The station-stop staircase (from CTO2_BUBBLE_PRINCIPLE) is the
reusable primitive:

- Glide from cruise toward a low approach speed by a marker offset before target
  (e.g. ~15 pKPH at M−5);
- stepped reductions across the final markers (e.g. 15 → 10 at M+1 → 5 at M+2);
- final ramp to a full stop at a mapped point.

All reductions are **measured-speed** transitions with gradual PWM change, not
fixed PWM jumps; the final ramp rate is tunable. The same staircase serves a
platform stop, a hold behind a stopped train, or matching a slower train ahead —
**the geometry is placed wherever traffic or service requires a stop**, not only
at platforms.

**Reusable movement primitives (mission-callable):** proceed along an authorized
route; approach a mapped target; stop at a platform / hold point / arbitrary
mapped location; wait for time, state, or a peer condition; resume on condition;
pass or skip a stop; reverse/switch when topology permits.

Station stops are **mapped locations, not QUORUM positions** — a naming boundary
the baseline is explicit about.

---

## §7 Travel-direction derivation (operator-raised gap)

The DNA sequence is verified unique in **both** CW and CCW windows, but the
navigator must score against the **actual direction of travel**, which motor
reverse flips:

```
travel_direction = session_direction  XOR  motor_reverse
```

- Facing CW, forward → travel CW → CW map
- Facing CW, **reverse** → travel **CCW** → **CCW map**
- Facing CCW, forward → travel CCW → CCW map
- Facing CCW, **reverse** → travel **CW** → **CW map**

Self-truth (§3) publishes this derived travel direction, never the raw session
direction and never an AUTO-assumed direction (July principle 3). Whether
current QUORUM derives travel direction correctly, or scores against session
direction alone, must be **verified against source** before this becomes a
firmware change; if the derivation is missing, it is a QUORUM spec amendment
(travel_direction as above), then firmware, then CODEX — the standard path.

---

## §8 Speed control (SPEED_HOLD, not THROTTLE_HOLD)

**PWM is the actuator; actual speed (pKPH) is the controlled variable.** The
governing principle, in the operator's words: *the mile-marker speedometer owns
speed control whenever it is trustworthy; PWM is only the means of applying
correction; when the speedometer is not trustworthy, the train becomes more
conservative, not more aggressive.*

Commanded mode is **SPEED_HOLD**: operator/mission sets `target_pKPH`; the ESP32
varies PWM within safe limits to hold it. This is cruise control, not
"hold-the-pedal-halfway."

**Speed-source hierarchy:**

1. **HALL_VALID** — recent magnet intervals fresh → measured speed is truth,
   governs PWM.
2. **HALL_AGING** — last measurement getting stale → blend with model, cautious.
3. **PWM_ESTIMATE** — starting / crawling / post-reset, no fresh Hall → use the
   learned PWM-to-speed model (§9) as best estimate. **A source of truth, not
   the truth** — "a fraction without a denominator" if voltage is unknown (§9).
4. **MOTION_UNCONFIRMED** — PWM commands motion but no magnet arrives in the
   expected window.

**Hard safety law (the U-Haul-in-the-median rule, stated verbatim intent):**
the ESP32 must never respond to "speed reads zero / not moving" by escalating
PWM toward maximum. If PWM is high and no magnet arrives, declare
MOTION_UNCONFIRMED / STALL_SUSPECT — do not keep increasing PWM. If the speed
source is unreliable, do **not** operate as cruise control; fall back to a
cautious, bounded mode.

**Measured speed ceiling (empirical, not PWM-based):** navigation degrades by
*speed*, not throttle — the same PWM is fast downhill and slow uphill. Field
data: clean navigation at 50–69 pKPH, cascade failures at 78–82 pKPH.

- target: ~55 pKPH nominal;
- \> 70 pKPH sustained → SPEED_CAUTION;
- \> 75 pKPH → do not trust map updates (MAP_SUSPECT / protected);
- \> 80 pKPH → treat as navigation-unreliable, reduce PWM regardless of command.

This is what makes **clockwise automation viable**: CTO2/CE ran CCW-only because
one fixed PWM couldn't climb Viaduct Hill without overspeeding elsewhere.
Segment-aware SPEED_HOLD gives Viaduct Hill more PWM without making the loop run
fast — **automatic operation must not depend on one fixed PWM setting.**

**Control-loop constraint:** corrections must be smooth and bounded (no jumps
that could derail); the "increase by 5" placeholder is explicitly rejected as a
governing principle. Approaching a station/curve/truth point, lower the target
*before* arrival (predictive, §9).

**Speed floor (hard constraint from the navigation layer):** magnets every
300 mm mean measured speed is unavailable below ~2.5 s/marker. No mission,
governor, or traffic behavior may command a sustained crawl that pulls the
baseline and manufactures phantoms. Below the floor, speed comes from the PWM
model, and the train stays conservative.

---

## §9 Learned PWM↔speed model (fallback + prediction)

A **segment × direction × PWM → observed pKPH** table, built to serve
speed-source #3 and, later, predictive control.

**Keying:** locomotive, travel direction, segment (from-marker → to-marker),
PWM, observed pKPH, sample count, confidence. CW and CCW are separate — the same
grade reverses by direction.

**Voltage is a required dimension, not optional.** A 4S pack swings ~16.8→13.2 V;
PWM 35 on a fresh vs. tired pack is a materially different speed — plausibly
larger than grade at low PWM. Capture bus voltage per magnet interval (add
`v=<busVoltage>` to the mm/marker line alongside `pwm=`/`dist=`), and normalize:
`estimated_pKPH ≈ table_pKPH × (v_now / v_ref)` with v_ref ≈ 15.5 V, refined by
running the PWM ladder at start-of-pack and end-of-pack and inspecting residuals.
*(Depends on INA219 being restored — QUORUM 1.5; currently the subject of an
open Otto INA219 fault.)*

**Calibration grid (operator-specified):** dense where the fallback carries the
most load — the low-speed regime where Hall data is sparse. **PWM 25, 30, 35, 40,
50, 70**, both directions, full loop; densify only where residuals are nonlinear
(Viaduct Hill likely). Reject truth-pair short intervals, MISSED_MM,
INTERVAL_ANOMALY, MAP_SUSPECT, and speed-reset intervals.

**Storage (Pi = brain, ESP32 = distilled copy):** the Pi holds full history
(CSV/JSON/SQLite), generates `otto_speed_model.json` / `toby_speed_model.json`,
and compiles a compact per-loco table (~2 KB: base pKPH at reference PWM + slope
+ confidence per segment-direction) for the ESP32 to carry. **The Pi generates
offline; the locomotive executes autonomously. The Pi is never in the live
control loop.**

**Predictive control (later):** with the table, the locomotive anticipates —
adds PWM at the foot of a grade before it bogs (the EMD revving at the bottom of
the hill), backs off before a downhill curve, plans the Grillers approach from
its known downhill+curve profile rather than one generic braking curve.

---

## §10 Command sources and dispatcher

**Initial supervisory surface (narrow, deliberate):** Start, Stop, eStop,
Release to Manual. These do **not** make the dispatcher the live traffic
controller — movement permission and separation stay onboard. The dispatcher is
**authority, not intelligence**: a referee/gatekeeper that grants or denies and
enforces a declared plan, not a driver.

**AUTO enrollment checklist** (dashboard, from MANUAL_VS_AUTO): session
direction → start interval (set + confirm) → motor direction → enable AUTO →
GO active only after AUTO. Manual requires none of it.

**Future (architecture must admit, not yet build):** richer structured missions
(destinations, platform/HOLD positions, conditional waits, skips) and voice.
Voice is another entry method — speech → the *same* validated structured
missions. **Neither voice nor a future dispatcher commands PWM directly or
bypasses onboard safety.**

---

## §11 First routine: expanding/contracting bubble

Two same-direction trains around the loop, preserving established front/rear
order. At each of the four stations the front train proceeds to the next while
the rear is held at a safe earlier position; the front later departs and the
rear advances in — separation stretches and compresses like a slinky.

**Built from §6 primitives, expressed in mapped stops + peer state + envelopes**
— not resurrected blocks, not special-case motor commands. Under NEW_PARADIGM
this simplifies drastically: a train makes its normal platform stop unless a
train ahead requires an earlier stop; a follower stops wherever traffic requires
via the same staircase; "pairing" becomes optional service behavior (a platform
train *may* wait until any valid same-direction train has stopped behind before
dwelling and departing). Circuit Express becomes "temporarily suspend station
stops for the lead train" — traffic control handles the rest.

Exact station choreography, departure handshake, stop profiles, dwell
(configurable/randomized), and recovery transitions belong to the **routine
sub-spec**, not here.

---

## §12 Development sequence

1. **Verify QUORUM travel-direction (§7)** against source. Fix if needed
   (amendment → firmware → CODEX) before any auto reverse.
2. **Restore INA219 (QUORUM 1.5)** and resolve Otto's INA219 fault — §9 depends
   on it. Add `v=` to the mm/marker line.
3. **Station Stop v1** — single loco, single stop (Arches): prove glide → stop →
   dwell → reliable restart. No bubble. (Preserve QUORUM R21 single-loco station
   logic unchanged.)
4. **Calibration runs (§9)** — only after the route map is proven to hold; the
   table is only as good as the map under it. PWM ladder, both directions,
   voltage logged. Pi-side parser can be built in parallel (depends only on the
   fixed log format).
5. **SPEED_HOLD governor (§8)** — measured-speed cruise with the hard safety law,
   fallback hierarchy, and speed floor.
6. **Self-truth broadcast + peer registry (§§3,5)** — always-on, universal,
   no-overwrite, freshness-aged. Field-verify visibility across chambers.
7. **Physical-envelope traffic control (§4)** — gap-between-envelopes following
   and stopping, single follower behind single lead, conservative on degradation.
8. **Bubble routine (§11)** — two trains, the first mission on the turntable.

Each step is spec → Sam/CODEX review → Claude Code → field test → committed
verdict, per standing practice.

---

## §13 Matters intentionally unresolved (do not infer from CTO2 constants)

Pairing/session protocol for the initial two-loco routine; exact ESP-NOW packet
fields, versioning, freshness periods; ordering/reordering rules; numerical
braking margins beyond the 18"/48" envelope; per-class navigation/peer
degradation behavior; precise station platform + HOLD coordinates; whether route
reservations begin onboard or with future dispatcher help; the detailed IR/QUORUM
fusion contract; the mission-description format. Each requires its own spec and
field evidence.

---

## §14 Failure lessons that must survive CTO2

- Both CTO2 locomotives stayed `CTO_ROLE_SOLO`, peer geometry was unavailable,
  traffic stayed `TRAFFIC_CLEAR`, the stopped-lead approach never armed, and the
  follower cruised into a near-miss.
- The proposed causes (pairing predicates, Manual/AUTO mixing, freshness/sequence
  filtering, session-direction setup) were **debugging hypotheses for a
  superseded paradigm, not established root causes.** Their fixes are not CTO3
  requirements.
- The **durable requirement:** a failure to recognize or qualify traffic must
  never be converted into clearance. This is the one law §4's conservative
  defaults exist to enforce.
