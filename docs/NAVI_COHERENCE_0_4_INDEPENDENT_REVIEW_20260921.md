# NAVI_COHERENCE 0.4 — independent review

Independent review of `firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_4_patch.zip`,
which packages the fix the
[0.3 review](NAVI_COHERENCE_0_3_INDEPENDENT_REVIEW_20260921.md) found and
deferred: the inherited `snprintf` argument/specifier mismatch in
`publishNav()` and `publishDecision()`. Desk review plus independent build
and host verification; nothing flashed, no hardware access.

## Bottom line, up front

The fix is correct, minimal, and complete. Diff confirms 0.4 touches exactly
three things versus 0.3 — the header comment, the `SKETCH_NAME` macro, and
one added `"evidence":"%s"` field in each of the two affected format
strings — and nothing else in the four other source files. Rebuilding clean
with full warnings now produces **zero** `-Wformat` warnings (down from the
15 the 0.3 review found), and a standalone reproduction of the fixed format
strings using the exact argument values that segfaulted 0.3 completes
without incident. This clears the pre-flash blocker the 0.3 review raised.

## Part 1 — the fix, verified by diff

`NAVI_COHERENCE_0_3.ino` vs the patch's `NAVI_COHERENCE_0_4.ino`: three
hunks, all matching the README exactly.

| Claimed change | Verified in diff |
|---|---|
| `publishNav()` gains `"evidence":"%s"` immediately after `"ruling":"%s"` | Yes, line 332 |
| `publishDecision()` gains `"evidence":"%s"` immediately after `"ruling":"%s"` | Yes, line 365 |
| Sketch identity advances to `NAVI_COHERENCE_0_4` | Yes — header comment and `SKETCH_NAME` macro |

