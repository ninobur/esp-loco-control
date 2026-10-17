# NGR NAVI — Ground and Gauss Ensemble Architecture
**Design discussion record — October 16, 2026**

**Status:** Proposed architecture and agreed research direction; **not** an approved navigation implementation or change to installed EWO_0_6.  
**Provenance:** Consolidates the operator–ChatGPT design discussion following Find the Ground Phases 1–3 and the October 15 field run. The research numbers below are reported results supplied in that discussion, not independently revalidated by this document.  
**Scope:** Magnetic encounter recognition, movement context, baseline maintenance, station-reference research, and parallel field evaluation.

## 1. Why we changed the question

NGR has repeatedly attempted to derive exact geographic identity and position from individual Hall openings, polarity, precise IR distances, and comparison rules. That creates false dilemmas when a real magnet produces multiple threshold recrossings or polarity lobes.

The revised question is deliberately smaller:

> **Identify a magnet. Know when it has been passed. Identify the next magnet.**

NAVI generally needs reliable landmark awareness, not millimeter-accurate localization. Finer control at stations is a separate task.

**NGR: Finding solutions by asking better questions.**

The operator's analogies establish the intended abstraction:
- **Figure and ground:** Recognize the quiet background, not just the conspicuous excursions.
- **People in driveways:** The number, size, and appearance of people do not change the fact that the driveway is occupied.
- **Towns along a road:** Different towns have different buildings; an isolated house is not a town, and a brief clearing does not mean the town has ended.
- **A map of door colors:** A characteristic that is unstable in observation must not outrank reliable evidence of landmark sequence.
- **Eyes Wide Open:** Do not ask NAVI to navigate by one sense when Hall, PWM, IR, time, geography, and history are available.

## 2. Two physical states, not a sequence of threshold events

The physical interpretation is:

```
BASELINE → MM ENSEMBLE → BASELINE → MM ENSEMBLE → BASELINE
```

- **BASELINE:** Recognizable background between magnetic encounters.
- **MM ENSEMBLE:** Collective Hall activity attributable to one magnetic encounter.
- BASELINE continuing is not a new event; MM continuing is not another magnet.
- Provisional, unresolved, and confidence statuses are **judgment states**, not additional physical magnetic states.

The **Gauss ensemble** includes peaks, dips, shoulders, transients, threshold recrossings, and possibly differing polarities. These are observations of an encounter, not votes for separate mile markers. The detector should report observations; NAVI evaluates their meaning.

Ground must be identified by the signal's **pattern**, not by selecting map-derived geographic windows or forcing agreement with the legacy 70×2 detector. The central 100 mm between magnets was a useful study region, **not** a definition of ground.

A brief spike need not end BASELINE; a brief quiet interval need not end MM. An ensemble is an extended condition.

## 3. Baseline as a maintained, proximal reference

The operator's design decision:

> **Ground at the point the magnetic field is encountered is the baseline for that encounter.**

- While confidently in BASELINE, update the local reference continuously, with smoothing that rejects spikes. **Median of five** is the proposed initial smoothing method, not yet a validated window/cadence.
- Evaluate local stability, slope, abruptness, persistence, and continuity. Gradual baseline drift is not automatically a magnet.
- At a supported transition to MM, **freeze the most recent trustworthy proximal baseline** as the encounter reference; do not allow a quiet displaced magnetic plateau to train the baseline.
- After a supported return to BASELINE, resume background tracking.
- IR distance can express Hall slope per distance when credible; PWM history and elapsed time provide additional context for time-domain changes. Neither is automatically exact.
- Do not treat a fixed startup ADC level or absolute ADC band as permanent geographic truth.

The open engineering problem is defining reliable, causal transition evidence and a robust initialization/recovery method. A stationary, flat ADC value alone cannot establish that it is ground.

## 4. Movement history is interpretation context

NAVI has a chronological **ticker tape** of applied PWM, elapsed time, acceleration/deceleration, STOP/GO, IR measurements and credibility, Hall observations, previous encounters, and geographic history.

- A universal **650 ms** guard is not a substitute for reachability reasoning.
- PWM gives movement context; PWM alone does not prove displacement (the Grillers stall is an example).
- Credible IR travel is useful even if not accurate to the millimeter.
- Physical reachability, signal continuity, and the recent history together inform whether a new ensemble is plausible.
- A new encounter is **not automatically proof of the next numbered MM** if an earlier encounter might have been missed. Identity and presence are separate judgments.
- Do not make a detector's isolated classification authoritative merely because it matches its own numerical gates.

**EWO separation:** Sensor observations remain preserved; NAVI evaluates them with context. The new approach is not an instruction to recreate DART under another name.

## 5. STOP/GO does not erase the magnetic landscape

> **Stopping must not interrupt NAVI's evaluation of the Gauss topography.**

- Retain the current physical-state interpretation, proximal baseline, and developing ensemble through ordinary stops.
- When PWM falls, the expected progression through a field slows; PWM zero supplies stationary context, not a new geographic event.
- A flat reading while stopped over a magnet must not be relabeled ground merely because it is flat.
- On restart, continue interpreting the existing ensemble. Do not manufacture an opening or close an encounter because of STOP/GO.
- Preserve the installed movement-at-zero protection. Changes in Hall ADC while stationary are observations, not proof of geographic travel.
- A full power reboot with lost state is a **separate unresolved initialization problem**; external verification or abstention may be required. Do not silently assume the first stable plateau is ground.

