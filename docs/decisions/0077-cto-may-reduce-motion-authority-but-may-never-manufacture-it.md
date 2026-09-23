# 0077 — CTO may reduce motion authority; it may never manufacture it

**Date:** 2026-09-10
**Status:** PROPOSED. Not authoritative until the operator reviews and approves
it. Drafting it allocates it no authority whatsoever, and nothing in it
authorizes an agent to act. See the README's rule that matters most.
**Follows:** 0030/0033 (consist extents and the six-marker invariant), 0039 (a
third node may relay peer truth under freshness and novelty gates), 0076
(navigation recovers from one wrong observation).
**Evidence:** `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §1, §5,
§6, §9. Donor behaviour read from `firmware/programs/QUORUM/QUORUM.ino:2860-2880`
(`ctoDesiredPwm` continuous cap) and `NAVI_ONE.ino:453-458`
(`requestPwm`/`serviceRamp`).
**Builds:** none. This record sets direction. Stage 1 (Otto alone on NAVI_ONE,
no CTO, no source change) does not exercise it.

---

## Decision

The architectural statement the operator required, verbatim:

> NAVI is the sole navigation authority. CTO consumes NAVI position and
> occupied-track truth but cannot create, correct, score, or replace it. CTO is
> distributed between locomotives over ESP-NOW. The Dispatcher assigns and
> observes service through MQTT but is not the real-time traffic controller.
> CTO may reduce motion authority; it may never manufacture movement authority.

Three consequences follow and are part of the decision, not commentary:

1. **CTO is a cap, not a throttle.** It may only lower the PWM NAVI already
   intends. It has no path that raises one.
2. **The cap is enforced continuously, not on events.** `requestPwm()` fires
   only at station orders and section boundaries, so a cap applied there is
   inert through minutes of cruise. The cap is therefore evaluated every tick
   in `serviceRamp()` against a separate, uncapped `naviIntentPwm`.
3. **Loss of any channel grants nothing.** Losing Wi-Fi, MQTT, the Dispatcher
   or a peer leaves local and peer safety exactly as conservative as before.

## Context

QUORUM earned the fleet's traffic behaviour but also its recognition, scoring,
offset, quarantine and evidence-ring machinery. The operator's direction is that
NAVI is the foundation and CTO is added to it — never the reverse, and never by
copying QUORUM as a monolith. That makes the authority boundary the load-bearing
statement of the whole design, because it is the one thing that stops the donor's
navigation logic re-entering through the traffic layer.

## Alternatives considered

- **Let CTO command throttle directly**, as the donor's leader/follower logic
  does in places. Rejected: it gives a traffic layer the ability to produce
  movement, which is exactly the authority this record withholds.
- **Apply the cap inside `requestPwm()`.** Rejected on inspection, not taste:
  it is event-driven, so a train already at cruise would hold cruise through a
  hold condition until the next station order.
- **Let the Dispatcher arbitrate separation centrally.** Rejected: it makes
  collision protection depend on a network path, and a dashboard that looks
  clear is not evidence that track is clear.

## Consequences, including the unwelcome ones

- A cap evaluated every tick is a new hot path in `serviceRamp()`, which already
  carries the e-stop and low-voltage clamps. The low-voltage clamp **mutates**
  `rampTarget` to 0; the CTO cap must not copy that pattern, or NAVI's intent is
  destroyed rather than limited and the train will not resume when the
  constraint lifts.
- "Never manufacture authority" means CTO cannot release a hold on optimism.
  A locomotive stopped by a stale peer stays stopped until truth arrives or the
  operator intervenes. That is a deliberate availability cost, and it will look
  like an unnecessary shutdown on the day it happens.
- The rule is easy to state and easy to erode. Any future field that lets CTO
  raise a PWM, extend a permission, or shorten a hold is a change to this
  record, not an implementation detail.

## References

- `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §1, §5, §6, §9
- `firmware/programs/QUORUM/QUORUM.ino:2860-2880`
- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/NAVI_ONE.ino:447-458`

## Review

Unreviewed. Operator has not ruled.
