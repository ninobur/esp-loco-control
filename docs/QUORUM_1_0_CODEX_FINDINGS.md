# CODEX findings on QUORUM 1.0

**CODEX implementation review of commit fa59756 (QUORUM 1.0). Findings 1-5
addressed in QUORUM 1.1; finding 6 resolved as documented deviation.**

---

I can now assess all eight ambiguity resolutions. The core QUORUM implementation remains sound, but I contest two resolutions and partially contest a third. Four source defects still block field testing.
Ambiguity resolutions

1. SKETCH_NAME — ratified, superseded. `QUORUM_1_0` is now consistent in the source and specification. No remaining issue.
2. Retaining dead DNA recovery code — contested as specification fidelity. The code is harmless because it is unreferenced, but Revision 20 explicitly orders `dnaBuf`, `dnaPush()`, `dnaMatch()`, `DNA_W`, and the reacquisition constants removed.
If a separate operator instruction explicitly required retaining them, that instruction legitimately overrides the spec—but this must be recorded as an intentional deviation. It cannot simultaneously support the report's "zero claimed deviations."
3. Replacing `conf_max`/`dna_w` in boot identity — ratified. Reporting deleted machinery would be misleading. Publishing the QUORUM constants is the correct minimal integration change.
4. Publishing both `state` and `nav_state` — ratified. Preserving the old field while adding the specified field is a reasonable compatibility choice. Both use `navStateName()`, so they cannot disagree.
5. `QUORUM_OPEN` / `QUORUM_CLOSED` names — ratified. The names are clear, consistent with the existing event vocabulary, and preserve incident boundaries in replay.
6. Keeping AGREE/DISAGREE on `pub()` — contested. Although the §5.1 table names adoption and incident events specifically, its general rule is:
One-time event → `pubMarker()`. Current-value state → `pub()`.
AGREE and DISAGREE are one-time observations, not current-value state. In the implementation they go through `navPublishState()` and the drop-oldest status queue at QUORUM.ino:1705. They can therefore be evicted while their corresponding marker and QUORUM events survive.
Either:
   * route event-bearing AGREE/DISAGREE payloads through `pubMarker()`, while keeping short current-state publications on `pub()`; or
   * explicitly declare AGREE/DISAGREE derivable from the marker stream and remove them as independent decision events.
I recommend the first.
7. Replacing loopstat `conf` with `miss_streak` — ratified. This follows the specification and avoids resurrecting `navConfidence`.
8. Alert payload changes — semantic choice ratified, implementation contested. Keeping `candidate_mm` as the leader's implied position and adding `viable` is sensible. However, the resulting alert can exceed the 512-byte queued transport and truncate; see finding 4 below.

Remaining implementation findings

1. High — terminal snapshot encodes leader and runner-up indices, not offsets. QUORUM.ino:759 passes `leaderIdx` and `runnerUpIdx` directly into `ld` and `ru`. These are indices `0..5`; the required values are offsets `-1,0,+1,+2,+3,+4`.
Decision events correctly convert through `QUORUM_OFFSETS[]`; the terminal snapshot must do the same.
2. High — the retained-state snapshot buffer crosses cores without synchronization. `loop()` writes the 512-byte `noQuorumSnapshot`; `networkTask()` reads it during `mqtt.publish()`. A volatile one-byte state flag does not protect the buffer or establish a safe C++ handoff.
Copy the snapshot under a mutex/critical section into a network-task-local buffer, release the protection, then publish. Do not hold a lock during the network call.
3. High — the retained state is forgotten after one successful send. At line 2297, success changes the state to `DRS_NONE`. If the broker later restarts while the locomotive remains `NAV_NO_QUORUM`, reconnect does not resend the snapshot.
Separate the persistent desired state (`SNAPSHOT` or `CLEAR`) from a per-connection "needs reconciliation" flag. Re-arm reconciliation after every MQTT reconnect.
Also decide explicitly what happens when `NO_QUORUM` occurs offline and the operator redeclares before reconnect: the present single slot replaces the undelivered snapshot with CLEAR, permanently losing the forensic record.
4. High — retained alert JSON can truncate silently. `publishAlert()` builds up to 640 bytes at line 1785, then `pub()` copies it into `PubMsg::payload[512]`. A realistic worst-case `NO_QUORUM` alert exceeds 512 bytes.
Compact the alert below 512 bytes and check `snprintf()`'s return, or introduce a suitable transport container. Do not enqueue truncated JSON.
5. Medium — AGREE/DISAGREE use the evictable queue. This follows directly from contested resolution 6. It does not affect navigation, but it breaks the goal of a complete, reconstructable decision record under congestion.
6. Low — dead recovery code remains. No runtime effect, but it should either be removed or documented formally as an approved deviation from §6.2.

Certified navigator properties
I confirm the report's claim for the actual control logic:

* State transitions and terminal exit behavior are correct.
* Accepted-event odometry is correct in all post-declaration states.
* Directional offset and wrap arithmetic are correct.
* Scoring is plain match counting against each entry's recorded coordinate.
* The evidence ring and `evalCount` accounting are correct.
* Adoption precedes the event-12 hard bound.
* Validation has exclusive comparison ownership.
* Adoption finalizes on the first agreement.
* Failed-adoption correction removal and ring rebasing are correct.
* Exclusions and incident failure count survive reopening correctly.
* Successful and terminal incident endings are distinct.
* Timing-gate priority and event-time PWM capture are correct.
* Zero-`dt` and `NO_PREV` bootstrapping are correct.
* Rejected events do not poison the timing predecessor.
* Every accepted ACTIVE event replaces the predecessor.
* Conservation interval indexing and units are correct.
* `NAV_NO_QUORUM` stops once, resets station state, retains diagnostics, and continues deceleration odometry.
* Both declaration paths recover through `navDeclare()`.
* Direction changes perform the required recovery reset.
* Stations remain active during `NAV_EVALUATING`.

Verdict: the QUORUM navigator itself is ready; the complete firmware is not yet ready for field verification because terminal evidence can be mislabeled, torn, forgotten after reconnect, or truncated. Fix findings 1–4, resolve AGREE/DISAGREE routing, then proceed to the §8 field/replay tests.
No files were changed, no credentials were displayed, and I did not run git or compile.
