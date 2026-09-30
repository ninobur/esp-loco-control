# 0115 — EWO uses a provisional single-sample boot reference and retains physical origin across Missed Magnets

Status: Current except its degraded-confirmation non-reopening clause,
superseded by 0116 (2026-09-30). Accepted 2026-09-29. This decision
supersedes/withdraws 0114 because the five-position startup mechanism addressed
an unproven failure mode and introduced a demonstrated startup failure mode.

## Decision

At boot, NAVI takes the first nonzero-PWM native Hall ADC observation as its
provisional initial reference. No IR movement, acquisition window, five-position
collection, boot median, retry, or permanent boot-readiness gate is used. The
provisional reference gets NAVI to the first expected MM; the established 0113
0–100 mm clearance and 100–200 mm spatial collection then replaces it at 200 mm.

A Missed Magnet advances the mapped target only. It does not advance or
manufacture a physical IR origin. The last physically observed/confirmed field
boundary remains the origin. Mapped intervals accumulate from that origin; the
coherence tolerance is ±15% of the current target's individual mapped interval,
not ±15% of the accumulated distance. A subsequently confirmed target makes
its observed leading boundary the new physical origin and resumes single-
interval navigation.

This decision does not reopen target-only authority, IR applicability, degraded
confirmation, PWM-zero semantics, spatial invalidation, or the exclusions in
0109–0113.
