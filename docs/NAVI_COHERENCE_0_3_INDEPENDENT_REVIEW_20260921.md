# NAVI_COHERENCE 0.3 — independent review, and a correction to the 0.2 review

Independent review of `firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_3_patch.zip`,
found sitting untracked alongside 0.2 (appeared 2026-09-21 18:24, after the
0.2 review below it in the timeline). Desk review plus independent build
verification; nothing flashed, no hardware access.

## Bottom line, up front

0.3's own stated purpose — fix exactly the five things the 0.2 review
flagged, nothing else — is met precisely and correctly; verified by diff and
by rebuilding, not by trusting the changelog. But rebuilding 0.3 **clean**
(no build cache) surfaced a bug the 0.2 review missed entirely: a mismatched
`snprintf` argument count in two telemetry-publishing functions,
**inherited unchanged from 0.2**, that reproduces as an immediate crash on a
host build with the identical argument list. This is not something 0.3
introduced and 0.3 does not fix it either — it just wasn't looked at, by
either review, because the 0.2 build check that should have caught it was
compromised by `arduino-cli`'s object cache silently serving a stale
"no warnings" result. **This is the one finding that matters before any
flash of either 0.2 or 0.3.**

## Part 1 — the five 0.2 fixes, verified by diff

Diffed `Navigator.h`, `NAVI_COHERENCE_0_3.ino`, and `test_coherence.cpp`
against their 0.2 counterparts directly (`RecoveryControl.h` is
byte-identical). Every changelog item in the patch's own `README.md`
corresponds to a real, minimal, correctly-targeted line change:

| Claimed fix | Verified in diff |
|---|---|
| MQTT client ID `NAVI_IR_%s` → `NAVI_COHERENCE_%s` | Yes, line 566 |
| Boot banner no longer hardcodes "0.1" | Yes — and better than a string edit: it now prints `SKETCH_NAME`/`BUILD_CLASS` directly, so it cannot drift from the real build again |
| Missed-and-advance keeps the polarity check | Yes — new `EvidenceClass::MissedWithPolarityDiscrepancy`, and the previously-computed-but-discarded `pol` variable now selects between it and `MissedObservation` |
| Leftover "NAVI_IR" operator strings | Yes, both AUTO and GO refusal messages |
| `nav/ir_compare` shows candidates 0–10, not 0–2 | Yes, and correctly: Navigator's real search is 1–10 mapped markers ahead; the diagnostic's step 0 is an added zero-distance anchor reference, so all 10 real candidates are still covered |
| Sketch/folder naming | Yes — `NAVI_COHERENCE_0_3.ino` inside `NAVI_COHERENCE_0_3/` |

The new `test_coherence.cpp` case (`n2`/`f`/`g`) specifically constructs a
missed-marker-then-wrong-polarity scenario and asserts
`EvidenceClass::MissedWithPolarityDiscrepancy` — it isn't just asserting the
ruling stays `MissedAndAdvanced` (which it does; 0.3 does not add a new
`Ruling` for this case, only a new `EvidenceClass`, so the distinction lives
in `evidence`/telemetry rather than in `Ruling` — consistent with how
`NonLandmarkHall` already works and with the README's own wording, not a
gap). Host suite (`g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined`)
passes clean, all 7 assertions including the new one.

No regressions found in this part of the diff: nothing here loosens a check,
widens a window, or touches the AUTO gate.

## Part 2 — the bug both reviews missed: a `%s` reading a non-pointer

### How this was found

Re-running 0.3's ESP32 compile with `--warnings all` produced **zero**
output — matching what I'd reported for 0.2. That result was suspicious
given how much text those two functions actually contain, so I re-ran both
0.2's and 0.3's compiles with `arduino-cli compile --clean`. Both then
produced **15 identical warnings**, in the identical two functions,
identical to the line. `arduino-cli` had cached the object for these
byte-identical source files (0.2's `publishNav`/`publishDecision` are
untouched in 0.3) and simply didn't recompile them the second time, so
`--warnings all` silently checked a cached artifact instead of the source. I
did not think to pass `--clean` in the 0.2 review. This finding exists
because of the 0.3 rebuild, not because of anything 0.3 changed.

### The actual bug

`publishNav()` (line 330, the `mm/marker` topic — the primary MQTT channel
for every navigation event) and `publishDecision()` (line 362, the
`nav/discrepancy` topic — published on every single judged Hall event) each
have **one more `snprintf` argument than the format string has
specifiers.** Comparing against the corresponding functions in
`firmware/test-programs/NAVI_IR/NAVI_IR.ino` (internally consistent —
argument count matches specifier count in both) pins the exact origin: at
some point NAVI_COHERENCE's author inserted
`evidenceClassName(navigator.status().evidence)` into both argument lists
— a reasonable, on-purpose addition given evidence classification is
central to this sketch's whole design — but never added the matching
`"evidence":"%s",` field to either format string. Every argument after that
insertion point shifts one position relative to its intended specifier.

In `publishDecision`, the shift lands two arguments on `%s` specifiers that
were never string pointers:

- `"ir_reason":"%s"` receives `navigator.status().unresolvedCount` (a
  `uint8_t`, typically 0).
- `"action":"%s"` receives `point.alignmentUs` (an `int64_t` microsecond
  value, typically a large arbitrary number).

### Confirmed by reproduction, not just by the compiler

Reproduced standalone on the host, with the exact specifier string and the
exact argument types from `publishDecision()`:

