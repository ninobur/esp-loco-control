# Target-Acquisition Decision Logic for TEMPLATES

## Status and required reading

- **Must read before building `TEMPLATES`, the successor to `QUORUM`.**
- `MAGNET_ADMISSION_DOCTRINE.md` is a co-required design constraint. TEMPLATES Hall/navigation code must satisfy both documents; this record must not be read in isolation.
- This document records the operator's intended decision logic for review.
- It is investigatory. It does not approve an implementation, numerical threshold, firmware flash, or locomotive operation.
- The Hall-processing review and an explicit operator approval remain mandatory before implementation.

## Evidence behind the direction

The 2026-08-24 waveform captures added an important physical distinction:

- passage-like track-magnet responses are broad, smooth curves;
- the large majority of collected phantom responses are narrow spikes;
- narrow spikes continue while Otto is powered but stationary, so they cannot represent successive track magnets;
- width changes strongly with speed, so width alone is not a magnet identity;
- integrated absolute flux helps distinguish narrow spikes from broad responses, but no production threshold has been selected;
- the original centered moving baseline can absorb or fragment a broad response;
- a pre-event frozen baseline can retain a stale offset and merge separate responses.

The captures therefore support investigating a simple curve/flux acquisition gate, but they do not yet demonstrate accurate magnet detection or establish false-positive and missed-magnet rates.

## The missing QUORUM gate

`QUORUM` is a bounded multiple-hypothesis recovery system, but it presently advances `navMm` for every navigation-accepted event before comparing the event's polarity with the mapped polarity. A disagreement is recorded after the advance. In plain language, it can say "that was the wrong polarity" while still counting the response as the next magnet.

The operator's intended rule is stricter:

> Position changes only after the response has earned recognition as the one physically possible next mapped magnet.

If a positive magnet is expected and a negative response is observed, the primary position must not advance. The response is not the expected marker.

## Required separation of responsibilities

### 1. Acquisition: is this a plausible distinct magnet passage?

Use the richest defensible physical evidence available, including:

- waveform duration;
- smooth curve structure rather than a narrow spike;
- positive and negative peak relative to baseline;
- signed integrated flux;
- absolute integrated flux;
- baseline and baseline-quality information;
- sample completeness and acquisition gaps;
- actual and commanded PWM, direction, and measured time context.

Responses demonstrated physically impossible as distinct marker passages are discarded for navigation. Examples for evaluation include:

- a narrow spike rather than a passage curve;
- a response occurring too soon for the locomotive to reach the next mapped magnet;
- an opposite-polarity return response occurring impossibly soon after the previous accepted marker.

"Impossibly soon" must be derived from track spacing and measured or conservatively bounded physical speed. It must not be a guessed dead-time.

Discard means:

- do not advance position;
- do not insert the response into the navigation evidence ring;
- do not score it against alternate position hypotheses;
- do not permit later resurrection as a marker.

The raw waveform and a diagnostic rejection reason may still be recorded. Diagnostic visibility does not grant navigation standing.

### 2. Target acquisition: is this the expected mapped magnet?

Only a physically plausible passage response proceeds to map validation. It must then satisfy the available target evidence:

- the next marker is physically reachable;
- the observed polarity matches the next mapped marker's polarity;
- route sequence and direction are consistent;
- timing is physically possible.

If the expected next magnet is positive and the plausible response is negative, the navigator says "No Way":

- do not advance the primary position;
- do not treat the response as the expected marker;
- record the disagreement diagnostically;
- continue looking for the expected marker.

Repeated credible contradictions may reduce position confidence, cause a safe stop, or require an operator declaration. The recovery policy remains to be reviewed and is not approved here.

### 3. Hypothesis recovery is downstream

Multiple-hypothesis reasoning should not be asked to explain obvious non-marker artifacts. It receives only evidence that has passed the physical acquisition gate, and it must not override a categorical physical impossibility.

Awareness is not candidacy. `QUORUM` may count and publish rejected observations and their reasons without preserving them as possible magnets—just as a driver notices driveways and farm roads while looking for a turn, rejects them immediately, and does not retain them as candidate destinations.

## Intended decision sequence

```text
raw Hall response
    |
    +-- narrow spike, incomplete, or physically impossible distinct passage?
    |       yes -> discard for navigation; log reason; do not advance
    |
    +-- plausible broad curve with adequate physical evidence?
            no  -> do not advance; diagnostic/review result
            yes
             |
             +-- arrived impossibly soon?
             |       yes -> discard; do not advance
             |
             +-- polarity matches the next mapped marker?
                     yes -> accept the expected marker and advance once
                     no  -> "No Way"; do not advance; record disagreement
```

The exact ordering of morphology and timing calculations may change during implementation review, but no rejected response may affect position before earning target status.

## Validation requirements

Before this direction can be implemented in `TEMPLATES`:

1. Prototype the acquisition and map gates in host replay against the existing complete waveform captures.
2. Use `QUORUM`'s actual magnet map and explicit operator-provided start interval, direction, orientation, stops, stalls, assistance and anchors.
3. Keep operator-confirmed position separate from detector-predicted position.
4. Record every gate result and the predicted position before and after each response.
5. Test spike rejection, broad slow responses, same- and opposite-polarity returns, physically possible opposite-polarity next magnets, gaps, baseline failures and repeated disagreements.
6. Demonstrate that rejected responses never advance position or enter hypothesis scoring.
7. Use operator anchors as independent ground truth. Internal map consistency cannot validate the detector by itself.
8. Measure false acceptance and missed-magnet behavior before selecting thresholds.

Prefer the simplest demonstrated adequate method. Do not introduce CFAR, matched filtering, full waveform matching, guessed dead-time or expanded MHT unless simpler acquisition and map gates prove inadequate.

## Approval boundary

This note records a required design direction and review boundary:

- enrich the Hall evidence reaching navigation;
- reject physically impossible/non-marker responses before navigation;
- require expected-map polarity before the primary position advances;
- keep diagnostic awareness separate from navigation evidence;
- reserve recovery logic for credible ambiguity, not obvious spikes.

It does **not** approve numerical gates, a baseline method, event-boundary rules, a recovery response, a `QUORUM` modification, a `TEMPLATES` implementation, flashing, or field operation.

## Mandatory companion doctrine

`MAGNET_ADMISSION_DOCTRINE.md` records the operator's required error asymmetry: false inclusion corrupts the coordinate system, while conservative omission creates the bounded missing-marker error the recovery layer is intended to repair. Any implementation or review that cites this document must also address that doctrine's mandatory coding-review questions.
