# NGR navigation: carry-forward ledger

**Purpose:** preserve the good engineering from each navigation line without importing the failure mechanism that later attached to it. This is a read-only evidence ledger, not an implementation plan.

**Evidence key:** **Field** = demonstrated on Otto or Toby; **Replay** = demonstrated only against recorded data; **Design** = a sound, explicit architecture point that was not adequately field validated.

## 1. DNA navigation — the 171-magnet foundation

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| The physical railway is a known, circular, direction-aware reference map—not a succession of anonymous sensor pulses. | **Field.** Bounded DNA runs showed polarity agreement and locked progression; later work retained `spacingMm[]` and route direction throughout. | The map must describe the *installed* magnets. A partial or stale polarity map cannot support global acquisition. |
| Use an ordered sequence, not a single magnet, to acquire or check location. | **Field / Replay.** This is the central DNA idea and later QUORUM/RVM evidence confirms why a single polarity bit is ambiguous. | A sequence should create candidates or confidence, not make one surprising sample a forced relocation. |
| Maintain a rolling motion/history record that can survive an imperfect individual observation. | **Design, supported by field failures.** DNA explicitly aimed to tolerate misses; RVM later showed a useful form of retained tracing. | Do not rewrite history to make a newly adopted label appear true. Keep the observation record immutable and attach interpretations separately. |
| Use measured segment lengths rather than nominal 300 mm timing. | **Design with direct measurement basis.** The actual `spacingMm[]` table replaced a known nominal-distance error. | Spacing and speed are supporting evidence; they do not independently prove that a detected event is impossible without a conservative physical bound. |
| Direction is first-class state. | **Field.** Direction changes exposed stale-buffer / stale-assumption errors early. | On a direction change, invalidate or explicitly reconcile direction-dependent evidence; never silently reuse the old sequence frame. |

## 2. Early templates / SOLONAV / CTO2

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| **Declare → count → refine.** A valid operator declaration is operational starting truth; subsequent evidence may refine it. | **Design and operating practice.** It avoids requiring a fresh magnet hit merely to become operational. | A declaration is not permanent proof; it needs an uncertainty model and must not be overwritten by one bad pulse. |
| Keep an odometer-like count as continuity, not as absolute truth. | **Field-informed.** It gives a stable prior through same-polarity sections. | The count must expose its uncertainty; CTO must never consume a bare marker count as exact occupied track. |
| Treat a train as an occupied **front/rear envelope**, not a Hall-sensor point. | **Design, motivated by CTO2 rear ends.** | The envelope must include localization uncertainty and actual consist geometry. Fixed offsets around a falsely exact MM are unsafe. |
| Traffic is a temporary interruption of a train’s own route/station mission. | **Field-demonstrated in parts; preserved CTO principle.** | The interruption must not allow a held train to be mistaken for a completed station visit, nor resume merely because a stale peer packet disappears. |
| A silent or untrusted peer is an obstruction, not clear track. | **Design; strongly safety-motivated by peer-stale field events.** | This is only safe if the stop response itself is physically adequate for closing speed and distance. |

## 3. QUORUM

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| Keep several location hypotheses and require a unique leader with margin before relocation. | **Field and replay.** QUORUM could recover bounded corrections and avoided arbitrary tie-breaking. | Candidate space must be symmetric enough to represent the observed error direction. The original `{-1,0,+1,+2,+3,+4}` fence was biased toward apparent misses and could not express common phantom-driven −2/−3 slips. |
| Honest uncertainty (`NO_QUORUM`) is preferable to a fabricated position. | **Field.** Out-of-fence declarations and ties stopped rather than guessed. | `NO_QUORUM` must be a controlled operational state, not a pathway for CTO/stations to keep using old coordinates. |
| Quarantine suspicious events pending later evidence. | **Field.** The MM008 migrated/stacked-magnet event was made diagnosable: suspicious weak, physically implausible events were held, then either discarded or committed. | Quarantine cannot derive its expectations from a label already known to be questionable; it needs causal, observable criteria and audit records. |
| Publish the alternatives, scores, winner, runner-up, and reason. | **Field forensics.** The later label-slip diagnosis was possible because these records existed. | Preserve raw event identity and avoid rewriting labels in the evidence ring after adoption. |
| Separate navigation uncertainty from motor authority. | **Design, partly fielded.** A navigation opinion should not silently seize manual control. | In AUTO, loss of trustworthy position must withdraw position-dependent authority; it must not manufacture a safe CTO clearance claim. |

