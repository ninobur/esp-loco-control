# QUORUM 1.14 implementation report — LAYER 5, CTO3 peer coordination

**Status: built and verified on the desk. NOT flashed, NOT field-tested,
NOT approved for deployment.** Two-locomotive behaviour has never executed
anywhere — the harness cannot exercise it (single instance). Operator order
2026-08-13: "write the sketch."

**Spec:** `docs/CTO3/BUBBLE_V1_SPEC.md`. **Contracts:** decision 0034
(Proposed). **Source:** `firmware/QUORUM/QUORUM.ino`, `SKETCH_NAME
"QUORUM_1_14"`.

## What was built

One new layer, LAYER 5, ~450 lines, plus eleven precise touch points:

| piece | where | what |
|---|---|---|
| Peer truth TX | `ctoTxStatus()` | frozen `CtoPeerPacket` v3 at 2 Hz, always — every mode, moving or not. Producer-applied occupancy bounds in markers (hall ∘ extent, decisions 0030/0033). Unfillable fields zeroed, never guessed |
| Registry | `ctoPeers[8]` | keyed by loco id, LRU eviction, 3 s freshness gating every logic path |
| Roles | `ctoEvaluateRoles()` | Q1/Q2 at ≤ 12 MM bound gap: peer ahead → I follow; behind → I lead. Latched (0032), dissolved on direction change / operator / CTO off — never silently on staleness |
| Role echo | `Cto3RoleEcho` 0xC5 | 1 Hz self-truth; leader-leader or follower-follower disagreement ⇒ stop + `CTO_ROLE_CONFLICT`, clears when the echo does |
| Formation | `ctoProvisionalLeader()` | long-range: smaller behind-arc leads, ±12 MM tie band, lower ID leads; drives only who waits at a station |
| Traffic protection | `ctoLimitPwm()` | the one deceleration profile as a cap: gap ≤ 18 → zone speed; ≤ 12 → 0; ≤ 6 or bounds crossed → 0 via contact guard. Moving leader: shadow its PWM. Applied at all five station-machine request sites |
| Station choreography | `ctoHoldDeparture()` / `ctoDwellMs()` | leader completes its own dwell then holds; follower arrival (stopped, ≤ 6+4 MM behind, positive evidence) starts the 10 s release dwell; paired follower dwells 20 s at platforms |
| Fleet stop | `ctoServiceFleetStop()` | 0031 by absence: expected peer stale or position-less ⇒ stop + alert; clears on its return, no autonomous surge |
| Telemetry | `state/cto` | transitions + 5 s heartbeat: role, partner, gaps, bounds, holds, radio counters |
| Command | `cmd/cto` | `clear` / `off` / `on`; unknown payloads refused loudly |
| Radio plumbing | `ctoOnRecv` → `ctoRxQueue(8)` → `ctoService()` | recv callback only copies; all state lives on the loop thread, the `onMqttEnqueue` pattern |

## Constitutional position (§0.2)

LAYER 5 **never writes the motor**. It caps what the AUTO station machine may
request, extends a dwell, and issues `requestPwm(0)` only under
`autoRunning` for fleet stop and role conflict — the same chamber-gated shape
as `enterNoQuorum()`. In MANUAL it broadcasts self-truth and touches nothing.
E-stop and manual authority are unmodified.

## Verification performed

1. **Compiles clean for both locomotives** — `esp32:esp32:esp32`,
   `--warnings all`, no new warnings. 995,627 bytes (75%), globals 53,108
   (16%): +12.5 KB flash, +640 B RAM over 1.13.
2. **The full replay suite passes unmodified** — all fixtures, both stateful
   orderings, the incident-C counterfactual. The harness's new
   `tests/shim/esp_now.h` is inert (init succeeds, sends dropped, no
   callback), so the suite doubles as the **solo-inertness proof**: with no
   peer ever heard, 1.14's navigator makes byte-for-byte the decisions 1.13
   made on every fixture.
3. **Arduino prototype-hoisting trap** hit and fixed: the layer's types are
   defined with the other enums near the top; functions stay before
   `setup()`.

## What is NOT verified, deliberately listed

- **Any two-locomotive behaviour.** Pairing, echo conflict, traffic ladder,
  hold/release choreography, fleet stop — none has ever run. The harness is
  single-instance.
- The provisional 18/12/6 ladder against real stopping distances (spec:
  worst-case trials owed).
- ESP-NOW + WiFi STA coexistence on this AP/channel under MQTT load.
- Extent truthfulness (M5 gate, decision 0033 amendment).
- `state/cto` rendering on the console — the dashboard knows nothing of the
  topic yet.

## Review round 1 (CODEX, 2026-08-13) — six findings, all applied

The first build carried five deployment blockers. Dispositions:

1. **CTO stops overwritten during station approaches** — the worst one:
   `ST_APPROACH`/`ST_FINAL` re-request speed every pass and bulldozed a CTO
   zero within one loop. Fixed structurally: **the limiter moved inside
   `requestPwm()`/`requestPwmOver()`** — the only two writers of
   `commandedPwm` — applied under `autoRunning` only. The five call-site
   wrappers are gone; no AUTO request path can bypass the cap.
