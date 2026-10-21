# NGR Navigation Model Retrospective

**Cut-off:** 21 October 2026
**Scope:** read-only reconstruction of historical navigation work for Otto and Toby. “Worked” below means demonstrated on the railway, not merely a successful replay or an attractive design.

## Conclusion

There are two different answers, depending on what “reliable navigation” means.

1. **CTO/QUORUM must not be called a proven collision-free two-locomotive system.** It demonstrated substantial coordination and station-service episodes, but CTO2's repeated rear-end collisions arose when clearance consumed confidently wrong position. Later CTO3/QUORUM work never established that this danger was eliminated; a September independent review records the operator result that the follower “kept rear-ending the leader.”
2. **For the later effort to create a robust single-locomotive navigator, NAVI_RVM—specifically the manual/continuity line—came closest.** Otto completed a 171/171 full-lap progression, a 272/272 claim run with four long stops preserved, and later Otto and Toby each produced long correct sequences. This is the best direct evidence for a replacement localization core. It was not yet a reliable autonomous two-train system.

The critical failure was not that RVM’s core recognizer had been disproved. It was that the project put station automation, peer spacing, pause/continuity logic, moving baselines, and single-strike stop authority around it before the core had been field-accepted across both locomotives and operating conditions. Each corrective revision repaired the preceding failure but created another dependency. Revision 5.1 then produced live contradictions at MM038 and MM049 on 21 October. That makes RVM promising but **not operationally accepted**.

The older promising approach most clearly abandoned before an adequate field verdict was **QUORUM’s plural-candidate / recovery principle**. It was field-useful inside CTO, but its separate localization limitations (bounded correction fence, dependence on declaration, and peer/radio exposure) were real. It should not be romanticized as a finished general navigator.

## Evidence categories

| Category | Meaning used here |
|---|---|
| Field-proven | Actual Otto or Toby operation with contemporaneous logs/field records. |
| Replay-validated | Recorded data replayed through a stated model or firmware path; not evidence of real-time operation by itself. |
| Proposed | Design, simulation, compilation, or tests without adequate live-locomotive evidence. |

## Chronology and findings

