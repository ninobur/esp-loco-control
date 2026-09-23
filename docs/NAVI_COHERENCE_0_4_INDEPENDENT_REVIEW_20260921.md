# NAVI_COHERENCE 0.4 — independent review

Independent review of `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4_patch.zip`,
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
`programs/{NAVI_COHERENCE_0_4, NAVI_SIMPLIFIED, NAVI_ONE_X22}` assembled
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

### Correction (2026-09-22): the build-size gap, now root-caused

The 0.3 review reported `1,000,539 B` (76%) flash for its clean build. This
review originally rebuilt 0.4 to `1,215,216 B` (92%) and, having also
rebuilt 0.3 itself to the same ~92% figure under identical conditions,
guessed the ~215 KB gap from 0.3's own historical number was unconfirmed
"build-environment drift." That guess was wrong, and it's worth correcting
precisely rather than leaving it stand.

The actual cause: that verification build had `test_coherence.cpp` sitting
flat in the sketch folder next to the `.ino` — exactly as the patch zip
ships it. Arduino's build system compiles every `.cpp` file it finds
directly in a sketch folder as part of that sketch, not just the `.ino`.
`test_coherence.cpp` does `#include <iostream>` and defines its own
`int main()`; all of that was being compiled into, and linked into, the
ESP32 binary alongside the real firmware. That's where the missing ~215 KB
went.

This was caught when placing the sketch for real at
`../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4/` (see below) with the host
test moved into a `tests/` subdirectory — matching `NAVI_IR`'s own
convention, which keeps its test file out of the sketch folder for exactly
this reason. That rebuild lands at `1,000,571 B` (76%), 32 bytes off the 0.3
review's original number — exactly the two added string literals, nothing
more. So 0.3's number was right all along; the anomaly was self-inflicted by
this review's own verification-tree layout, not anything in the 0.4 diff.

**Consequence for anyone building the patch zip as-shipped**: compiling
`NAVI_COHERENCE_0_4_patch.zip`'s contents directly, without moving
`test_coherence.cpp` out of the sketch folder first, silently bloats the
flashed binary by ~215 KB (16% of the partition) with unreachable host-test
and `iostream` code. Not a correctness bug — nothing in `test_coherence.cpp`
executes on the ESP32 — but worth knowing before treating flash-size as a
signal, and worth fixing in how future patches are packaged.

## Part 3 — items not touched by this patch, carried forward

- **`test_coherence.cpp` still prints "NAVI_COHERENCE_0_3 ..." on pass.** The
  file is byte-identical to 0.3's (confirmed above), so this isn't a new gap
  from 0.4, and the test doesn't assert on `SKETCH_NAME` — it's a cosmetic
  label mismatch a future patch could clean up, not a correctness issue.
- **"Manual only for the first field run"** is a verification-checklist item
  aimed at whoever conducts the flash/field test, not a property of the
  code. Nothing in this review substitutes for it.

## Update (2026-09-22) — placed for real, ready to flash

The build-placement question this review originally deferred is resolved:
0.4 is now placed at
[`../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4/`](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4/),
alongside `NAVI_IR`, `NAVI_SIMPLIFIED`, and `NAVI_ONE_X22` — the same real
siblings its shared-dependency includes already pointed at. Two differences
from a plain copy of the patch zip:

1. `RouteMap.h`, `MovementEvidence.h`, `Ops.h`, `Stations.h`, `HallObserver.h`,
   `LocoConfig.h` are vendored in from `programs/NAVI_IR/` (the patch's
   own README says these are shared and not to duplicate them; this is that
   placement, not a new copy of NAVI_COHERENCE's own files).
2. `test_coherence.cpp` moved into a `tests/` subdirectory, for the reason
   in the correction above — compiled and passing there with
   `g++ -I.. tests/test_coherence.cpp`, unchanged in content.

Compiled in place — no assembled verification copy, no template
substitution — using the real `../../QUORUM/credentials.h` that already
exists on disk: clean, 0 warnings from NAVI_COHERENCE, `1,000,571 B` (76%)
flash. This is the artifact to flash, not a review copy of it.
`firmware/README.md`'s catalog now carries this row; the open item about a
missing catalog entry is closed.

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

- [Ready-to-flash sketch](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4/) (placed 2026-09-22; see the update above)
- [Patch README](../firmware/reference/NAVI_COHERENCE/README_0_4.md)
- [0.3 independent review (the bug this patch fixes)](NAVI_COHERENCE_0_3_INDEPENDENT_REVIEW_20260921.md)
- [0.2 independent review](NAVI_COHERENCE_0_2_INDEPENDENT_REVIEW_20260921.md)
- [Development history and governing design](NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md)