`Navigator.h`, `RecoveryControl.h`, and `test_coherence.cpp` are
byte-identical to 0.3. No argument lists changed — only the format strings
gained the one field each was missing. This matches the README's claim of
zero navigation-logic change, and matches the 0.3 review's own prescribed
fix ("add one field... immediately after `ruling`... nothing else needs to
change") exactly.

### Hand-traced specifier alignment

Recounting both functions' format strings against their argument lists
specifier-by-specifier (the same exercise the 0.3 review did to find the
bug, run in reverse to confirm the repair):

- `publishNav()`: 21 specifiers, 21 arguments, all types now match in
  order — including the `evidenceClassName(s.evidence)` argument, which
  previously had no specifier of its own and shifted everything after it.
- `publishDecision()`: 22 specifiers, 22 arguments, all types now match in
  order — including the two the 0.3 review identified as landing on `%s`:
  `unresolvedCount` (now correctly on `%u` → `ambiguity_count`) and
  `point.alignmentUs` (now correctly on `%lld` → `ir_alignment_us`).

### Swept for the same bug elsewhere

The 0.3 review pinned this to one insertion point in two functions. Rather
than take "just these two" on faith, I counted specifiers against arguments
for every other `snprintf`/`Serial.printf` call site in the `.ino` (11 more:
`requestPwm`'s cap warning, `topic()`, `warn()`, the `nav/ir_compare`
publisher, `serviceIr()`, both `serviceStatus()` payloads, the boot record,
the station-event publisher, and the Hall-acquisition diagnostic). All 11
have matching counts and matching types. The bug was exactly where the
README says it was, nowhere else.

## Part 2 — build verification (clean, not cached, with real dependencies)

Same assembled-dependency approach as the 0.2/0.3 reviews, replicating the
actual relative-include layout (not flattened) so `LocoConfig.h` and
`HallObserver.h` resolve their `../NAVI_SIMPLIFIED/` and `../NAVI_ONE_X22/`
includes correctly: `common/`, `QUORUM/credentials.h`, and
`test-programs/{NAVI_COHERENCE_0_4, NAVI_SIMPLIFIED, NAVI_ONE_X22}` assembled
under one root, verification copy only, not part of the repo.

| Check | Result |
|---|---|
| `arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --clean` | **PASS**, exit 0. **Zero** warnings from NAVI_COHERENCE (down from the 0.3 review's 15 `-Wformat`/`-Wformat-extra-args`); only the same 3 pre-existing, unrelated `-Wdeprecated-enum-enum-conversion` warnings from the vendored Adafruit_INA219 library |
| `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` on `test_coherence.cpp` | **PASS**, 0 warnings, ASan/UBSan clean, all assertions pass (unchanged file — still prints "NAVI_COHERENCE_0_3 focused checks passed"; cosmetic only, see Part 3) |
| Standalone repro of both fixed format strings, exact argument types, and the **same dangerous stand-in values that crashed 0.3** (`unresolvedCount=0`, `alignmentUs=123456789` / `0x75bcd15`) | **PASS** — compiles clean under `-Wformat`, runs clean under ASan/UBSan, both JSON payloads well-formed with `"evidence"` correctly positioned immediately after `"ruling"` in each |

The third row is the direct rebuttal of the 0.3 finding: the exact value
that produced `AddressSanitizer: SEGV on unknown address 0x0000075bcd15` in
the 0.3 repro now prints as `"ir_alignment_us":123456789`, an ordinary
integer field, because it lands on the `%lld` specifier it was always meant
for.

### A build-size observation, not attributable to 0.4

The 0.3 review reported `1,000,539 B` (76%) flash for its clean build.
Rebuilding 0.4 the same way here gives `1,215,216 B` (92%) — a large enough
gap to be worth chasing before trusting it. Rebuilding 0.3 itself, same
session, same assembled dependencies, same command: `1,215,184 B` (92%),
i.e. 0.3 and 0.4 are size-identical modulo the 32 bytes the two added
`"evidence":"%s",` string literals account for. So the size delta versus the
historical 0.3 report is a build-environment difference between that
session and this one (library or core patch drift over the same day is the
likely candidate, not confirmed), not something 0.4 introduces. Flagging it
rather than dropping it, per this file's own precedent of correcting cached
build claims — not mine to root-cause further here.

## Part 3 — items not touched by this patch, carried forward

- **`test_coherence.cpp` still prints "NAVI_COHERENCE_0_3 ..." on pass.** The
  file is byte-identical to 0.3's (confirmed above), so this isn't a new gap
  from 0.4, and the test doesn't assert on `SKETCH_NAME` — it's a cosmetic
  label mismatch a future patch could clean up, not a correctness issue.
- **`firmware/README.md` still has no NAVI_COHERENCE catalog entry.** Same
  open item both the 0.2 and 0.3 reviews flagged; 0.4 doesn't add or resolve
  it. `firmware/NAVI_COHERENCE/` remains untracked in git.
- **The build-placement question** (`firmware/NAVI_COHERENCE/` vs
  `firmware/test-programs/NAVI_COHERENCE_0_4/`, needed for the vendored
  headers' relative includes to resolve for a real flash, as opposed to this
  review's temporary assembled copy) is still explicitly deferred by the
  README rather than resolved. Not new to 0.4; not mine to decide.
- **"Manual only for the first field run"** is a verification-checklist item
  aimed at whoever conducts the flash/field test, not a property of the
  code. Nothing in this review substitutes for it.

## Bottom line

0.4 does exactly what it says: a two-line, surgical fix to the exact bug the
0.3 review found, verified here independently by diff, by a clean rebuild
with full warnings (0 remaining, down from 15), by the unchanged host test
suite, and by reproducing the fixed format strings standalone with the same
argument values that crashed 0.3. No other instance of the bug exists
elsewhere in the file. No navigation logic changed. On its own stated
terms — a telemetry-format correction, not a design change — 0.4 is ready
for the clean-build-plus-Manual-field-test path its own README lays out.

## References

- [Patch README](../firmware/NAVI_COHERENCE/README_0_4.md)
- [0.3 independent review (the bug this patch fixes)](NAVI_COHERENCE_0_3_INDEPENDENT_REVIEW_20260921.md)
- [0.2 independent review](NAVI_COHERENCE_0_2_INDEPENDENT_REVIEW_20260921.md)
- [Development history and governing design](NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md)