| Period / model | Problem and architecture | Evidence and result | Failure classification | Recoverability |
|---|---|---|---|---|
| **DNA map, June–July** | Use an ordered N/S magnet-polarity sequence and rolling window to acquire location on the 171-marker loop; later add timing, confidence and actual measured spacing. | **Field-proven in bounded stretches.** The archive records clean map agreement (e.g. Hans MM064–106; later Otto MM060–82 locked). It also records that the physical map was incomplete/mismatched outside converted territory and that sensor height could cause missed alternates. | Mostly physical-map, sensor, direction-reset, and acceptance-policy problems—not a demonstration that a correct complete DNA map could not work. Full-loop reliability was never established before the architecture moved on. | DNA firmware v2.0/v2.1 lineage and discussions are preserved; v2.0 was the last stated stable flash. Exact later images are not established. |
| **TEMPLATES / early acquisition experiments, late July** | Target/word matching and timing databases; recover from uncertainty by comparing a sequence rather than a single pulse. | Mixed. One experiment was explicitly marked failed by its own rule; individual incidents succeeded. | Reversal blindness and insufficient ambiguity handling. | Git commits include `3ac37f33`, `885ec569`, `db9bb54c`; source retained. |
| **QUORUM / SOLONAV, Aug** | Several position hypotheses compete inside a bounded fence; polarity/map observations vote; uncertainty is quarantined rather than guessed. CTO layers add peer-to-peer spacing and station logic. | **Field-demonstrated, not accepted.** On 15 Aug: Toby 795 agreed / 0 disagreed, Otto 779 agreed / 13 disagreed in the reported sample; bubble behavior worked. On 19 Aug: four Circuit Express cycles completed with station service and role swaps. These are partial successes, not proof of safe sustained separation. | CTO2 rear ends came from clearance arithmetic acting on leader position asserted with unjustified certainty. CTO3/QUORUM improved role, freshness, and stop logic but retained marker-space bounds without a validated uncertainty term; its ramp had no remaining-distance/closing-speed proof. The operator's later report of repeated rear ends is controlling evidence that safety was not demonstrated solved. | Strongly recoverable: `QUORUM` commits and field records; representative field commits `31c4898`, `ca27f1c`. |
| **NAVI_ONE 0.1–X21, late Aug–Sep** | Single next-marker navigation: Hall waveform detection/recognition, expected polarity and map progression; later shape/baseline gates and station stops. Development philosophy deliberately stopped on disagreement (“tough love”). | Field runs showed that basic progression and station mechanics could work, especially Toby. But archive analysis counts **115 wrong-magnet stops in 71 sessions** (29 Aug–16 Sep), all polarity strikes; `CONTRADICTED` never fired. | Core architectural issue: one surprising observation could withdraw position and stop the train. Later waveform/baseline/closure fixes made the detector increasingly stateful and vulnerable to stops over magnets, stalls, lurches, and baseline latches. Many failures were created or amplified by these later gates, not by simple polarity mapping alone. | Very recoverable: X18 `242109e`, X19 `227c330`/field source `16f3e7e`, X20 `75cb888`, X21 `c255700`, plus field-finding documents. |
| **NAVI_SIMPLIFIED, Sep** | Strip NAVI_ONE back to instantaneous two-sample detection, hard physical protection, then transparent multi-observation judgment; explicitly separate detection from navigation authority. | **Not validated; one decisive failed field run.** Otto went one physical magnet then stopped with “unresolved after 10 further magnets.” | Implementation defect, not a fair architecture verdict: repeated openings from the same magnet were admitted, protection was disabled once position became unknown, and rereads falsely consumed the ten-observation budget. Complete event trace is missing, so the exact field chain remains an informed reconstruction. | Source/build artifacts survive but were untracked; exact flashed equivalence is not proved. See `docs/NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md`. |
| **EWO 0.1–0.5, late Sep–mid Oct** | “Eyes Wide Open”: preserve current position/direction and combine Hall and IR/distance evidence; geographic station overlay rather than legacy procedural state. | **Field-proven in important but qualified runs.** A Toby automatic run from about 18:41–19:53 maintained reported position, including a Grillers stop during a documented Hall-loss episode. Otto later made 57 station stops while many advances were classified missed magnets. | The apparent Hall resilience was not clean proof of an independent IR correction mechanism: forensic review notes `ir_inferred_added=0` during Toby’s 20 miss events and identifies `position_reliable` as too coarse. Otto’s MM053 failure was a reboot due to station-event argument order, fixed in `4721b64`, rather than a NAVI-commanded stop. EWO 0.5 then had a startup-memory failure and rollback. | Recoverable through `codex/ewo-pwm-zero-localization`, especially `dd495ab`, `4721b64`, and field records. Exact flashed state must be matched to logs. |
| **RVM shape manual, 18–19 Oct** | 5 kHz Hall → 20 ms tracing; multi-scale statistical waveform classifier against learned magnet/non-magnet shapes; next-map-polarity progression. Initially a manual navigation observer. | **Best later field localization evidence.** Otto’s 19 Oct field record has a 169/169 full CW lap; another exact lap recorded 171/171. The continuity build later yielded 272/272 claims MM024–124 with four long stops preserved and no reboot; slow crawl reported 86/86. | Core caveats: single wrong polarity caused CONTRADICTION with no recovery; initial shape/manual build had a backlog/watchdog issue later addressed. Run 12 showed a 12-ADC false ensemble; fixed by a 70-count gate. These are real bounds, but the full-lap/long-stop evidence is direct. | Highly recoverable. Shape build `3a8953c`; continuity implementation `8986c5f`; field evidence and replays under `analysis/navi_rvm_shape_20261018/`. |
| **RVM AUTO stations + bubble r1–r5.1, 20–21 Oct** | RVM recognizer plus EWO-style station carpet and peer bubble. r1–r5 progressively changed pause, continuity, roles, leader/follower behavior and declarations. | Some real stops and peer behavior occurred, but it remains **experimental**, not field accepted. r2 had early successful stops; r4 produced an Arches collision when continuity froze Otto’s position and follower creep let Toby approach. r5.1 flashed to both locomotives, then live contradictions at MM038 and MM049 occurred. | Predominantly subsequent-integration failures: Toby stopped over a magnet (MM064 contradiction); r2 released follower too early; r4 creep plus frozen position caused the collision; r5 reference feedback withheld moving baseline and led to contradictions. The model accumulated tightly coupled corrections. | Source freeze `5b7a518` (local, unpushed) is recoverable. Flashed binaries are not byte-identical reproducible because build timestamp changes; flash records identify the source with high, not absolute, confidence. |
| **Oct 16 whole-tracing aligner** | Different model: dynamic-programming/Viterbi alignment of entire Hall “landscape” to a whole-track template; fixed-lag revision rather than irreversible per-magnet decisions. | **Replay-validated/proposed only.** It was not flashed or run as real-time locomotive firmware. Its inputs/templates are Otto-specific and do not cover Toby or later Hall shifts. | Not a field failure; it has insufficient operational validation. It cannot be ranked above field builds from retrospective alignment. | Frozen version recovered at `analysis/navi_rvm_replay_20261016_frozen/aligner.py`; templates/speed models intact outside git. |
| **CPR, 19 Oct** | Isolated causal/replay prototype comparing components and thresholds. | **Replay-validated only; negative field evidence explicitly preserved.** No live Otto/Toby implementation found. | Not an operating candidate. | `f9cc9493`, `f8476e97`, `eeec67a2`, `fca9e5b8`; branch `research/navi-cpr-0-1-20261019`. |

