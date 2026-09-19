# NAVI_SIMPLIFIED Decision Record

This document is the durable record of decisions made during the decision-by-decision review of NAVI_SIMPLIFIED.

## Scope

Every NS-Dxxx decision in this document applies specifically to NAVI_SIMPLIFIED. A decision recorded here does not automatically modify or govern NAVI_ONE, X18–X21, QUORUM, or any other experimental or production build.

## Status and field verification

Unless explicitly stated otherwise, an approved NS-Dxxx decision has the status:

PROVISIONALLY APPROVED — SUBJECT TO FIELD VERIFICATION

Approval means that the decision is approved for NAVI_SIMPLIFIED implementation and testing. It does not mean that the behavior has been proven correct on the railway.

Field evidence may cause a decision to be amended or superseded. An existing approved decision must not be silently rewritten to accommodate later evidence. Preserve the original decision and explicitly identify the later decision that amends or supersedes it.

## Record discipline

Only decisions explicitly approved by David are entered as approved decisions.

Do not infer decisions from discussion.

Do not convert proposals, hypotheses, implementation suggestions, or unresolved questions into decisions.

Each decision receives a permanent sequential identifier in the form `NS-Dxxx`.

---

## NAVI_SIMPLIFIED NS-D001 — Magnet Detection

Status: PROVISIONALLY APPROVED — SUBJECT TO FIELD VERIFICATION

Date: 2026-09-18

For NAVI_SIMPLIFIED, a magnet detection is a complete, instantaneous event. It occurs on the second of two consecutive 1-kHz Hall samples whose absolute departure from baseline is ≥70 counts. The event timestamp is the time of that second sample, and the polarity of the Hall departure is recorded with the event. Subsequent Hall readings are not part of completing, closing, or validating that detection.

Boundary: This defines NAVI_SIMPLIFIED detection only. It does not determine readiness for another detection, physical same-magnet protection, Hard Protection, or NAV acceptance. Later Hall waveform information may be retained diagnostically but cannot revoke the detection.

Terminology: A magnet detection does not “open” anything and has nothing to “close.”

Verification: This decision remains provisional until verified by NAVI_SIMPLIFIED field testing. Contrary field evidence requires explicit amendment or supersession rather than silent alteration.

---

## NAVI_SIMPLIFIED NS-D002 — Sensor Reporting and NAVI Authority

Status: PROVISIONALLY APPROVED — SUBJECT TO FIELD VERIFICATION

Date: 2026-09-18

For NAVI_SIMPLIFIED, the Hall detector reports qualifying magnet-detection events to NAVI with their associated sensor information. The detector does not decide whether a detection represents the next mapped magnet. NAVI receives the available evidence and determines the event’s navigation significance. Sensor detection and navigation validation are separate functions.

---

## NAVI_SIMPLIFIED NS-D003 — Sensor Report Content

Status: PROVISIONALLY APPROVED — SUBJECT TO FIELD VERIFICATION

Date: 2026-09-18

For NAVI_SIMPLIFIED, a magnet-detection report contains facts produced by the Hall detection itself: event identity, detection timestamp, Hall departure value, baseline, and polarity. Navigation state and motion context are maintained by NAVI and are not duplicated into the sensor report merely to support NAVI judgment.

---

## NAVI_SIMPLIFIED NS-D004 — Distinct Sensor Event

Status: PROVISIONALLY APPROVED — SUBJECT TO FIELD VERIFICATION

Date: 2026-09-18

For NAVI_SIMPLIFIED, after reporting a magnet detection, the Hall detector must observe the signature of a distinct new magnet encounter before reporting another detection. This is sensor-event separation only. It does not determine whether the locomotive has physically reached another mapped magnet and does not provide navigation authority.

---

## NAVI_SIMPLIFIED NS-D005 — Baseline Establishment: Governing Technical Requirements

Status: PROVISIONALLY APPROVED — SUBJECT TO FIELD VERIFICATION

The baseline measurement must represent ordinary track and therefore must be centered within the region between the magnetic fields of adjacent MM magnets and sample an adequate section of track.

Baseline acquisition is spatial, while the Hall sensor measures that space as a time series. Locomotive speed determines the relationship between distance and sampling time. Speed therefore affects the time from the preceding magnet detection to the usable baseline region, the duration of that region, the track distance represented by a sampling interval, and the time at which the next magnet’s field is approached.

Fixed elapsed times therefore cannot be assumed to identify or sample the same physical section of track at different locomotive speeds.

NAVI_SIMPLIFIED baseline acquisition must account for these relationships when determining the sampling start point, sampling end point, sample duration, and required track coverage.

Quantitative requirements shall be specified in NS-D005A and subsequent subsections.