2. **Stopping intent detected too late** — `ctoPeerStopping()` now fires from
   `ST_APPROACH` onward and on a falling ramp (2-count hysteresis on
   consecutive broadcasts), so the follower pre-slows through the leader's
   whole approach.
3. **Contact guard skipped after bounds cross** — the symmetric hall-arc
   proximity check now runs *before* the ahead/behind topology rejection that
   was discarding an overlapped peer exactly when contact was imminent.
4. **Simultaneous reversal preserved stale roles** — direction is stored at
   latch (`ctoPairDir`); *any* local direction change dissolves the pair, so
   one train reversing and both trains reversing dissolve alike.
5. **Echo did not require agreement** — three-valued now (0034 revised):
   CONFIRMED (fresh, opposite role, names me) is the only state that runs
   release choreography or the 20 s follower dwell; CONFLICT stops both;
   UNCONFIRMED leaves traffic protection active and the leader holding.
   A mixed-version bubble degrades to supervised running, never to
   unconfirmed automation.
6. **`cmd/cto` accepted prefixes** — exact match after trim; `clear-anything`
   no longer disarms fleet protection.

Reverified after the fixes: both profiles compile clean under
`--warnings all` (995,815 bytes, +188 over round 0); the full replay suite
passes unmodified — the solo-inertness proof holds with the limiter inside
the request functions, because with no peer ever heard `ctoLimitPwm()` is
the identity.

## Review round 2 (CODEX, 2026-08-13) — seven findings; the limiter becomes continuous

Round 1's fix was necessary and not sufficient, and the round-2 review said
so plainly: a cap applied only at request time cannot react to a peer EVENT
during steady cruise, because no request fires when `commandedPwm` already
equals the cruise target. The claim that the choke point covered "fleet stop
or role conflict" also disagreed with the code — the limiter never consulted
`ctoEchoConflict`. Dispositions:

1. **(Critical) Limiter not reactive mid-cruise** — `ctoDesiredPwm` now holds
   the last *uncapped* AUTO intent; the two writers cap at request time, and
   `ctoService()` re-derives `cap(desired)` **every pass**: a new stop
   condition bites within one loop, a lifted cap restores the desired speed
   (the spec's traffic resume). AUTO chamber only; E-stop/NEUTRAL enforced
   downstream as before.
2. **(Critical) Role conflict bulldozable** — `ctoLimitPwm()` returns 0 on
   `ctoFleetHold || ctoEchoConflict`; with (1), both are now continuously
   enforced rather than one-shot requests.
3. **Ramp-duration corruption** — `ST_IDLE` compares against
   `ctoDesiredPwm`, not `commandedPwm`, so a standing cap no longer causes a
   re-request every pass recomputing `pwmStepMs` from a shrinking delta.
4. **Direction dissolution gated on usable nav** — the
   `navDir != ctoPairDir` check moved above the navigation-usable gate; a
   reversal during NAV_NO_QUORUM (or to UNSET) now dissolves in every state.
5. **Old echo confirms new pairing** — echoes must **postdate the latch
   epoch**; a stale/absent echo yields clean UNCONFIRMED and clears an
   existing conflict through the normal transition (the silent partner is
   0031's jurisdiction, not a latched conflict's). 0034's freshness row
   updated.
6. **Prefix matching, second attempt** — the token is now also required to be
   followed by nothing but whitespace; `clear anything` is refused with
   `TRAILING_CONTENT`.
7. **rampFalling defects** — slot identity change resets the whole entry
   (no cross-occupant comparison; also purges the previous occupant's echo),
   the threshold is a true ≥2-count fall, and detection persists while the
   ramp is non-increasing.

Reverified: both profiles compile clean under `--warnings all`
(996,079 bytes); full replay suite green. The suite still cannot exercise any
of this — its ESP-NOW path is inert by design — so these fixes are verified
by review and compilation only, and the two-locomotive session remains the
first executable test of the layer.

## Deployment gate

Per the spec and standing practice: **operator + CODEX review of this report
and decision 0034, then a two-locomotive supervised session with hands on
both E-stops** — formation first, then station choreography, then an induced
fleet-stop (power a locomotive off mid-run). Do not flash before review; do
not run unattended before M7's crossing test.

## Known compromises, stated

- Navigation-uncertainty term absent from published bounds (extent only) —
  owed at M5, recorded in the spec.
- One expected peer (`ctoExpectedId`), not a roster — two-train spec;
  multi-train needs 0034 revisited.
- `stationPhase` on the wire carries OUR enum values; peers run this same
  firmware. An r12-era reader would misread phases — none exists on the
  railway.
- CE severance / express-local missions are **not in 1.14** — the spec's §9
  needs the mission filter wired to `cmd/cto`, a small later increment.
- The three local `{buf,len}` queue-item structs are layout-identical by
  construction; a shared typedef would be cleaner — cosmetic, noted for
  review.

## References

- `docs/CTO3/BUBBLE_V1_SPEC.md`; decisions 0030–0034
- `docs/CTO2_AUDIT_DISPOSITION.md` — what was ported, reviewed, rewritten
- `firmware/QUORUM/tests/` — suite run 2026-08-13, all green on 1.14
