# Replacement host navigator — implementation report

Date: 2026-08-22
Status: Host model complete against the generated acceptance suite. Not
validated. Authorises no firmware change, no flash and no train run.

Companion to `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`, the frozen
harness at `db9bb54`, and decisions 0042 and 0043.

## What was built

The replacement navigator, as a host model, at `tools/navlab/hostnav/`:

| file | role |
|---|---|
| `route.py` | committed map parsed read-only; `W_dir`/`W_both` computed from it |
| `params.py` | engineering parameters (§10), all uncalibrated |
| `envelopes.py` | robust timing-envelope interface, PWM ring |
| `branches.py` | propagation, branch-local elapsed time, branch list |
| `occupancy.py` | conservative occupancy, peer bounds, decision-0033 separation |
| `navigator.py` | four states, confirmation, movement authority, telemetry |
| `test_hostnav.py` | 23 focused unit tests for the internals |
| `README.md` | module, command, architecture, unvalidated assumptions |

Factory: `tools.navlab.hostnav.navigator:Navigator`. It is deliberately
outside `tools/navlab/acceptance/`, which remains the independent harness.

A runner-only `--require-navigator` gate mode was added first, in its own
commit: it changes no family, no generated stream, no invariant and no
expected outcome, only how the tally becomes an exit status. In gate mode a
missing or unloadable navigator, any FAIL and any NOT_IMPLEMENTED each exit
non-zero, and NOT_DEMONSTRATED stays a distinct reported status that never
counts as a pass. The ordinary no-navigator command still reports
PASS=18 FAIL=0 NOT_IMPLEMENTED=31 NOT_DEMONSTRATED=3 and exits 0.

## Result

```bash
NGR_NAVIGATOR=tools.navlab.hostnav.navigator:Navigator python3 -m tools.navlab.acceptance.run_acceptance --require-navigator
```

**PASS=49  FAIL=0  NOT_IMPLEMENTED=0  NOT_DEMONSTRATED=3**, exit 0.
Byte-reproducible; the record is
`tools/navlab/results/hostnav_generated_acceptance.json`.

Every one of the 31 navigator-dependent families passes, including the four
that carry the harness's own teeth:

- **S1** — no false confirmation anywhere, across every family. A single one
  would have raised `SuiteFailure` and stopped the run.
- **S2** — the true `(marker, direction)` is inside every set the navigator
  marks `COMPLETE`, at every detection, including across the reversal family
  and at every discontinuity gap size from 0 to 20 intervals.
- **S3** — no authority granted that could close the decision-0033 bound
  under any candidate pair, in `T14`, `T14b` and `T20`.
- **T15.5** — 10,000 generated candidate sets, zero cases where the published
  occupancy omitted a candidate the bitmap held.

The usefulness gates are met rather than dodged. Launch-region acquisition
reaches the true marker from all 20 cases within the computed `W_dir = 10`
observations; orientation-known route-wide startup does so from all 342;
orientation-unknown from all 342 within `W_both = 12`; zero unscheduled
navigation stops across every CLEAN family; zero speed reductions from
isolated doubtful detections over the 500-event oscillation family.

The three NOT_DEMONSTRATED results are unchanged and are the three things
generated data cannot supply:

- **N1** — no untouched anchored capture exists, and none was consumed.
- **N2** — no engineering parameter carries calibration evidence. §10 blocks
  candidate freeze until each does. No historical log was analysed to move
  this.
- **N3** — no protected-region declaration mechanism exists in firmware
  (open operator decision 4).

## Specification readings that were choices

Five clauses admit more than one implementation. Each choice, its rejected
alternatives and its consequences are recorded in decision 0043: the reversal
shadow that keeps the complete set complete across an unobservable native
reversal; confirmation by uniqueness only; a zero lower distance bound with
standstill taken from the PWM profile; C1 never established from silence; and
a contradiction latching a fault until the operator restarts.

No frozen test was changed to accommodate any of them, and no specification /
contract / generated-truth contradiction was found. The one clause that came
closest — §3.5.1's direction-preserving propagation formula against the
reversal family's requirement that the truth stay in `H` — is reconcilable,
and decision 0043 records how: the formula describes the tracked set, and the
published complete set is the union of the tracked set with a
reversal-admitting one.

## What this is not

Passing generated cases is not evidence about the railway. Every distance
window in this model rests on an uncalibrated nominal PWM-to-speed table
standing in for the versioned envelope table §3.8 requires and which does not
exist. The amplitude and duration floors separate the *generated*
distributions cleanly and have no measured basis. The full list is in
`tools/navlab/hostnav/README.md` under "Known unvalidated assumptions", and
`N2` is the acceptance suite's own statement of the same thing.

Production firmware is untouched. No QUORUM change, no MQTT or CTO
integration, no station behaviour change, no historical-log analysis, and no
capture spent.

## Next, and not yet authorised

1. Calibration evidence for every §10 parameter, which is what closes `N2`.
2. The protected-region declaration mechanism and the two new operator
   commands (open operator decisions 3 and 4), which is what closes `N3`.
3. Only then a candidate freeze, and only then one untouched anchored capture
   spent per `docs/AUTONOMOUS_ACQUISITION_VALIDATION_PROTOCOL.md`.
4. Firmware implementation remains a separate, separately approved work item.
