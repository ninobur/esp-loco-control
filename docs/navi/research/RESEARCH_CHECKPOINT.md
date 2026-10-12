# NAVI historical research checkpoint

Status: IN PROGRESS — not an exhaustive-completion claim.

Snapshot: 2026-10-12. Mandatory inclusion begins 2026-09-19T00:00:00Z (deliberately includes the local-date boundary). 66 accessible branch heads were enumerated. Their dated histories produced 300 unique commits. Parent closure was checked; the two external boundary parents date to August 11 and September 18. The API returned a full 66-branch page and the branch-search continuation was empty. Deleted/unreachable branches and uncommitted machine files cannot be certified.

Research branch: docs/navi-engineering-continuity-20261012. Starting firmware snapshot: codex/navi-ewo-0-3-stop-integration at 4ad1df225f3768c192330c634b53d114faabf894. This selection preserves the latest accessible 0.3 corrections; it does not assert field acceptance or overwrite other branch authority.

## Reviewed so far

Root AGENTS.md (037dc755), docs/AGENTS.md (9fb7112d), docs/CLAUDE.md (08f9acc6); three supplied philosophy/overlay text attachments. Prior failure-inventory commit 2580917 was read as a retrospective research input, not independently established proof.

## Remaining

Retrieve per-commit changed-file metadata and all distinct in-scope revisions. Establish a content-identity source ledger, including deletions/renames, supporting modules and configuration. Substantively read each source; fetched/inventoried is not reviewed. Follow earlier CTO/NAVI foundational references. Reconcile approved decisions with actual code and field evidence. Only then finish the three principal documents and completion report.

## Significant provisional findings

Main is dated September 25 and lacks the later development record. Latest operational-branch code and latest governing documentation occur on separate branches. The October 11 failure retrospective explicitly disclaims complete independent verification. No current field-accepted build has yet been established.

## Exact next steps

1. Use branches.json and commits.json in this folder as immutable snapshot inputs.
2. Fetch /repos/ninobur/esp-loco-control/commits/{sha} for each commit (paginate files where needed); retain metadata, file blob IDs, patches, parents and dates.
3. Build the path/blob revision inventory. Include previous content for deletions and necessary earlier baselines; fetch substantive content and track review notes separately.
4. Update this checkpoint and commit it periodically; do not mark any item reviewed without inspecting its logic/content.

No firmware changes, new design decisions, deployment, or acceptance are authorized by this research. Source additions from later transcripts must preserve approval provenance and contradictions.