## Performance ranking by demonstrated operation

This is not a sophistication ranking.

| Rank | Candidate | Evidence class | Why it ranks here | What stops it from being the answer |
|---:|---|---|---|---|
| 1 | **RVM manual/continuity (`8986c5f` lineage)** | Field-proven plus parity replays | Full lap 171/171, 272/272 long-stop run, 86/86 slow run; preserves stalled/paused magnet encounters better than later NAVI_ONE gates. | Still single-strike/no recovery; field envelope incomplete (Toby, temperatures, CCW and slower crawl). No autonomous station/bubble acceptance. |
| 2 | DNA map v2.0-era direction/timing work | Field-proven only in portions | Correct ordered polarity/map matching and concrete recovery intent; valuable foundation. | Full physical map mismatch and incomplete full-loop field proof. Later claims of map correctness cannot substitute for whole-loop real-time operation. |
| 3 | EWO 0.1–0.4 | Field-proven but ambiguous | Long Toby operating period and many Otto stops show useful station/continuity behavior. | “Reliable” flag was not calibrated confidence; Hall/IR causal account is unclear, and regressions/reboot/memory failures prevented acceptance. |
| 4 | CTO/QUORUM 1.16R + Circuit Express | Field-demonstrated but safety-rejected | Strong episodes of bubble/CE/station behavior and useful uncertainty handling. | Rear-end safety was not established. Historic CTO2 used confidently wrong position; later implementation still lacked validated position envelopes and a braking-distance guarantee, and the operator reports repeated rear ends. |
| 5 | NAVI_ONE | Field-proven partial | Useful direct Hall/map progression and rich diagnostics. | 115 recorded false polarity stops; its deliberate immediate-strike development policy made ordinary transients operationally fatal. |
| 6 | NAVI_SIMPLIFIED | Field failure / proposed architecture | Its separation of detection, protection, and judgment is conceptually sound. | The only live run failed after one magnet due to same-magnet rereads; no valid operational verdict exists. |
| — | Whole-tracing aligner, CPR | Replay-validated / proposed | Interesting alternatives, particularly the aligner’s ability to revise earlier path hypotheses. | No live locomotive proof; do not equate retrospective fit with navigation. |

## Architecture versus later modification

### Failures inherent or fundamental to the candidate

- **QUORUM/CTO:** a bounded candidate fence cannot self-correct an operator declaration outside that fence while the locomotive is stationary. That is an explicit design limit, not a later regression.
- **NAVI_ONE:** an irreversible one-strike polarity rule cannot distinguish a transient/bad observation from a true geographical contradiction. Its “tough love” was intentional as a development instrument, but it is unsuitable as an operating policy.
- **RVM core:** per-event polarity advancement is also irreversible; a false accepted ensemble or missed next ensemble becomes a contradiction without recovery.
- **DNA:** an incomplete or physically inconsistent polarity map cannot yield global recognition. No algorithmic refinement fixes an incorrect physical reference map.

### Failures introduced by subsequent fixes/integration

- NAVI_ONE’s baseline/shape/closure machinery created failure modes around station dwell, field tails, stalls and lurches that the simple map-polarity principle did not itself require.
- NAVI_SIMPLIFIED’s observed failure came from implementation interactions: rearm, loss of protection on unknown position, and recovery-word accounting—not from its stated three-layer architecture.
- EWO’s MM053 stop was a reboot caused by a station-event format/argument bug; EWO 0.5’s startup-memory failure also was not a navigation theorem.
- RVM AUTO r1–r5.1 shows the clearest corrective chain: each revision addressed a real defect, then added coupling. The r5 recognizer/continuity feedback loop was able to preserve a wrong reference and withhold the evidence that could correct it.

