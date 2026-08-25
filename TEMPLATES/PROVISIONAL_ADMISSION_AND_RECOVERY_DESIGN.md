# Provisional TEMPLATES admission and recovery design

**Status:** offline design evidence only; not approved for firmware or field use.

## Result first

The architecture is defensible, but the first numerical candidate is **not
acceptable for firmware**. It preserves every broad agreeing Toby passage and
structurally prevents rejected artifacts from contaminating recovery. However,
an event-level duration/peak gate still gives 14 QUORUM timing-phantom proxies
coordinate standing in Otto's restarted CW session. Those labels are not
physical ground truth, but a candidate that cannot clear this conservative
proxy has not earned implementation.

The safe conclusion is to keep the layer ordering below, retain the
precision-first doctrine, and replace the event-summary classifier with a
full-waveform passage classifier before another replay.

## Required pipeline

1. **Acquire one physical passage.** Work from the 1 kHz sample stream. Keep a
   passage open across brief baseline returns and defensible opposite-polarity
   lobes. Compute width, signed and absolute area, peak, rise/decline structure,
   baseline quality, gaps, saturation, PWM, direction, and capture integrity.
2. **Classify artifact versus credible passage.** A narrow, discontinuous,
   near-threshold, incomplete, or otherwise non-magnet-like response is logged
   and discarded. It receives no position, observation, or recovery standing.
3. **Enforce physical arrival.** A distinct second passage cannot arrive before
   distance divided by a measured maximum physical speed (with an approved
   margin). Activity before that bound is merged as passage structure or
   discarded. The replay's fixed 280/350 ms placeholders are not approved;
   route distances and maximum-speed evidence must replace them.
4. **Validate the expected target before mutation.** Compare direction, route
   order, expected polarity, and available timing/template evidence. Only an
   expected credible passage advances primary position exactly once.
5. **Observe credible contradictions separately.** A broad, timely
   contradiction leaves primary position unchanged. It may start an
   observation-only set containing bounded forward omission hypotheses. The
   set never contains discarded artifacts.
6. **Adopt only a uniquely explained omission.** The provisional replay tests
   offsets representing zero through four omitted markers. Adoption requires
   one hypothesis to explain every credible observation and every rival to
   fail at least once, after at least six observations. Twelve unresolved
   observations cause a safe stop. These numbers are provisional.
7. **Monitor liveness independently.** While tractive movement is positively
   evidenced, elapsed time/distance without a credible landmark reduces
   confidence and ultimately stops safely. It must not advance position or
   manufacture a marker from PWM. Stops, manual motion, reversals, ramps,
   E-stop, capture gaps, and unknown direction suspend or reset the appropriate
   timer under a separately reviewed state table.

## State ownership

- `primary_position`: changed only by expected admission or an approved unique
  omission recovery.
- `current_passage`: raw/sample-derived evidence for one physical field.
- `contradiction_observations`: credible passages only; never artifacts.
- `recovery_hypotheses`: private candidate positions and scores; no motor or
  station authority until adoption.
- `physical_anchor`: operator-grounded and immutable by navigator inference.
- `liveness`: movement-without-landmark timer; stop authority only.

Direction reversal closes the current passage, clears timing history and
recovery observations, preserves the last confirmed primary position, and
requires a fresh first-target confirmation. A capture/session gap invalidates
timing and prevents automatic recovery adoption across the gap.

## Mandatory invariants

- Artifact rejection is a terminal classification for navigation.
- No artifact can advance position, enter a hypothesis, change a score, or be
  resurrected.
- Map polarity is checked before primary mutation.
- One physical passage can cause at most one advance.
- Recovery can move only forward along the declared route and only within the
  approved omission bound.
- PWM provides context and liveness evidence, never location.
- Unknown or contradictory evidence ends in continued observation or a safe
  stop, not a guessed coordinate.

## What the current captures cannot establish

The captures have internal QUORUM decisions and a few operator annotations,
but not a physical marker-by-marker truth track. Therefore absolute false
insertion, position drift, and incorrect adoption cannot be certified from
them. QUORUM `AGREE` and timing-phantom decisions are explicitly reported as
retrospective proxies, not truth. The next evidence collection needs
independent physical anchors, reversal/station annotations, and complete
capture-gap accounting.

## Comparison with the independent working concept

After this design was produced, the untracked
`docs/TEMPLATES_NAVIGATION_CONCEPT.md` was reviewed. Both designs agree that
only the physically expected target may advance position, wrong-polarity
responses do not advance, uncertainty must remain honest, and QUORUM carries
no automatic authority. This proposal is more conservative about the concept's
suggestion that later accounting can identify and discard an already included
extra response: the doctrine requires preventing that inclusion at the
navigation boundary, because retrospective repair may already have triggered
station or traffic behavior.

## Recommendation

Do not modify or flash locomotive firmware. Build the next replay classifier
from the recorded sample waveform, add independent-anchor manifests, then
repeat full-capture and synthetic testing. The event-summary prototype is a
useful safety skeleton and a failed numerical baseline, not an implementation
candidate.

