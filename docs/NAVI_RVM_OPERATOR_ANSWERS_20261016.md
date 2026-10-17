# NAVI_RVM — Operator answers to the six research questions

2026-10-16 · Recorded by Claude (Opus 5.5) from David's message of the same day

> **Status.** This page records David's decisions verbatim in substance. They
> answer §14 (Q-A to Q-E, plus the PWM question) of
> [`NAVI_RVM_MAGNETIC_LANDSCAPE_RECOGNITION_RESEARCH_20261016.md`](NAVI_RVM_MAGNETIC_LANDSCAPE_RECOGNITION_RESEARCH_20261016.md).
> Their stated scope:
>
> - They govern the **next research stage**.
> - They **do not authorize firmware implementation** or changes to
>   operational NAVI.
> - They do not authorize changes to geographic maps, station-control
>   instructions or operational services.
>
> The constrained-DTW architecture is **not validated** until the replay
> experiments demonstrate its performance.

## 1. Local search width

**Yes to local alignment. No to an arbitrary ±150 mm boundary.**

- NAVI maintains one trusted geographic history and knows the next expected
  MM.
- The recognizer may examine a local region of the landscape. Local waveform
  alignment must **not silently change the trusted geographic identity**.
- The **spatial extent of each magnetic ensemble is unknown**. Ensemble
  boundaries are to be discovered by observation, not defined from magnet
  spacing.
- The alignment search width and the physical width of an ensemble are
  **separate questions**. Determine suitable limits experimentally.

## 2. Station stops

**Neither provisional nor retrospective. Preserve the existing tile-based
station-control system.**

- Station stops are prescribed by geographic tile instructions and PWM
  profiles. NAVI knows which tile it is traversing and what instructions come
  next.
- Relinquishing continuous speed control in favor of prescribed operating
  instructions is what makes retrospective geographic confirmation possible.
  Station control does not wait for completed ensembles.
- Preserve all existing station MM targets, stopping profiles and operating
  behavior.

## 3. Magnet obscured by a recording gap

**No STOP.** Missing evidence does not defeat an otherwise coherent geographic
pattern.

- A gap is not proof of a missed magnet.
- NAVI retains its interpretation and expects later observations to confirm
  progression.
- The missing-data record is preserved.
- An isolated inconsistency triggers watchful waiting.
- A *demonstrated* failure of geographic continuity may require STOP.
- Immediate safety protections remain independent.
- **Do not manufacture missing Hall observations to satisfy the map.**

## 4. IR shortfalls in the map

**Absolutely not.** The geographic map describes the physical railroad, not
an instrument's errors.

- IR is supportive evidence and carries **no veto** over a coherent magnetic
  pattern.
- IR discrepancies may be kept separately for sensor-performance analysis.
- A repeated IR shortfall should make NAVI question the IR, not its identity.

## 5. "Atypical"

**Determine normal variation empirically** from repeated successful
traversals, across speeds, directions and operating conditions. Do not impose
an arbitrary threshold.

- An atypical observation is not automatically erroneous. It prompts
  retrospective examination and continued observation.
- The principal question: *does the observed magnetic landscape continue to
  match the expected landscape?*

## 6. PWM as a movement limit

**PWM is mandatory contextual evidence, not an absolute geographic
authority.** PWM, elapsed time, historical performance and geographic
operating conditions estimate speed and travel. PWM has been substantially
underutilized.

- No guaranteed minimum distance: Otto may stall.
- A historical maximum must allow for downhill acceleration and other
  variation.
- Estimates should reflect Otto's actual history at comparable PWM and
  location.
- **Neither PWM nor IR independently vetoes a coherent magnetic landscape.**
- The normal expectation is that Otto progresses according to PWM. Stalls,
  derailments, obstructions, dislodged sensors, disconnected wiring and
  operator relocation deserve protection, but should not define ordinary
  navigation.

> This supersedes the report's §4 and §8.4 framing. In those sections, the
> PWM upper bound was a hard constraint and PWM was a "leg" of demonstration.

## Governing clarification

- NAVI operates in a deterministic geographic environment. After acquisition
  there is one expected next magnet.
- Geographic confidence comes from the **accumulated history** of successful
  encounters, not from validating the latest observation in isolation.
- The complete landscape is the primary source of continuity. It is
  recognized retrospectively from the continuous waveform, its broader
  structure, historical encounters and movement context.
- An inconsistency triggers scrutiny, not rejection. Preserve the evidence,
  keep observing, and let later observations resolve or corroborate it.
- **Avoid feedback loops** in which one questionable measurement corrupts
  later interpretation.
- Trackside geographic anchors are a separate, unproven future capability.
  NAVI_RVM must work without them.

## Validation requirements before any implementation recommendation

The model must be shown to:

1. recognize continuity across repeated landscapes despite speed variation;
2. use the continuous Hall pattern without depending on the existing
   detector's opening decisions;
3. preserve the value of multi-lobe ensembles;
4. incorporate PWM history as mandatory movement context;
5. tolerate inconsistent IR without losing continuity;
6. distinguish ordinary discrepancies from genuine physical disruptions;
7. maintain one established history rather than searching alternatives;
8. operate without trackside ESP sensors.