```
==75671==ERROR: AddressSanitizer: SEGV on unknown address 0x0000075bcd15
    #0 ... internal_strlen(char const*)
    #1 ... vsnprintf
    #2 ... snprintf
    #3 ... main
```

`0x75bcd15` is exactly `123456789` in hex — the test's stand-in
`alignmentUs` value, read by `%s` as if it were a string pointer, and
`strlen()` faulting on the resulting bogus address. The `uint8_t` value
landing on the earlier `%s` (`unresolvedCount`, value 0 in the repro) did
*not* crash — this host libc special-cases a null pointer for `%s` and
prints `(null)`, silently masking half the bug. That is a property of this
particular libc, not a guarantee; ESP32/newlib's `vsnprintf` may or may not
extend the same courtesy, and it is not something to rely on either way. The
second case — an arbitrary, almost-certainly-unmapped 64-bit microsecond
value read as a pointer — is not the kind of value any common `%s`
implementation special-cases, and reproduces a hard crash here.

`publishDecision()` runs on every judged Hall event
(`loop()`: `compareMovement(...); ...; publishDecision(j,before,result,point); publishNav(...)`),
unconditionally, Manual or AUTO. If ESP32/newlib's `vsnprintf` behaves like
the host libc tested here, **the first Hall event judged after boot would
crash the sketch** — which would make a field test not merely produce bad
telemetry but fail outright, likely before it produces any usable data at
all. `publishNav()`'s shift is instead a data-corruption bug, not a
crash — none of *its* shifted specifiers happen to be `%s` (they're
`%c`/`%lu`/`%d`/`%u`, so a shifted argument prints as a wrong-but-harmless
number rather than dereferencing one), but the `mm/marker` topic's `obs`,
`expected`, `opened_ms`, `gap_ms`, `raw_open`, `base_open`, `pwm_open`,
`unresolved_count`, `seq_len` fields would all carry the wrong value from
that point on, and `seq_matches` would be silently dropped.

### The fix

Add one `"\"evidence\":\"%s\",` field to each format string, immediately
after `"ruling":"%s",` — restoring one specifier for the one argument that
was already being passed. That is the whole fix; nothing else in either
function needs to change. I have not applied it — this is still someone
else's patch under review, and 0.2 and 0.3 are both affected identically, so
whether the fix lands as a 0.4 patch or an edit to one of the existing
artifacts is not mine to decide.

## Build verification performed here (clean, not cached)

Same assembled-dependency approach as the 0.2 review (borrowing
`RouteMap.h`, `MovementEvidence.h`, `Ops.h`, `Stations.h`, `HallObserver.h`,
`LocoConfig.h` from `NAVI_IR`; `LL_LocoConfig_9950011.h`/`_9950012.h` from
`NAVI_SIMPLIFIED`; `ExcursionDetector.h`/`MagnetRecognizer.h` from
`NAVI_ONE_X22`; common headers and QUORUM's `credentials.h`), verification
copy only, not part of the repo:

| Check | Result |
|---|---|
| `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` on `test_coherence.cpp` | **PASS**, 0 warnings, ASan/UBSan clean, all 7 assertions |
| `arduino-cli compile --fqbn esp32:esp32:esp32 --clean` | **PASS** — 1,000,539 B flash (76%), 68,028 B RAM (20%) |
| same, `--warnings all --clean` | **15 warnings**, all in `publishNav`/`publishDecision`, all `-Wformat`/`-Wformat-extra-args` (plus 3 unrelated, pre-existing warnings from the vendored Adafruit_INA219 library) |
| Standalone host reproduction of `publishDecision`'s exact format string/argument types | **SEGV** (ASan), confirming the mismatch is a live crash, not just a compiler nitpick |

The host test suite's 7 scenarios don't exercise either publish function's
formatting at all (they test `Navigator` in isolation via `Navigator.h`,
never calling into the `.ino`'s `publishNav`/`publishDecision`) — a passing
test suite here says nothing about this bug either way, which is exactly
why it went unnoticed until a clean rebuild's compiler warnings were
actually read.

## Documentation currency

Same open items as the 0.2 review: no `firmware/README.md` catalog entry,
and the placement question (`firmware/NAVI_COHERENCE/` vs
`firmware/test-programs/NAVI_COHERENCE_0_3/`, needed for `LocoConfig.h`'s
and `HallObserver.h`'s relative includes to resolve) is explicitly deferred
by 0.3's own README ("must be placed where the existing shared dependencies
resolve") rather than resolved. Neither is new to 0.3; neither is mine to
decide.

## Bottom line

0.3 does exactly what it says: five precise, verified corrections to the
five things flagged in the 0.2 review, no scope creep, no regressions. On
its own terms it is ready. But a `--clean` rebuild — which the 0.2 review
should have done and didn't — found a real, reproducible, likely-crashing
bug in code neither patch touched. Fixing it is a two-line change (one added
field in each of two format strings) but it should happen, and be verified
with a **clean, uncached** build plus a host reproduction of the fixed
format string, before either 0.2 or 0.3 is flashed to Toby.

## References

- [Patch README](../firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_3/README.md)
- [0.2 independent review (corrected above)](NAVI_COHERENCE_0_2_INDEPENDENT_REVIEW_20260921.md)
- [Development history and governing design](NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md)
- `firmware/test-programs/NAVI_IR/NAVI_IR.ino` (the internally-consistent
  version these two functions were adapted from)
