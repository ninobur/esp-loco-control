# CTO3 Manual/AUTO Authority Model

Status: current explanatory guide; restates accepted decision 0013 and does not
create a new architecture decision
Recorded: 2026-08-07

## The distinction Claude Code must preserve

Manual, AUTO, navigation, and traffic visibility are not four names for one
state. They answer different questions:

- **Propulsion authority:** who may command motor PWM right now?
- **Navigation:** what does the locomotive believe about its position and motion?
- **Traffic visibility:** what self-truth does the locomotive publish to peers?
- **Mission state:** what automatic routine is armed or executing?

Changing propulsion authority must not erase navigation or make a powered
locomotive disappear from traffic awareness.

## The two propulsion chambers

### MANUAL

The human operator owns propulsion. Manual throttle and direction are direct
and sovereign. Navigation may observe, count markers, estimate speed, and
publish position, but navigation, station, dispatcher, and CTO3 code may not
write PWM in Manual. Manual remains usable when navigation is unset or degraded.

### AUTO

Onboard software may own propulsion only after two separate conditions:

1. **Enrolled:** the locomotive accepts `cmd/auto`; this crosses from Manual
   into the AUTO chamber but does not itself launch motion.
2. **Running:** a dispatcher GO is accepted after its existing safety and setup
   gates; only then may automatic movement and station code command PWM.

Enrollment and GO are deliberately separate. A station mission being present
in firmware does not authorize it to move a Manual locomotive.

## Exactly four chamber doors

1. **E-STOP:** acts in either chamber and overrides everything.
2. **Enrollment (`cmd/auto`):** Manual to AUTO; the locomotive's explicit act.
3. **Release/END:** AUTO to Manual; clears enrollment and running authority.
4. **Dispatcher STOP:** stops an enrolled AUTO locomotive but never zeros a
   Manual locomotive's throttle.

There are no implicit crossings. Navigation loss, station detection, stale
telemetry, or a mission request cannot seize propulsion authority from Manual.

## Visibility is independent of authority

Every powered locomotive continues to publish its best self-truth in Manual,
AUTO, stopped, moving, dispatcher-stopped, or E-stopped states. Manual means
human propulsion authority; it does not mean absent, untracked, or invisible.
Unknown or stale peer information is never converted into clear track.

## Station Stop v1 consequence

The Arches routine may arm and command PWM only while AUTO is both enrolled and
running. Its expected authority path is:

```text
MANUAL --cmd/auto--> AUTO enrolled --dispatcher GO--> AUTO running
    --> Arches mission owns station PWM through departure/reset
    --dispatcher STOP/RELEASE--> MANUAL
```

The current operator finding is that the live dashboard cannot place a
locomotive into AUTO. That is a dashboard/command-surface blocker for field
validation. It is not permission to bypass enrollment in firmware, and a
retained `state/auto = 1` value alone does not prove that a dashboard enrollment
action succeeded.

## Source authority

When sources disagree, use this order:

1. David's current operator ruling.
2. Decision 0013 and QUORUM/CTO3 constitutional authority sections.
3. `CTO3_INTENT_BASELINE.md` and the current CTO3 specification.
4. Contemporary verified field evidence.
5. Historical proposals and prototype code in `resources/`.

The resource folder intentionally contains older three-mode and four-mode
proposals, block-era logic, Blynk-era interfaces, dispatcher-centered systems,
and prototype Flask code. They are evidence of design evolution, not current
requirements and not implementation-ready source.

## Required fidelity check before Station Stop v1 work

Before editing QUORUM, an implementer must explain back:

- who owns PWM in Manual;
- the difference between AUTO enrollment and AUTO running;
- the four and only four chamber doors;
- why navigation and self-truth continue in both chambers;
- why the dashboard enrollment failure must not be solved by weakening the
  firmware authority boundary.

If any answer differs from this guide or decision 0013, stop before editing.

## References

- `docs/decisions/0013-bicameral-four-door-authority.md`
- `docs/CTO3/CTO3_INTENT_BASELINE.md`, Bicameral locomotive authority
- `docs/CTO3/CTO3_SPEC.md`, §2
- `docs/CTO3/resources/MANUAL_VS_AUTO.txt`
- `docs/CTO3/station-stop-v1/README.md`