## What to preserve for recovery

| Purpose | Source / commit / branch |
|---|---|
| Proven CTO/QUORUM field baseline | `31c4898` (1.16R), `ca27f1c` (Circuit Express); field records `20260815_QUORUM_1_16R_BUBBLE_TEST.md`, `20260819_CE3_FIRST_CIRCUIT_EXPRESS.md`. |
| Exact NAVI_ONE field archaeology | `242109e` (X18), `16f3e7e` (X19 field source), `75cb888` (X20), `c255700` (X21); branch `agent/toby-1-13-flash`. |
| SIMPLIFIED postmortem and retained artifacts | `docs/NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md`; source/build artifact hashes listed there. Exact flashed image remains uncertain. |
| EWO field/rollback line | Branch `codex/ewo-pwm-zero-localization`; `dd495ab`, `4721b64`, `44d02754`, `5c80fdd6`. |
| RVM manual/continuity core | `3a8953c` (shape manual), `8986c5f` (continuity manual), `8f343b0` (Toby continuity); field commits `1755f4b`, `fdeaf4c`, `9651363`. |
| RVM AUTO frozen operational state | `5b7a518` on local `agent/navi-rvm-gated-peer-20261020`; flash record `387d501`. Preserve before any normal cleanup; it is local/unpushed. |
| Whole-tracing alternative | `analysis/navi_rvm_replay_20261016_frozen/aligner.py`; frozen package and external hashed templates. |
| CPR research | `f9cc9493` → `fca9e5b8`, `research/navi-cpr-0-1-20261019`. |

## Recommendation

1. **Do not describe the present RVM AUTO/peer build as reliable navigation.** The 21 October contradictions follow a documented chain of corrective changes, and the source itself labels the class experimental/not field accepted.
2. **Use CTO/QUORUM as partial operational evidence, not the safety benchmark.** Its coordination and station-service episodes are valuable, but rear-end prevention depends on trustworthy bounded position and braking authority. Neither was field-proven adequate; the repeated-rear-end observation overrides claims based on partial log metrics.
3. **If localization work is resumed in a future, separately authorized effort, begin from the recoverable RVM manual/continuity core—not r5.1’s integrated AUTO stack.** First reproduce the already-demonstrated single-locomotive tests on both Otto and Toby: full-lap progression, ordinary stop/restart, slow crawl, reversal, missed/false magnet handling, and a defined recovery policy. Do not add stations/peer logic until that core is independently accepted.
4. **Treat the whole-tracing aligner as a research comparator, not a replacement claim.** It may address the irreversible-event weakness, but it has no live proof and is calibrated to Otto-specific historical conditions.
5. **Preserve before changing.** The commit/source references above are the recovery map. No firmware, branch, or archive was modified for this investigation.

## Source coverage and limitations

Examined directly:

- Local repository at `/Users/davidbrown/esp-loco-control`, including local and remote branch refs, commits, field records, RVM recordings/replays, EWO/NAVI/CTO documents, and the current local freeze.
- Processed local ChatGPT discussion collections, including original conversation passages in the navigation and implementation chapters rather than only their chapter summaries.
- Local preserved evidence tree and Otto telemetry inventory under the Codex Documents archive.

Not established as directly examined:

- A raw, complete ChatGPT export outside the processed local collections; the available collections preserve passages and provenance but may omit unrelated/export-unselected conversations.
- A mounted Arduino-IDE/archive directory or any external disk beyond the locally available archives.
- Byte-for-byte readback of flashed locomotive images. Several build directories were overwritten, and ESP32 builds embed time-dependent bytes.
- Fresh remote GitHub network state. The local clone contained the relevant branches and commit objects available at the time of review; no repository mutation or fetch was performed.

### Principal primary records

- Local Git field records and commits cited above.
- `NGR_Discussion_Collection_2026-10-17` and `NGR_Reference_Archive_2026-10-17`, especially navigation and implementation chapters.
- `docs/NAVI_RVM_MODEL_INVENTORY_AND_EVALUATION_PLAN_20261021.md` at `6f000d0b`.
