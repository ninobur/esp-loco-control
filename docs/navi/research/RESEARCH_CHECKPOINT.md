# NAVI engineering history — interruption recovery checkpoint

**Updated:** 2026-10-12. **Assignment status: INCOMPLETE. Phase I remains in progress; Phase II is not complete.** No exhaustive-review, engineering-handover, field-readiness or final-document claim is made.

Research branch: `docs/navi-engineering-continuity-20261012`. First durable checkpoint: `d94e2cce359a9d294cf03499335ef9bec8367ab2`. Starting tree is preserved from the latest accessible STOP-correction branch, `codex/navi-ewo-0-3-stop-integration` at `4ad1df225f3768c192330c634b53d114faabf894`. This is a documentation base, not an assertion that this candidate is approved, field accepted or includes its sibling IR-review changes.

## What has been established

All 66 initially accessible branch heads were enumerated. Branch-search continuation was empty; the REST page also contained 66. Dated histories were deduplicated into 300 unique commits. The apparent 100-commit pagination limits were reconciled by a second history page and checking all known parent edges: the only external boundary parents were `31d877abc5224f596dd7c381e551a613e9bd36be` (August 11) and `e5b586c5e7bd88fa17dfff088bed9bcd74f6a933` (September 18).

The inclusion query begins September 19 at 00:00 UTC, deliberately **seven hours broader** than September 19 at midnight Los Angeles. Boundary records are retained rather than excluded prematurely. Dates come from Git, not inferred physical test dates. The current execution's own documentation commits are excluded from the historical denominator.

Per-commit changed-file metadata was collected for all 300 commits. The September 23 organization commit `51019746` has 130 changed files (pages 1–2); `d59a332b` has 628 (pages 1–7). Every page was retrieved through the final short page. Renames, removals, previous paths and content identities are retained. Deduplication is by full Git blob identity, never by a title or version name.

The reconciled **Git changed-content inventory** contains 1,653 distinct blobs across 1,371 paths. It includes 746 Markdown revisions and 115 sketch blobs as well as modules, configuration, tests, experimental tools and evidence. It is deliberately inclusive. Necessary pre-window source baselines, embedded archive contents and transitive dependencies are still to be reconciled into the complete mandatory source inventory; therefore this is **not yet a completed source-scope certification**.

54 repository content revisions have been substantively reviewed; one is partially reviewed and 1,598 remain pending. Full review of a report does not mean its primary logs were independently checked. There are additionally eight supplied original text inputs fully read and preserved under `input_sources/`; the other supplied attachments are not silently counted.

Content recovery from complete Git patches was accepted only when the reconstructed Git blob hash matched exactly. 1,128 revision contents were verified locally. Those scratch bytes may expire; immutable object IDs, occurrence commits and review notes are durable. Fetching or reconstructing content alone never marks it reviewed.

A final branch refresh contained the same 66 historical branch heads and only this research branch as a new 67th branch. No external head movement was detected at that refresh. Future execution must refresh again; this checkpoint does not certify later changes or inaccessible deleted/unreachable refs.

## Durable files and their roles

- [LEDGER_MANIFEST.json](LEDGER_MANIFEST.json): exact counts and the 17 ordered ledger parts.
- `ledger/000.json` through `ledger/016.json`: one entry per distinct blob, every known path/commit occurrence, review status and relevance/limitation notes. PENDING is not an exclusion.
- [branches.json](branches.json), [commits.json](commits.json): fixed branch heads, author/committer chronology and parents.
- [SOURCE_INDEX.md](SOURCE_INDEX.md): 54 fully reviewed sources with immutable links and preserved supplemental input provenance.
- [RESEARCH_FINDINGS.md](RESEARCH_FINDINGS.md): 15 source-grounded working findings, successes, failures, contradictions and classifications.
- [map_verification.json](map_verification.json), [verify_map.py](verify_map.py): this execution's bounded mathematical verification and its reproducible script.
- [validation_20261012.json](validation_20261012.json): precisely which tests were run here. Historical full-suite and build reports are not misrepresented as newly executed.

## Significant current findings

1. `main` is not the later NAVI project state. Source and documentary authority occupy different branches.
2. `c4cd2b2` and `4ad1df2` are siblings based on `253faa4`; STOP correction does not contain the time-required-query follow-up. Production query callers were distinguished from the problematic legacy overload.
3. The 0.4 audit's no-numbered-amendment claim is incomplete across branches: `cbdfe949` amended decisions on the 0.2 branch, but retained contradictory older bodies. Operator approval and scoped supersession still need reconciliation.
4. 0.2's amended 0122 says 10,000 µm/pulse; 0.3 preserves 9,652. This research does not choose or alter calibration.
5. Source inspection corroborates PROXIMAL_R1's reversal/reference circular hold. Successful IR distance windows can coexist with wrong identity.
6. The general physical no-missed-pulses requirement remains unresolved after the IR correction. Detector/telemetry integrity is not proof of complete physical counting.
7. The accepted-MM-to-STOP handoff defect and correction are directly traceable. Seven production-code host cases pass here; no railway stopping validation follows from them.
8. All cyclic ten-pole map words are unique **within each known direction**. This proves a map property, not complete observed-word identity or reachability.
9. The October 2 field verdict's 0120 interpretation conflicts with the actual intervention decision. Its reported empirical successes must be retained separately.

