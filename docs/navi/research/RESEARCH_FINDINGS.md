# NAVI engineering research — verified working findings

**Execution:** 2026-10-12. **Status: research in progress.** This is a durable Phase I checkpoint, not the completed engineering history, an implementation specification, or a replacement for approved decisions. The 54 substantively reviewed repository content revisions are listed in [SOURCE_INDEX.md](SOURCE_INDEX.md). The remaining revision ledger is explicit; retrieval is not review. Earlier history has only been selectively investigated. Original field logs underlying the reports below remain unreviewed in this execution unless specifically stated.

## Evidence and authority conventions

Use two independent properties. **Authority:** GOVERNING, PROVISIONAL, SUPERSEDED, REJECTED, or UNRESOLVED. **Validation:** VALIDATED only for the particular experiment or deterministic property actually supported; otherwise PROVISIONAL or UNRESOLVED. A source's claim to be accepted is recorded as its claim, not independent proof of a newly supplied operator approval. An implemented rule is not automatically approved or field validated.

Distinguish direct source inspection, a contemporary experiment report, an operator retrospective, and this investigation's interpretation. These working finding IDs are historical research identifiers, not new numbered engineering decisions. Preserve them when integrating the final three documents.

## RH-001 — Current state is distributed across branches

**Period:** September 19–October 12. **Authority/validation:** UNRESOLVED current engineering baseline; VALIDATED Git ancestry facts.

`main` points to `7409c19d3199511759f837d31a1054ba0822fd2d`, dated September 25. Its recursive tree has 277 entries; the October 11 STOP-correction tree has 1,603 entries. Neither a default branch nor a later timestamp establishes approval or track acceptance.

The 0.3 operational foundation is `85a2cf2a16d262abdf7258c60f60508b823913ee`, the 0.1 MM045 STOP lineage. The complete 0.3 candidate is `16c65f61662c358923c780a2edffda9733f0a8da`. `253faa464692b97ab48beb0aa621aa11372aaa55` is its IR qualification correction. `c4cd2b2b8edd5641a0c4a7a31191614f7cba153e` and `4ad1df225f3768c192330c634b53d114faabf894` are **siblings**, each with parent `253faa4`.

Direct inspection of `NaviIntegratedCore.h` establishes that the STOP branch still has `irMagnetVetoAvailable()` using `decisionUs_`. The review branch removes that overload and requires an explicit current time. Production callers on the STOP branch already pass time; the remaining overload is a reuse/API defect, not evidence that those callers necessarily exhibit stale validity. A successor must not describe `4ad1df2` as including the entire `c4cd2b2` correction. No merge was performed.

Sources: [branch snapshot](branches.json), [commit snapshot](commits.json), source index entries for both core blobs and the correction reports.

## RH-002 — The October 11 audit must be reconciled against its source snapshots

**Problem:** A review can accurately describe one branch while incorrectly implying that no conflicting record exists anywhere. **Validation:** direct documentation and commit comparison; approval remains UNRESOLVED.

The 0.4 audit at `0db254fb59612d8f73fab30a785a25649499778b` says numbered records were not amended to cover raw 70×2 detection or DNA recovery. It used governing records from `85a2cf2` and the primary guide at `bd0b15f`. However, `cbdfe9495ca1b52d2165a5ce6be27cbfce1d1439` already amended 0109, 0110, 0113, 0116, 0122, the decision-model index, and governing directory on the **0.2 branch**. Exact file-change metadata and the full relevant records were inspected here.

The amendments are themselves incomplete reconciliation: 0109 adds retrospective DNA and withdrawal after ten unresolved encounters, then retains a body excluding automatic correction/count limits; 0110 adds raw 70×2, then retains median-of-five and rejection of two-sample persistence; 0116 adds conditional IR and Hall/DNA identity, then retains mandatory IR and withdrawal of Hall-only fallback. Preserve all clauses and their commit provenance. Establish the original operator instruction before assigning supersession scope; do not independently choose an architecture.

The latest examined primary guide is at `c5fcef1c741dbb217699f7eda4f9a99bf3123053`, later than the audit's guide baseline. It expressly records cruise PWM 90 and grade retention. Thus the audit's guide-versus-code PWM 60/90 discrepancy is partly a **version-selection discrepancy**. This does not resolve every STOP-profile difference: later implemented penultimate 50→35/300-ms calibration must be reconciled with guide 50→30 wording and its trial records.

## RH-003 — Pitch is branch-specific evidence, not a value to select by recency

