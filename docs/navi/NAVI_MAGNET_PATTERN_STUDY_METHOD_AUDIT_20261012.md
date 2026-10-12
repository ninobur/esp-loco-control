# NAVI magnet-pattern study — methodological audit and engineering interpretation

**Date:** 2026-10-12  
**Status:** Research finding; no magnet or firmware change authorized  
**Source:** User-provided Work study and subsequent Work methodology audit, with independent ChatGPT engineering interpretation. The underlying ZIP audit was mentioned but was not supplied for independent inspection in this session. Numeric claims below are **reported**, not independently reproduced.

## 1. Scope and decision status

The magnet-pattern study compared Gold W12, strict NS alternation, NNSS, and optimized nonrepeating sequences for a known-start loop of 171 magnets, with an optional 172nd magnet. The original recommendation to reverse 12 W12 magnets **was withdrawn** after a methodology audit. Do not treat the ranking as validated operational performance. Retain W12 pending a NAVI-representative decision-model comparison. No firmware or magnets were changed in this study.

## 2. Findings reported by Work

- Strict NS alternation: an opposite-polarity return-flux lobe can mimic the expected next pole; two missed magnets are invisible to polarity phase alone.
- NNSS: may reveal some skipped or extra encounters more quickly, but four-magnet phase shifts are indistinguishable by polarity alone.
- W12: long same-polarity runs, including seven N markers near Arches, can postpone polarity-only error detection.
- Valid measured travel distinguished nearby false extras immediately across tested patterns; polarity changes contributed little in those specific cases.
- The original synthetic mixed-error decoder reported 100% eventual recovery for W12 and some nonrepeating candidates versus 65% for NNSS172; **these are not operational NAVI reliability estimates**.
- A 12-reversal W12 candidate shortened the longest run from seven to five and changed the original simulator's worst resolved mixed-error recovery from 18 to 15 genuine observations. **No physical validation; operational recommendation withdrawn.**
- A proposed 172nd magnet and spacing redistribution had no established navigation benefit.

## 3. Why the original recovery metric was misleading

The worst 18-observation case began at a **known** MM106, CCW. Three genuine markers (MM105, MM104, MM103) were omitted; no false observations were inserted. The simulator used a bounded minimum-edit alignment decoder and reported the first point where its last-matched marker was uniquely correct for the rest of the trial. This combined incorrect declarations and unresolved ambiguity, and compared last *accepted* marker to most recently *physically passed* marker.

The first inconsistency appeared at observation seven; unique resolution occurred at observation eighteen in the polarity-only mode. Thus **18 was not the number of magnets needed to reject a nearby return-flux lobe**, nor a measure of NAVI's actual normal recovery performance.

In the simulated IR mode, the same scenario resolved by the fourth supplied observation. At the first observed real marker MM102, approximately 1,195 mm of travel supported skipping three magnets, yet the decoder preferred treating the observation as an insertion because it cost only one edit. This was an artifact of the scoring rule, not evidence that W12 intrinsically required four encounters.

A separate test with trusted MM096 CW, next MM097 N approximately 300 mm away, and an unexpected opposite-polarity Hall observation at 20 mm was rejected immediately with usable simulated IR.

## 4. Confirmed methodological deficiency (from Work's self-audit)

Although initialized at the correct known position and direction, the decoder did **not** faithfully preserve NAVI's last well-supported geographic anchor as distinct from competing hypotheses. Alternative alignment paths could assign new markers and distance anchors before sufficient confirmation. Its movement mode used a hypothetical 600 mm/s ceiling rather than actual PWM, speed, and acceleration history. Polarity-only mode omitted all distance and timing evidence. The shared decoder was therefore a known-start **sequence alignment** study, not a faithful model of NAVI's **trusted-position, next-encounter judgment**.

Work explicitly withdrew the 12-reversal recommendation and the interpretation of 65% versus 100% as real NAVI performance.

## 5. Engineering analysis to preserve

**A. Correctly frame the task.** NAVI normally knows the last well-supported magnet, direction, next expected polarity, mapped distance, movement command history, and (when valid) IR travel. Its normal task is to judge the *next* encounter, not rediscover a position among all 171 markers after every anomaly.

**B. Protect evidence, not assumptions.** Preserve the last well-supported geographic position separately from provisional physical progress and alternative location explanations. A candidate observation must not automatically create a new authoritative distance anchor. Conversely, the trusted position must remain revisable if accumulated evidence warrants correction.

**C. Distinguish sensing from identification.** A Hall observation can be a genuine physical field response yet *not* a new magnet; opposite-polarity return-flux lobes are a principal example. A disagreement is not automatically invalid sensor data. Evaluate whether the observation represents the expected magnet, a secondary lobe, a missed marker, a mistaken earlier anchor, or unresolved ambiguity.

**D. Use physical reachability and distance appropriately.** If reliable travel is only 20 mm and the next mapped marker is 300 mm away, a new-marker declaration is unsupported; there is no reason to wait for a long polarity signature. Without valid IR, physical reachability, recent movement, PWM history, and subsequent DNA sequence can constrain alternatives, with uncertainty retained when necessary.

**E. Distinguish four different outcomes.** (1) Recognizing an inconsistency; (2) excluding a false extra encounter; (3) identifying *which* historical observation was false; and (4) recovering from a genuinely incorrect geographic declaration. A decoder can succeed at one without succeeding at the others.

**F. Pattern and decoder must be evaluated together.** Repeating patterns have real polarity-only ambiguities, and long W12 same-pole runs have local limitations. But no ranking of operational patterns is justified until the decoder models the actual navigation decision and relevant physical evidence. Reproducible simulation can still faithfully answer the wrong question.

## 6. Next research gate

Before recommending magnet reversals, additions, or firmware changes:

1. Reconcile the latest governing Hall, IR-validity, fallback, physical-reachability, stop/reversal, and position rules. Do not silently replace them.
2. Specify an inspectable decision model: observation → plausible explanations → evidence comparison → provisional/revised position → confirmation or retained uncertainty.
3. Replay real failure cases, including nearby opposite-polarity lobes, the MM106 CCW three-miss sequence, stale anchors, repeated same-magnet signals, long same-pole runs, IR invalidity, and both directions.
4. Report event-by-event evidence and decisions without using future observations at decision time; ground truth is for scoring only.
5. Keep last confirmed landmark, physical progress, provisional position, and actual ground truth distinct.
6. Validate against additional cases not used to devise the model.
7. **Only then** compare W12, NS, NNSS, the 12-reversal candidate, and other 171/172 configurations under the same representative decision model.

## 7. Source and verification limitations

This record combines the user's pasted Work findings, Work's pasted eight-question methodology audit, and a separate ChatGPT engineering critique. It does not claim direct examination of the simulation source, ZIP audit, raw telemetry, or historical repository records. Before promoting any specific numerical result or source claim to validated status, attach exact source paths, commits, and reproducible outputs.

**No implementation approval is implied.**