See finding IDs RH-001–RH-015 for evidence, qualification and successors. These findings are not new architecture.

## Exact resumption procedure

1. Read root `AGENTS.md`, this checkpoint, the ledger manifest, source index and working findings. Work as one agent unless David authorizes otherwise. This assignment authorizes research and documentation only.
2. Refresh all branches and tags and compare the saved heads; add subsequent accessible historical revisions and record the new cutoff. Consider reachable tags and PR-only/deleted-branch evidence explicitly; the current branch snapshot does not prove their absence. No repository maintenance, merge or history rewrite is authorized.
3. Parse all 17 ledger parts. Resume the one PARTIALLY_REVIEWED source (`docs/NAVI_DECISION_MODEL.md`, blob `bd710ed9434973bc75990513c3bc41e9d4ce48c4`) before calling it complete. Then review PENDING sources in commit/dependency order. Do not reread the 54 completed blobs except to resolve a new contradiction or inspect a new revision.
4. Retrieve immutable content via Git blob SHA (`GET /repos/ninobur/esp-loco-control/git/blobs/{sha}` or the connector's fetch-blob operation), or a non-removal occurrence commit/path. A removal occurrence requires the parent tree for file-at-path access. Check actual bytes/hash; incomplete API patches cannot establish substantive content. For code, read actual logic and transitive local includes; a full ancestor plus the **entire verified delta** may document a distinct revision review, with that method recorded.
5. Expand ZIP/TGZ/GZ/PAGES or other embedded source/evidence containers where substantive material exists. Assign each embedded source its container SHA, member path and SHA-256; deduplicate only byte-identical members. Current ledger container rows are not reviews of their contents. Complete unmodified supporting/configuration dependency closure across relevant branches.
6. Follow earlier CTO/NAVI references selectively: CTO2 roles and collision history, truths/DNA and reversing, QUORUM/X18–X22 acquisition and baseline evidence, NAVI_SIMPLIFIED prospectus/reviews, IR architecture 0.1–0.4 defects, station pause/restart history and original June/September evidence. The eight supplied text inputs are useful starting records; earlier firmware and two DOCX inputs remain to investigate.
7. Check reports against primary evidence. In particular: September 22 genuine-versus-rejected observations, September 23 global correction, all five 20Q3 runs (run 4 and earlier report revisions are pending), September 27–28 raw spatial/impostor studies, September 29–October 2 hardware/station trials, October 9–10 pulse fixtures and gate/anchor behavior, and 0.2/0.3 review assertions. Map math or synthetic host tests cannot replace missing field evidence.
8. Reconcile authority using original instructions/transcripts where actually available. Resolve or explicitly preserve contradictions from RH-002/003/014 and later calibration profiles; retain abandoned reasoning and empirical successes. No decision is rewritten or invented during history research.
9. Update per-blob review notes and counts after each substantive batch. Commit checkpoints periodically. Newly inaccessible sources must have the attempted retrieval and exact limitation recorded; do not relabel merely unreviewed sources as inaccessible.
10. Only after mandatory review and verification complete, produce `docs/navi/NAVI_ENGINEERING_CONTINUITY.md`, `NAVI_DECISION_HISTORY.md`, and `NAVI_EXPERIMENTAL_HISTORY.md`. Use these working findings as input, not a substitute. Verify every consequential reference, code transition, evidence status and current-state claim; commit documentation separately from firmware.

## Limitations and acceptance state

Direct shell Git clone was unavailable; authenticated GitHub connector reads and Git-object writes supplied repository access. That is not evidence that repository files themselves are inaccessible. Referenced private Pi/Mac recordings not preserved in Git were not accessed. Raw repository logs/fixtures and most archive contents remain unreviewed, not certified unavailable. Full native ChatGPT transcripts and the review summaries mentioned by the 0.4 audit were not supplied; only actual attached fragments and embedded quotations were examined.

The standing branch discipline and maintenance constraints were honored: no merge, reset, rebase, cleanup, firmware change, flash or live-service change. The documentation tree preserves the starting firmware tree.

**Handover assessment:** a successor can resume this research without reconstructing its inventory or losing verified findings. It cannot yet begin arbitrary NAVI engineering on the assumption that this historical investigation is complete. The remaining 1,599 unfinished content reviews, archive/dependency scope, primary evidence checks and authority contradictions can materially change the final history. The three required final documents have intentionally not been presented as complete while Phase I is unfinished.
