# 0084 — Test a 90 ms completed-passage floor

**Date:** 2026-09-12
**Status:** **Superseded by 0085, 2026-09-12**
**Decided by:** operator — “I think your 60 threshold is too conservative. Try 90.”

**Pre-flash result:** **Blocked by gate 8; never flash this value.** The audit found the
suite had been exercising 40 ms rather than the flashed 90 ms. After the value
was shared with every acquisition gate, the sustained-offset case failed.

## Decision

Build an explicitly non-field-accepted NAVI_ONE image with a **90 ms**
completed-passage duration floor. Keep the unconditional 500 ms close-to-open
guard unchanged. Every sub-floor close produces a one-shot background
acquisition record and never reaches the magnet recognizer or Navigator.

This authorizes a field trial, not adoption of 90 ms as the production value.

## Context

Otto stopped after Bamboo when a 47 ms, peak-86 phantom opened 558 ms after
MM151. It cleared the 500 ms timing guard and was admitted at cruise PWM 90,
six genuine markers and approximately twelve seconds after the station's
departure command. It was not an immediate restart event.

Nine of eleven earlier Otto phantoms occupied 40–58 ms. The earlier 60 ms
proposal would exclude that measured band, but the operator chose 90 ms for
the trial. The available corpus contains one known genuine Otto passage at
82 ms; the next shortest is 128 ms. Toby's accepted primary minimum is 131 ms
at PWM 91.

## Alternatives considered

- **60 ms:** excludes the measured 40–58 ms band without rejecting a known
  genuine passage; judged too conservative for this trial.
- **Raise Otto's entry amplitude:** not combined with this experiment because
  the earlier entry-90 trial missed genuine magnets.
- **Change smoothing or the 500 ms guard:** unrelated to this duration test and
  left unchanged.

## Consequences

- The 47 ms Bamboo phantom is rejected before navigation.
- The known 82 ms genuine Otto passage would also be rejected. That is the
  principal deliberate risk and makes field observation essential.
- The setting is shared by Otto and Toby in this sketch. Toby has 41 ms of
  measured clearance above it among accepted primary durations. One truncated
  Toby survey record no longer closes when its retained buffer is replayed at
  90 ms; it is excluded from polarity scoring because the retained record is
  not a faithful complete passage.
- In the sustained −50-count, opposite-polarity, at-rest offset simulation,
  raising the floor from 40 to 90 ms changes genuine markers seen from 20/20 to
  0/20: the brief close that separates the stalled offset from each magnet is
  discarded. This is a blocking safety regression unless the operator elects
  to measure 90 ms in shadow rather than give it admission authority.
- A follow-up sweep located the cliff precisely: floors 40–87 ms retain 20/20
  marker visibility in that case; floors 88–90 ms retain 0/20. Independently,
  82 ms is the highest floor that preserves Otto's shortest known genuine
  82 ms passage because the code rejects durations strictly below the floor.
- This does not solve multi-second departure plateaus, the post-stop exception,
  or every phantom lasting 90 ms or longer.
- The field run must examine whether rejected events cluster within ten marker
  intervals after station departures, as the operator observed.

## References

- `docs/NAVI_BAMBOO_TRANSIENT_ANALYSIS_20260912.md`
- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/fixtures_bamboo_20260912.h`
- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/gate_bamboo_transient.cpp`
- Decision 0081 — unconditional 500 ms guard
