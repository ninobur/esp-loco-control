# Final Toby R3 run: recovered logs and analysis

2026-09-23, Pi receipt timestamps in PDT. Analysis only; no control commands,
firmware changes, service restarts or serial readers were used.

## Scope and result

Recovered the Pi's authoritative daily MQTT log and preserved the complete
Toby/dispatcher slice from R3 battery boot at 21:58:48.578 through the downloaded
snapshot's final relevant message at 22:40:59.079: 47,477 messages. Boot
2C074C69CDA60A10, IR boot D542D673B80110D9. One Toby boot in this interval.

Contrary to the earlier short-capture description of "manual motion", the
full log records cmd/auto=1 at 21:59:09.174 and AUTO remained enabled until
dispatcher release at 22:37:18.981. This was an AUTO run with an operator pause.

Declared interval 050-051, CCW. First accepted landmark MM50 at 21:59:28.562;
last accepted MM42 at 22:37:20.417. NAVI's 1,206 marker steps equal seven full
171-marker circuits plus nine steps from its initial MM51 reference. These
are reported route progression, not independent visual certification.

## Navigation retained context

All 1,221 Hall event serials are present, consecutive and unique:

- 1,192 ordinary ADVANCED judgments.
- 22 NON_LANDMARK_HALL refusals, preserving current position.
- Seven MISSED_AND_ADVANCED judgments, each advancing two markers locally.
- No correction/relocation, no post-declaration LOST or UNCERTAIN transition,
  no redeclaration and no direction reversal.
- Recovery reports: nine WARMUP, 1,190 INCUMBENT_BEST. This run does not exercise
  the competing-hypothesis correction branch, so cannot certify that branch.

Seven refusals deserve special attention. Each was followed by a two-marker
advance. The supposedly missing observations were actually captured and
refused, not absent from the log. They cluster at expected MM125 (two), MM121
(three), and MM19 (two). Recorded Hall opening and window poles agree.

For MM125/121, IR reported 250.952 mm against a 300 mm span: only 4.048 mm
below the 255 mm lower admission bound. For MM19, 270.256 mm against 320 mm:
only 1.744 mm below the 272 mm bound. Both shortfalls are less than one 9.652 mm
pulse. Subsequent two-span distances were accepted and history continued.
This suggests a distance-window/alignment/quantization investigation, not an
assertion of seven physical Hall detection failures or automatic threshold
widening. Map geometry and actual pulse errors remain possible contributors.
The other 15 refusals had only 19.304-38.608 mm since the anchor, far short of
the next mapped magnet. Their classification is consistent with extra signals,
but physical identity is not independently proved by these logs.

## Station operations: one reproducible pause/resume omission

27 complete DWELL_BEGIN -> DEPART sequences were logged: Patio six; Bamboo,
Arches and Grillers seven each. Dwell zero-PWM periods were approximately five
seconds. No station-withdrawal, low-voltage or E-stop warning occurred.

At 22:05:20.379 the operator paused. MM25 was accepted at 22:05:20.519, then
MM24 at 22:05:21.611. GO resumed at 22:05:55.632. Toby passed reported MM15,
14 and 13 at PWM90 without a Patio approach/dwell sequence. The next station
sequence was Bamboo. The run's other six Patio approaches completed normally.

Code explains the omission: stationService returns while autoRunning=false;
an idle StationMachine arms only at off==APPROACH_START (-10). For Patio CCW
that is MM25. Passing it during the pause left the machine idle, and resuming
at MM24 did not arm the already-entered approach. This is a station-intent
continuity defect, not evidence that NAVI lost its location. A focused
pause-before-MM25 / resume-MM24 regression is the next appropriate check.
No fix was applied during this analysis.

## IR continuity, distance and display

All 29 epoch endings were INSTRUMENT_UNAVAILABLE / INADEQUATE_CONTRAST around
deceleration: 27 station stops, the explicit pause and final release. Nearest
STATUS readings were PWM11-21, within 0.502 s; PWM does not independently prove
wheel motion. No epoch ended during the intervening cruise portions. Each
departure reacquired, without a NAV position reset. Detector continuity remains
distinct from NAVI's retained position/motion knowledge.

Six successive Grillers-to-Grillers stop intervals contained 5366, 5365, 5367,
5363, 5369 and 5366 pulses. Nominal travel: 51,763.676-51,821.588 mm, mean
51,792.632 mm. Full spread is six pulses / 57.912 mm, about 0.112% of the mean.
The map array sums to 52,150 mm: mean nominal travel is 0.685% below that.
This is strong repeatability evidence, not a new calibration or a count-error
rate: stopping point variation, map association and counter behavior across
low-speed epoch gaps are not independently measured here. Do not bridge these
gaps into certified continuous odometry merely because cumulative totals exist.

For 1,170 accepted, assessable Hall intervals, median signed IR/map residual
was -0.263%; median absolute residual 2.955%. These were selected by NAVI's
distance gate, so they are a conditioned consistency check, NOT an unbiased
accuracy estimate. Reject cases are preserved separately in the analysis JSON.

Every one of the 29 post-start zero-PWM episodes included STOPPED / 0 speed
after settling. There were 300 STOPPED reports while raw IR remained unavailable.
The operator pause kept pulses at 6329; the final stop kept them at 37944.
The final stop had 191 STOPPED reports before a last REACQUIRING report at
22:40:35.681, with PWM0 and unchanged count. Handling context for that last
sample is unknown; it shows that current display interpretation does not yet
retain STOPPED across every instrument state. It did not reset navigation.

MM speed stayed at its final 237 mm/s (44.107 pKPH) after the last Hall event,
including in the final STATUS. This independently confirms the stale MM tile
reported by the operator. The shared train-state display correction remains
unimplemented; this analysis does not silently change it.

IR accepted packets exceeded 25,000; rejected, duplicate, health queue-gap and
publication-drop counters were zero. All sampled telem/ir reports were fresh.
This is not a claim of zero radio packet loss: sequence gaps can occur without
these rejection counters increasing. Last loopstat hall_drop, ir_drop,
baseline_diag_drop, stale_frame, pub_drop and cmd_drop were all zero. Recorded
battery voltage ranged down to 16.00 V; last was 16.07 V.

## Evidence and reproduction

- `field-records/logs/20260923_toby_final_r3.log.gz`: complete recovered slice.
- `field-records/analysis/20260923_toby_final_r3_summary.json`: topics, commands,
  states and exceptions.
- `field-records/analysis/20260923_toby_final_r3_detail.json`: epoch endings,
  zero-PWM episodes, refused/accepted interval comparisons and lap counts.
- `tools/analyze_navi_final_run_20260923.py`: offline parser; use the capture
  path plus `--out /private/tmp/navi-final-recheck` to reproduce both JSON files.

Daily source snapshot SHA256:
118dfe5ae8104a90e6bfe43597e3e7a65793e0a926b02d007990f850f0d02d0b.
Daily source was actively appended on the Pi during download; the stored slice
defines the analyzed endpoint, not the whole night's eventual final byte count.
Reanalysis from the compressed slice reproduced the daily-source JSON results
byte-for-byte. No missing Hall serials or unreported extra boot were found.

Bottom line: strong evidence for retained trajectory through repeated stops
and local refusals; one definite station-arming gap, a repeated near-boundary
distance-gate issue, and the already-known stale Hall-speed display remain.
No automatic promotion to field-accepted navigation or new IR accuracy bound.
