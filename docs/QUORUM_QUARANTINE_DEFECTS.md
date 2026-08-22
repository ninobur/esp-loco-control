# Two quarantine defects, recorded separately — found 2026-08-21 hold-outs

Both are properties of the CURRENT production quarantine (1.16R lineage).
Neither depends on any experimental flag: they reproduce under BASE, T and TO
identically. Recorded per operator direction as standalone defects so they are
not lost inside the T/O verdicts.

## Defect Q1 — discard-inflated dt blinds the conservation gate

When quarantine DISCARDS a pending event, its interval is folded into the
successor's published dt (by design, so the committed chain measures physical
travel). But the folded dt is what the conservation gate then tests. On the
2026-08-11 beta log's derailment burst, three phantoms with fed intervals of
485/694/500 ms were ADMITTED by every variant because intervening quarantine
discards inflated their published dt past the reject band. The quarantine and
the conservation gate each did their job locally; their composition admitted
what neither would alone.

Evidence: beta-log hold-out, sessions s01/s02 decision streams
(`tests/results_20260821_holdout_adjudication.json`, holdout[1]).

## Defect Q2 — the commit path can admit a probable phantom

A weak RAMP-gate event (fed dt 262 ms, peak 38, duration 1,339 ms — phantom
signature on every axis) was QUARANTINE_COMMITTED at harness mm 24 under ALL
variants: the successor-fits-genuine arbitration vouched for it, and a vouched
event bypasses the conservation veto entirely. One admitted phantom = one
label slip = the failure class this whole investigation chases.

Evidence: same hold-out, beta log s01, log mm 29 / harness mm 24.

## Disposition

Under the reachability recovery plan both defects become regression fixtures
for the off-locomotive comparison (pending-hypothesis handling must catch
both), NOT patches to the current quarantine. No production change is
proposed here.