**Important correction from the field record:** many apparent positive “miss bursts” were cascades after a label slip. The original fence’s premise—frequent multiple misses but at most one phantom—was not a clean measurement of physical errors. A holdout replay of symmetric `{-3…+4}` reduced `NO_QUORUM` 10 → 6 with five −2/−3 recoveries and no new `NO_QUORUM`, but it was never approved or field accepted.

## 4. NAVI_ONE / X18–X21

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| Per-locomotive sensor characterization, rather than copying Otto constants to Toby or vice versa. | **Field.** The two locomotives have materially different Hall/motor behavior. | Characterization values are evidence/configuration, not universal “truth” that overrides a new observation. |
| Record the complete Hall passage/waveform and preserve it for diagnosis. | **Field and replay.** The X-line’s archaeology exposed baseline latches, lurches, tails, and wrong-shape false stops. | Morphology can be diagnostic; it must not automatically revoke a valid physical opening or create a navigation strike near a threshold. |
| Separate raw recording from filtered/judged interpretation. | **Design and source practice.** It kept later review possible. | No filter should erase the raw path that would reveal that it was wrong. |
| Retain station geometry, controlled ramps, dwell, and locomotive/direction-specific stop calibration. | **Field.** Station service repeatedly demonstrated useful operational behavior across the line. | Arrival must not be inferred merely from the ramp reaching PWM zero; location uncertainty and physical arrival are separate facts. |
| Develop with a clear failure witness. | **Field diagnostic value.** “Tough love” revealed real defects. | Do not deploy the development strike policy as passenger operation: 115 wrong-magnet stops in 71 sessions show that one transient cannot be an operational stop rule. |

## 5. NAVI_SIMPLIFIED

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| **Detection → Hard Protection → Transparent Judgment.** Sensor detection reports facts; navigation decides their meaning. | **Design, strongly supported by the NAVI_ONE failure history.** | Do not confuse electrical rearm or waveform closure with physical clearance to the next mapped magnet. |
| An opening is a timestamped, polarised sensor event; later waveform information is diagnostic rather than retroactive deletion. | **Design.** The proposed two-sample opening makes the evidence boundary inspectable. | Detection still needs same-magnet/restart protection before it becomes a mapped advance. |
| Preserve ambiguity and collect subsequent evidence rather than instantly choosing or striking. | **Design.** This is the direct antidote to one-bit/polarity cascades. | The recovery counter must count distinct physically possible future observations—not repeated reads of the same magnet. |
| Hard Protection is about physical impossibility, not confidence or convenience. | **Design grounded in measured route spacing.** | If the bound is unknown or weak, be permissive and expose uncertainty. A global PWM or nominal-speed threshold is not physical proof. |
| Event-keyed traceability, monotonic drop/suppression accounting, and exact running-build identity. | **Design, necessary from past unrecoverable incidents.** | A message queue or dashboard must not be allowed to erase the only evidence explaining a safety decision. |

The sole railway run failed because same-magnet rereads entered the recovery word after physical protection was disabled when position became unknown. That rejects that implementation, not these separations of responsibility.

## 6. NAVI_COHERENCE / geographic stations / EWO

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| Current position, current direction, and current operating overlay should determine what is required now. | **Design and EWO operational practice.** It removes historical visit/phase residue as hidden authority. | A station’s current requirement is not evidence that the locomotive’s position is correct. |
| Station behavior should be geographic and explicit, with transparent current state. | **Field.** EWO/NAVI runs demonstrated station approaches and dwell/departure behavior. | Do not let a prior phase, timeout, or “completed visit” suppress a current safety requirement. |
| Keep independent observations separate until their credibility and authority are explicitly demonstrated. | **Field forensic result.** EWO’s Hall-loss run was useful, but logs showed IR inference was not actually added during the cited miss episode. | Do not claim IR rescued localization merely because position stayed marked reliable; publish the actual inference/action. |
| Stop/restart and motor-zero are genuine navigation cases requiring retained context. | **Field.** Stops over magnets created failures in multiple generations. | PWM zero is not itself a detector veto or proof of spatial clearance. Preserve the last physical anchor across dwell/restart. |
| Remove obsolete compatibility/procedural machinery that can acquire authority. | **Design review result.** The EWO cleanup identified old phase, timeout, pause/resume, and visit-latch paths as dangerous. | Preserve measured protections and field evidence before removing code; “simpler” is not automatically safer. |

