# NAVI_RVM Decision 23 — Recognition Threshold Calibration

**Date:** 2026-10-17  
**Status:** Operator-selected research approach; not yet validated or implemented  
**Scope:** NAVI Rear View Window mathematical specification M1 → M2

## Decision

**Choose Approach A first: empirical calibration from existing successful recordings.**

Use independently validated, successful laps to establish the statistical characteristics of convincing MM–BL–MM puzzle-piece recognition. Evaluate the resulting criterion on held-out successful runs and actual or clearly labeled injected anomalies. Measure premature tile advancements, missed advancements, and recognition delay.

Do **not** begin by imposing an arbitrary target false-advancement rate.

## Alternative retained

**Approach B:** Specify an acceptable false-advancement rate (for example, per 100 laps), then calibrate a change-detection threshold to achieve it.

**Why deferred:** The operator prefers to learn what the actual railway recordings support before choosing a numerical reliability target. Revisit B if A produces unacceptable premature advancements or other operating errors.

## Implementation boundary

This decision chooses a **calibration strategy**, not a detector algorithm, threshold, guaranteed reliability level, or permission to change firmware. Do not train and evaluate on the same recordings. Preserve the governing one-expected-piece puzzle model, one count per new MM, and persistent geographic history.

## Continuity note

This file explicitly records the operator's request to **remember** this choice in the repository, rather than relying solely on chat history. Link it from the next M2 specification and decision ledger.