**Problem:** The installed measurement scale must be distinguished from a candidate's altered configuration. **Authority:** original configuration principle GOVERNING; numeric conflict UNRESOLVED. **Validation:** direct patch/source/report inspection.

At `cbdfe949`, 0122 changes the stated 0.2 pitch from 9,652 to **10,000 µm/pulse**. The retained context still identifies the transmitter as 9.652 mm/pulse. The 0.3 STATUS and verification records explicitly preserve **9,652 µm/pulse** from the operational 0.1 foundation. The mandatory guide also describes nominal 9.652-mm pulse resolution.

Do not infer permission to apply 10,000 to 0.3 from the newer 0.2 decision-file text. Do not erase the 0.2 change or claim it never occurred. Native approval provenance and actual transmitter configuration require further investigation. This research changed neither calibration nor firmware.

## RH-004 — Hall detection, encounter qualification, and geographic identity are distinct

**Implementation examined:** entire 0.3 sketch and the qualifier, bridge, integrated core, and pure identity function at `4ad1df2`.

`hallTask()` records one ADC conversion per 1-ms tick with serial, acquisition PWM and direction. `HallQualifier::observe()` requires departure ≥70, two same-polarity samples, consecutive serials and 0.5–1.5-ms separation. It retains the first sample's opening coordinate/time separately from the second sample's detection time. A continuous field is latched; a below-threshold sample clears that latch.

When usable IR supports the span, the opening must satisfy both sides of the ±15% mapped interval. Otherwise the 650-ms detection-to-detection fallback applies. A rejected window does not move the exclusion origin. Only admitted encounters enter the 12-entry DNA history; the history is therefore a **qualified-encounter ledger**, not every raw excursion. Native samples may still exist in recorder evidence. This distinction matters whenever a history claims to preserve every physical encounter.

`judgeExpected()` checks known direction and target polarity, returning a proposed identity; the integrated core alone commits `mm_`. A comment in the pure function that IR disagreement is not a veto must not obscure the actual full-window qualification upstream within NAVI. Read the complete call path.

**Unresolved:** adequacy of physical duplicate protection during nonzero-PWM stalls or threshold leave/re-entry when trustworthy displacement is unavailable. Source inspection shows the fallback is temporal; it does not establish physical travel. This is not a newly approved restriction or proof that this exact 0.3 candidate has failed on track.

## RH-005 — IR stream continuity does not prove complete physical pulse acquisition

**Period:** September health/epoch work through October 11. **Authority:** applicable measurement and no invented counts principles GOVERNING; general physical missed-pulse discriminator UNRESOLVED.

The historical `IrOdometryEpoch` explicitly carries owner, epoch, boot, configuration and order identity. A ended epoch cannot silently reopen under the same ID. Its diagnostic counter changes and readiness can end epochs. In contrast, 0.3's `observeIr()` records optical health but admits mathematically coherent Type-5 distance independently of contrast, saturation and abort diagnostics. These are different implemented validity models, not interchangeable versions of one policy.

The current Type-6 parser checks wire structure, CRC, installed scale, source/boot, sequence, completed counts, ordering and interval coherence. This establishes observable detected-event integrity, **not that every physical optical cycle was detected**. A wholly missed cycle increments neither the detected count nor event sequence. Consecutive reports may remain internally coherent while distance undercounts.

The initial 0.3 timing monitor treated >1.5× median and returning-normal brackets as authority to withdraw IR's veto. `253faa4` removes that authority because the same findings occur in the healthy control. It also expires Type-6 support at the existing one-second bound and enforces the full mapped qualification window. `c4cd2b2` adds an observable interrupted-cycle test, but explicitly disclaims a general physical missed-pulse classifier. Neither healthy-looking counters nor a long interval alone satisfies a universal no-missed-physical-pulses requirement.

**Engineering consequence:** preserving ±15% admission is not sufficient if its validity predicate cannot distinguish the undercount that makes genuine landmarks arrive outside the window. Do not invent a detector or widen tolerance during documentation.

## RH-006 — Instrument availability, epoch identity, and geographic synchronization are separate

0091 proposed ten-marker trust, keeping trust through stops, and recalibration after restart. Its status also requires ratification; operator quotations and a Proposed status must both be preserved. Accepted 0094 deliberately reverses the probation portion of 0092. Its embedded transcript explains the cyclometer analogy: a fresh report with unchanged counts is zero **observed** movement; lack of a new pulse is not lack of communication. Geographic correction history is a separate “ten.”

