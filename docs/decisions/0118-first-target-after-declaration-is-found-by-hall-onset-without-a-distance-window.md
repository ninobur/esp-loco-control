# 0118 — First target after declaration is found by Hall onset without a distance window

Status: Accepted rule, 2026-10-02; supersedes 0117's startup rule.
Implementation **NOT field accepted**. No flashing authorized by this record.
Decided by: David, in the scoped FT2 revision instruction.

## Decision and reason

At declaration the locomotive's position within the interval is unknown; the
first magnet is anywhere in `(0, interval]`. A first-target distance window or
missed-magnet ruling assumes an exact marker origin that the declaration does
not establish. A resulting miss carries that unknown offset into the next
target's expected distance, allowing misses to cascade.

Restore the earlier NAVI startup behavior: find the first known target by
target-polarity Hall support onset observed after declaration, retaining the
existing absent→present guard, applicable IR and `traveled > 0` requirement.
There is **no distance window or upper bound**, and **no MISSED_MAGNET ruling
for the first target**, however far the locomotive travels. The declaration
IR reading can establish positive travel but is not a marker-distance origin.
Confirmation establishes the physical IR origin at the selected Hall landmark
exactly as before. Clear the first-target flag on confirmation and reversal.

From the next target onward, ordinary ±15% interval-local windows, cumulative
miss distances and physical-origin policy are unchanged. Polarity, ±70,
median-of-five, direction, IR applicability, landmark selection, spatial
reference, commands, stations, MQTT and NSR1 formats are unchanged. Runtime
build identification becomes `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2`.

## Accepted risk and boundaries

**If the first magnet is not detected, the next target-polarity magnet is
accepted as the first, silently, one or more markers off.** This is a known
limit of the authorized rule, not addressed by an alternative-position search.
Reversal before first confirmation still uses the assumed declaration origin;
reverse math is explicitly out of scope. Judgment remains NAVI-owned; measured
travel is distinguished from knowledge of marker position (principles 1, 3, 10).

Rollback points: `c3c925ac10c55bf3a9be1ed4a1a31d0f4af117f1` (0117 implementation)
and `d0185be25ec51f9ba6d58567458a59d3f1c289ca` (David's current Otto build).
Field basis: David's Otto CCW report, `9950011_20261002_120424.log`, declaration
45 and target 44 support at 106 mm. Host tests reconstruct that case; they do
not constitute a full NSR replay or field acceptance. Stop for David's review.
