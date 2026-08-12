# 0025 — The phantom was a maintenance artefact, not a firmware defect

Status: Accepted (2026-08-12)

## Decision

The extra Hall marker events tracked since 2026-08-10 were caused by **stacked
double magnets** installed as a remedy for markers that read weak. Replacing them
with single disk magnets eliminated the phenomenon.

Three consequences follow:

1. **No firmware containment will be implemented for the phantom.** The
   `expectedDt = previousAcceptedDt` change proposed in
   `docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md` and analysed in decision 0024 is
   **not** to be implemented on phantom grounds. It may still be argued for on
   other merits; it no longer has this justification.
2. **An unexplained extra event is a diagnostic signal.** It reliably indicates a
   doubled or otherwise malformed magnet installation. Operator policy: *if
   another phantom appears, dig.*
3. **Doubling a weak magnet is withdrawn as a maintenance practice.** The correct
   remedy for a weak marker is to reduce the sensor-to-magnet distance — a disk
   magnet on top of the crosstie — not to add a second magnet.

## Context

Evidence: `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md`.

One 190.9 min capture, one locomotive, one firmware image (`QUORUM_1_13`,
unmodified), with the magnets at mm 99–102 and 61–63 replaced between the two
sessions. On extraction, **mm 102 was a stacked double bar magnet** — two bars,
like poles aligned, south upward, clamped together and not distinguishable from a
single magnet without excavating it.

| | before | after |
|---|---|---|
| weak events (peak < 80) | 12 in 235 markers (5.1%) | **0 in 214 (0.0%)** |
| minimum peak | 39 | 113 |
| `PHANTOM_REJECTED` | 4 | 0 |

Every marker 0–170 was crossed and captured at least once after replacement.
mm 101 and 102, which produced a phantom on 9 of 9 ramped CCW crossings in the
2026-08-11 beta, produced a single clean read each.

### Why the earlier analysis did not find this

The mechanism is the one described as H2 in `docs/QUORUM_PHANTOM_HYPOTHESES.md`
— a doubled magnet's stronger, spatially broader field puts its return flux above
the ±38 enter threshold, so the reversed lobe opens a second event of opposite
polarity once `EVENT_EXIT_HOLD_MS` has closed the first. What H2 could not supply
from telemetry was *why only four markers on the route behaved that way*. The
answer was a maintenance action recorded nowhere and visible only by digging.

The telemetry contained the signature of the fault and never its cause. Further
analysis of the same logs would not have produced it.

An earlier inference that mm 102/103 were doubled was dropped on report rather
than on measurement, and the analysis then worked to explain the observations
without it. That is the process failure worth remembering.

### Amplitude is not the discriminator

After replacement the treated markers read **stronger** than before — peak median
at those sites 150 → 222, against an unchanged route median of 149 — and produce
no weak companion events. A strong single magnet does not phantom; a doubled one
does.

This retires two things: the absolute-peak-floor mechanism of the superseded
low-PWM proposal, and the causal reading of "the phantom sites are the strongest
markers". They were the strongest markers *because* they were doubled.

## Alternatives considered

**Implement the timing-expectation change anyway.** Rejected. The governing rule
is that no compensation or protection mechanism is allowed unless operational
data demonstrate the need. The need is now withdrawn: the generating fault is
physical and corrected. Implementing it would also suppress the signal that
identifies which magnet to excavate — consequence 2 above.

**Hardware EMI mitigation (H1).** Rejected as unnecessary. H1 was never excluded
by instrument and its mitigations remain independently reasonable, but no
observation now requires it: the polarity correlation, the site selectivity, the
fixed separation distance and the removal experiment are all accounted for
physically.

**A peak-magnitude-per-polarity detector.** Not rejected, but deferred and
separated. It would emit one event per magnet carrying the dominant lobe's
polarity, and it is the only proposal that could resolve an *identity* error —
which decision 0024 records the timing rule provably cannot. It is now much less
urgent. It owes its own record if ever proposed.

**Leave the doubled magnets and tolerate the offset repairs.** Rejected. On
2026-08-10 a doubled pair produced offset −2, which the fence cannot express, and
the incident-C cascade followed.

## Consequences

- Decisions 0023 and 0024 stand. 0024's *analysis* of why containment failed
  remains correct and is worth keeping; its proposed *change* loses its
  justification.
- `docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md` remains superseded.
- **The 1.13 HARD_BOUND advisory was exercised in the field for the first time
  during this session and behaved correctly** — on a full ring with no unique
  exact window it published `null` plus its audit fields rather than a guess.
  Decision 0023 is validated in service.
- **A separate, unresolved fault is now isolated.** The CW leg of the same
  session terminated in NO_QUORUM near mm 120 with the true offset outside
  `QUORUM_OFFSETS` — the same class as the 2026-08-10 incident C, and not
  addressed by the magnet work. Root cause is not established because the capture
  lost the marker trail. This does **not** reopen phantom containment; it is its
  own investigation.
- **The Pi SD card must be replaced before the next CW run.** It has now cost
  evidence in three sessions, and it is the likely reason ~860 of 1075 markers
  were never delivered.
- Route maintenance owes an audit: **enumerate every marker that received the
  doubling remedy**, since each is a predicted phantom site. mm 149 and 150 are
  already corrected; 99–102 and 61–63 are now replaced.
- Installation of a replacement magnet must be **polarity-verified against
  `NGR_DNA1`** before the track is returned to service. mm 99 was installed
  inverted in this round and was detected only by a running locomotive.

## References

- `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md` — the evidence
- `docs/QUORUM_PHANTOM_HYPOTHESES.md` — hypotheses and how the wrong answer was built
- `docs/decisions/0024-*` — containment analysis; its change is not implemented
- `docs/decisions/0023-*` — the advisory, validated in service here
- `docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md` — loses its justification
