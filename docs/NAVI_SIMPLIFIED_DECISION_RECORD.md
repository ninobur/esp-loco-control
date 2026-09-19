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