The successful 20Q3 recovery chains in the September 26 reports illustrate a further distinction: a new healthy measurement epoch can resume, while its old mapped MM origin remains unusable; the next accepted boundary synchronizes the new epoch to geography. The current EWO lineage later withdrew parts of that Hall-only resynchronization policy. Preserve the earlier measured success without treating its authority model as automatically current.

## RH-007 — Successful recovery can conceal an initial acquisition defect

**Experiment:** Toby COHERENCE 0.5 first AUTO run, September 22, boot `F37B52F0930A43B6`. **Classification:** reported partial field success; raw log review pending.

The contemporary report records 710 advances, 13 refusals, 15 station services and a correct final 041–042 interval. It also records a weak Arches departure event accepted as MM111 while IR was excluded, followed by sequence correction MM117→116. A later 700-ms duplicate with only 38.6 mm of IR progress is refused. The success is recovery and operational continuity; it is not proof that initial acquisition was correct.

The report also records a strong correct-polarity observation refused at 251/300 mm (0.837), followed by a two-step advance. A detected observation rejected later is not a magnet the Hall acquisition missed. Station offsets in this report were derived from NAVI, and physical platform checks were explicitly absent; retain that limit despite its broad opening verdict.

## RH-008 — Pattern uniqueness does not establish physical reachability

**September 23 investigation:** logged correction MM065→166 occurs while a station ZERO_RAMP is active, with incumbent agreement 8/10. Subsequent station dwell/departure use offsets around −65. The operator separately reports physical 029–030 versus displayed 163. The endpoint alone does not prove that the earlier correction quantitatively explains every later error; intermediate labels are NAVI belief, not independent ground truth.

The historical response was geographically proximal recovery. EWO later excluded search-based recovery; subsequent candidates reconsidered bounded DNA. Do not reinstate a historical ±10 bound or current candidate criterion simply because this failure is real.

This execution independently verified the map's combinatorial claim: for **each known direction separately**, all 171 cyclic 10-pole words are unique. At lengths 7/8/9, positions with unique words are 65/129/163; distinct words are 113/149/167. These are different metrics. This math neither validates observed-event completeness nor authorizes map-wide relocation. See [map_verification.json](map_verification.json).

## RH-009 — A physical recovery gate can become circular

**Experiment:** Toby 20Q3 run 3, September 26, reverse movement. **Classification:** reported demonstrated identity failure with source-corroborated mechanism; primary log review pending.

The report identifies 32 real opening poles matching physical MM022→053, while NAVI assigns MM020→051. All admitted IR spans fit ±15%; regular spacing cannot detect the two-marker identity offset. The operator confirms the final physical MM053.

Actual `ProximalRecovery::reverse()` clears `reference_`, sets `settled_=true`, and resets directional positions. After maturity, `evaluate()` holds with `TRAVEL_UNAVAILABLE_HOLD` when that reference is absent. The normal establishment path requires the incumbent to agree fully. When the incumbent is wrong, agreement cannot supply the reference needed to evaluate the corrective alternative. This corroborates the report's circularity explanation.

Retaining the incumbent is valuable evidence inertia, but a mechanism that cannot evaluate contrary evidence can preserve error permanently. Reject the circular dependency, not the general principles of physical bounds or retained history.

## RH-010 — Strong sensor runs must not be mislabeled as complete navigation acceptance

20Q3 run 1 demonstrates a four-marker fallback-to-epoch handoff and correct endpoint, explicitly not broad acceptance. Run 2 reports three recovery chains and an IR-rejected 402-ms extra event, followed by handling-contaminated false advance. Run 5 reports 926 correct opening poles over five laps but **no effective position declaration**; every NAVI event is `NO_POSITION`. That is strong detection evidence, not location or AUTO validation.

The X22-derived held reference remained successful across slow-run cadence rejection and hundreds of later locks. Novel authority structures should retain demonstrated acquisition insights without automatically carrying upstream judgment or old control policy.

## RH-011 — Geographic operation and physical actuator execution have different authority

0121 makes current position, direction and overlay determine the requirement; historical ARMED/visit/phase state cannot grant operating permission. The October 9 guide subsequently makes the base cruise-only, retains grades, selects one PWM overlay, and defines a portable destination-based STOP sequence before station and CTO applications. Closed-loop homeostasis, adaptive glide, the 95% handoff and ticker-tape playback are superseded in the guide; earlier decisions/implementations remain history.

Applied PWM zero starts dwell; dwell expiry with applied PWM still zero completes timed service. Target error and IR-rest detection are not completion gates. The actuator continues an established final ramp without new pulses. Actual observed IR displacement at PWM zero still invokes the distinct 0120 intervention rule. A timer completes a maneuver; it does not locate another magnet.