The operator also proposed eventually using position-aware final ramps to reduce stops directly over magnets. This is **not** authorized as a current station-control change.

## 6. Polarity and geographic identity

Polarity is **not a prerequisite for declaring the presence of an ensemble**. Opposite-polarity lobes and transients inside one ensemble must not manufacture doubt about its existence.

Polarity may still have diagnostic or geographic-identification value **if** its reliability is demonstrated against repeated encounters. A proposed research comparison is opening polarity versus strongest absolute lobe versus whole-ensemble dominant polarity, in both directions. No operational polarity rule is removed by this document.

The DNA map and prior identity can assist with **which** MM was reached. They do not define whether the Hall signal contains an ensemble.

## 7. Timing and the magnetic-center hypothesis

The operator prefers the **complete ensemble** as a potential landmark reference, rather than calibrating every stop from the first threshold opening.

- Recognize developing arrival early, but preserve the full ensemble until return to ground.
- Once both boundaries are known, evaluate a candidate midpoint in **distance**, not simply elapsed time.
- The center becomes known only after the encounter; preserve actual decision time and distance already traveled.
- A center may correspond more closely to the physical magnet, but **the earlier midpoint comparison did not demonstrate superior repeatability**. This is a hypothesis for field measurement.
- Measure the travel and time from first indication to confirmed ground return, then compare center consistency over laps.
- **Do not change station MM targets, ramp profiles, stopping distances, or dwell behavior yet.**

Preserved station centers: **PATIO MM015; GRILLERS MM063; ARCHES MM108; BAMBOO MM157.** The operator considers the Patio double stop an interesting behavior, not an automatic defect to eliminate.

## 8. Evidence available and its limits

Reported Find the Ground research:
- Phase 1: **23,184,336** October 15 Hall ADC samples; **1,601** ground intervals, **1,184** bracketed activities; **1,187/1,195** recorded selections bracketed, eight unresolved.
- Phase 2: **1,768** ground intervals, **1,185** activities; **866** held-out reference activities one-to-one; 288 short interruptions judged background disturbances, nine clear additional departures and one ambiguous X002; one apparent recovery split.
- Phase 3: fixed-reference causal stream reproduced **1,768** ground intervals and **1,185** encounters, including **866** held-out; **376 bytes** persistent state, no dynamic allocation in tested recognition calls; sustained indication ~**40 ms**, confirmed departure ~**80 ms**, return confirmation ~**50.3 ms**. Host validation is not proof of ESP32 timing or independent physical identity.
- Fixed startup reference can be wrong if restarted inside a quiet displaced magnetic plateau. Adaptive variants introduced additional apparent splits.
- A reviewed recovery split at roughly **60.200–60.290 s** demonstrates why a detector-only decision can be misleading. Full Hall waveform and movement history are required before geographic interpretation.

The prior reports and their raw artifacts remain the source of truth for exact numbers, provenance, and exceptions. Do not present agreement with existing selections as independently proven magnet detection.

## 9. Authorized next experiment: observation-only parallel EWO

**Approved in principle:** a new ground/ensemble observer runs alongside the existing EWO_0_6 Hall detector for a controlled field test **after** resource verification and separate flashing/operation authorization.

The existing detector retains **all** navigation, motor, station, IR-credibility, and safety authority. The observer must not advance geography, alter PWM, trigger stops, change station targets, or silently replace existing Hall rules.

The observer should separately record:
1. Signal-only ground/ensemble interpretation and uncertainty.
2. Context-informed interpretation using PWM, IR, time, and recent history.
3. Encounter boundaries, provisional indications, actual decision times, and source sample identities.
4. Proximal baseline, smoothing, drift and slope measurements.
5. IR-supported width and candidate midpoint when available; otherwise unresolved.
6. Disagreements with the legacy detector, including splits, merges, and recording gaps.

Use bounded, allocation-free processing and bounded diagnostics. Validate CPU cost, stack/heap margin, Hall queue/input loss, and control-loop latency on target hardware before moving operation. Existing EWO_0_6 resource failures make these checks material.

**Authorized for Codex:** reconcile source, implement isolated research observer, host replay/tests, compile, and prepare field procedure.  
**Not authorized by this document:** flash, operate, change Pi services, change installed navigation authority, change station stops, or change the physical map.

## 10. Open questions for operator and research review

1. How does the observer establish ground at a cold boot or reboot over a magnet without adopting a quiet magnetic plateau?
2. How should median-of-five smoothing and slope measurements be sampled, and when exactly should the proximal reference freeze/unfreeze?
3. What evidence is sufficient to confirm a ground↔ensemble transition without unnecessary delay?
4. Can movement history resolve the known recovery split without suppressing genuine nearby encounters?
5. Does a completed ensemble midpoint improve repeated-pass station reference in actual field data?
6. How useful is whole-ensemble polarity in practice?
7. Does parallel instrumentation preserve acquisition and control-service margin on the ESP32?

**Design discipline:** Ask one unresolved question at a time. Keep observation, inference, and operating authority separate. Measure before adding exceptions. Prefer the least complexity that demonstrably improves reliable landmark awareness.

---
*This document preserves the design discussion and experimental intent. It does not claim a finished detector, a validated navigation model, or an approved operating change.*