## 7. RVM manual / continuity

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| Preserve the **native Hall tracing** as a time series, rather than reducing the world immediately to open/close events. | **Field.** It supported the 169/169 and 171/171 progression runs and made later diagnoses reproducible. | Trace storage/feeding must remain bounded; the original manual build encountered a watchdog/backlog failure. |
| Recognize a complete magnetic landscape/ensemble rather than a single threshold crossing. | **Field and replay.** RVM manual/continuity produced the strongest later single-locomotive progression evidence. | The learned recognizer is locomotive/condition-specific and still needs a recovery mechanism; a wrong-polarity ensemble was an irreversible contradiction. |
| Preserve genuine motion through ordinary stops, restarts, long holds, and slow running. | **Field and firmware-parity replay.** Continuity preserved four long stops in the 272/272 run and avoided old hold-back loss. | Continuity must not hide a moving passage or create feedback with the recognizer’s own drifting baseline. |
| Use parity replay against the actual firmware path before claiming a replay result. | **Replay discipline with strong value.** C++/Python parity was exact across ten RVM recordings. | Parity proves implementation equivalence, not field generalization to a different locomotive, temperature, or operating mode. |
| Keep the core navigation observer separate from actuation while it is being proved. | **Field strategy.** The manual core generated its best evidence before AUTO/bubble integration. | Do not infer that observed correct MM claims alone prove safe station stopping or peer spacing. |

## 8. RVM AUTO / bubble revisions

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| Explicit peer freshness, same-direction checks, dynamic role reporting, and a declared response to peer silence. | **Design plus partial field exercise.** | They are only consumers of position; they cannot repair a frozen/wrong local location. Treat any uncertain peer as an obstruction. |
| Use a simple, inspectable station speed carpet rather than opaque timer-only release behavior. | **Host-tested and partially fielded.** | It needs location acquisition, real stopping-distance validation, and a separate proof that follower release cannot touch the leader. |
| Keep a source/flash record for each field image. | **Field process.** The r1–r5 chain could be reconstructed because commits and flash records were retained. | Preserve the binary/readback too when possible; the r5.1 source is retained but its flashed binary cannot now be proved byte-for-byte. |

**Do not carry forward as a package:** r1–r5.1 added coupled pause, continuity, moving-reference, station, and peer corrections. The Arches collision and MM038/MM049 contradictions show why a field-proven core must remain separable from the operations layer.

## 9. Whole-tracing aligner and CPR research

| Carry forward | Evidence | Boundary to retain with it |
|---|---|---|
| Whole-history/path matching can revise an earlier interpretation when later evidence arrives. | **Replay.** The Oct 16 Viterbi-style aligner differs fundamentally from irreversible marker counting. | It was never real-time fielded and is calibrated to Otto’s historical Hall landscape; do not call retrospective alignment navigation. |
| A soft movement prior is better than treating PWM as a hard truth source. | **Design.** The aligner used movement/IR as a cost, not a veto. | Its template and priors require cross-locomotive and environmental validation. |
| Freeze inputs, model, code hash, and evaluation protocol before judging a candidate. | **Replay methodology.** The RVM/CPR work made provenance limitations visible. | Frozen evaluation must precede tuning; do not claim untouched holdout status when a model changed afterward. |
| Compare causal components in isolation. | **Replay / research.** CPR’s principal value is that it preserves negative results rather than blending them into a new model. | It has no live-running evidence and should not be promoted as an operating architecture. |

## Non-negotiable carry-forward rules

1. A sensor event, a navigation interpretation, and a movement command are different things.
2. Keep raw, timestamped observations and immutable event IDs; attach hypotheses rather than rewriting evidence.
3. Position uncertainty must be explicit and must travel with any peer report; never broadcast a bare MM as exact occupied track.
4. A position error can cascade. Once it may be wrong, do not use the stale expected label to “prove” that real subsequent magnets were missed.
5. Station operations and two-train spacing are downstream consumers of location. They cannot validate a navigation model merely because a few stops look right.
6. Field success on one locomotive/condition is not portability proof. Otto and Toby must each earn it.
7. Preserve successful station behavior and original source/images before replacing anything; field identity must be recoverable.

## Primary sources reviewed

- Local `esp-loco-control` commit history, sketches, tests, replay packages, and field records.
- Processed ChatGPT discussion collections, especially the DNA/NAVI and implementation chapters, including original passages rather than only summaries.
- Key records: `CTO2_HARVEST.md`, `QUORUM_LABEL_SLIP_ROOT_CAUSE.md`, `QUORUM_HOLDOUT_VERDICTS.md`, `NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md`, `NAVI_RVM_CONTINUITY_MANUAL_DELIVERY_20261019.md`, and `NAVI_RVM_MODEL_INVENTORY_AND_EVALUATION_PLAN_20261021.md`.