The examined 0.3 implementation retains a target at MM045 plus 150 mm, final decrements at 300 ms/count, five-second applied-zero dwell, 150-ms/count restart and geographic grades. Its STOP adapter reads NAVI; it has no second estimator. These deterministic mechanics do not establish arbitrary-location physical braking performance or future two-train collision protection.

## RH-012 — Accepted MM and a fine IR coordinate are different knowledge dimensions

At `253faa4`, `confirm()` can establish `positionReliable_=true` and `physicalPositionAnchored_=true` without a usable fractional IR origin. The old `stopView()` admitted coarse geography only when `!physicalPositionAnchored()`. Thus an accepted MM could be withheld from STOP even though NAVI identity was valid. `4ad1df2` admits the existing MM lookup when fine offset is unavailable, keeping unreliable-position and PWM-zero-hold guards.

The seven production-code handoff cases were reproduced in this execution with ASAN/UBSAN and warnings as errors; **all pass**. They exercise no IR, stale IR, both directions, fine IR, fine-to-MM fallback, Manual, rejected Hall and pending DNA. This is VALIDATED deterministic software handoff, **not track acceptance**. Broader Hall-only operating authority remains subject to the unresolved governing reconciliation in RH-002.

## RH-013 — October 9 failures are not one established mechanical cause

The MM045 investigation records a third maneuver stuck before the final section, never entering braking/dwell/release. Dispatcher STOP came later; it cannot explain the preceding failure to complete. Two navigation onsets differ: one long counted interval with subsequent optical discontinuities, and a later periodic eight-count long-interval pattern despite TRACKING and stable abort counts.

It also identifies a concrete qualification/anchor mismatch: decision-time distance could pass the window while the earlier saved Hall anchor lay outside it. Re-anchoring at a wrongly identified same-pole landmark can make later residuals look normal. A coherent next span does not retroactively verify that anchor's identity.

The original reports do not independently identify every rejected physical marker or distinguish optical misses from wheel slowdown/slip/binding. Native October 9 Hall/optical data are absent from the cited recording investigation. Missing MQTT event records do not mean missing cumulative measured distance. Preserve the reports' detailed counts and exact source hashes; further raw-log/source checks remain mandatory.

## RH-014 — The October 2 verdict contains a material authority contradiction

The field verdict reports eight station cycles and 28 IR displacement notifications during hand turning; exact flashed commit is not in the MQTT log. These are one handling episode, not 28 railway failures.

Its 0120 hyperlink and prose say displacement did not require redeclaration and interpret code requiring it as nonconforming. The actual examined 0120 record **requires** operator verification/declaration and withdraws 0119 automatic recovery. The verdict reflects a different historical interpretation. Do not cite it as current 0120 policy; do not erase it. Reconcile timestamps, source versions, operator instructions and numbering/path changes. Its empirical observations may remain useful despite its policy interpretation.

## RH-015 — Earlier foundations are targeted evidence, not present rules

The supplied June navigation brief separates persistent location from disposable timing and identifies blindness during ramps as a defect. It proposes every Hall hit advancing a relative count and truth clusters correcting that count. Later NAVI explicitly separates magnetic observation from encounter/identity; preserve the conceptual ancestry while identifying that authority change. The brief's “field-validated” June claims have not been checked against original June recordings here.

The supplied DNA “truth source” describes an earlier MM062→142 exact-pattern relocation and advocates continuity before changing reported belief. It is an undated discussion summary with proposed numeric thresholds, not a current approved recovery algorithm. The reversing note preserves position across intentional reversal, but its simple last-marker expectation cannot be imported into current code without the later stopped-on-marker evidence.

Only the supplied text fragments and embedded transcript excerpts in reviewed records have been examined. No absent ChatGPT conversations have been reconstructed from memory.

## Integration into the eventual three documents

CONTINUITY should lead with authority precedence, branch divergence, current candidate limitations and authorized scope. DECISION_HISTORY should expand RH-002/003/006/008/009/011/014 into dated predecessor/successor records with approval provenance. EXPERIMENTAL_HISTORY should expand the report-supported field cases above, retaining exact raw-log, build, boot, contamination and reinterpretation limits.

These are starting records, **not a substitute for the remaining exhaustive review**. Before finalization, cover every remaining distinct revision, necessary earlier baseline and supporting dependency, unpack source archives, verify primary evidence, resolve or explicitly preserve contradictions, verify links, and reconcile source completeness. No firmware change, rule, merge, deployment or field activation is authorized by this checkpoint.
