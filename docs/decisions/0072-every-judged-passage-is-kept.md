# 0072 — Every judged passage is kept

**Date:** 2026-09-02, evening
**Status:** PROPOSED. Not authoritative until the operator reviews and approves it.
**Touches:** 0065 and 0071 (the recording is never altered; only the judgement
copy is filtered), 0070 (the stitched passage and its `stitchAt`), the
waveform-capture rulings of 2026-08-31 ("do not discard information the
firmware processes to compute the value").
**Evidence:** `docs/NAVI_ONE_TWO_SIDED_CALIBRATION_20260902.md`,
`docs/NAVI_ONE_1_0X9_THE_ARCHAEOLOGY_20260902.md`,
`docs/CURVES_DATABASE_IMPLEMENTATION_REPORT_20260902.md`.
**Builds:** none. The Pi-side decoder is written and proven on the mirror; the
locomotive-side change is a proposal
(`docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260902.md`), not a build.

---

## The decision, in the operator's words

> One thing we should be doing is building a database of magnet curves with
> every run, storing it on the 128 GB card in the Pi.
>
> — the operator, 2026-09-02

Every Hall passage the locomotive judges is kept, accepted or refused, in one
database on the Pi's card, exactly as the firmware recorded it, together with
the verdict the firmware reached on it. The archaeology, the two-sided
calibration, and whatever recognizer comes next run against the whole history,
not against a text file assembled by hand.

## What forced it

The two-sided calibration of 2026-09-02 rests on 312 real passages. Assembling
them took a day of forensics across the 2026-08-28 survey and three days of
window dumps, because the fielded build publishes a waveform only when it
refuses one, or when AUTO is withdrawn. The single most informative record of
the day — the one stitched arc that **passed**, Arches CW at 11:03:54, residual
0.1223 — was never transmitted and cannot be recovered. Its scalar event is in
the log; its shape is gone.

On 2026-09-02 the locomotive judged about 5,600 passages. 124 waveforms reached
the Pi. The other 98% were judged, acted on, and discarded.

## What is kept, per passage, and why

| kept | why |
|---|---|
| **the samples, verbatim** — oriented, entry-baseline-relative int16 counts, decimated exactly as the firmware stored them, spikes included | Decisions 0065 and 0071: the recording is never altered; only the judgement copy is filtered. A replay must reproduce the field verdict to the count. Nothing is smoothed, resampled or reconstructed on the way in. |
| **decimation** (ms per stored sample) and **sample count** | Without them a stored point has no duration. 0071 registers that five samples at decimation 128 is 640 ms; the database must let a reader see that. |
| **the wire header's verdict** — outcome, isMagnet, shapeTested, peak, gain, amplitude ratio, residual, gap, polarity | What the recognizer decided on this passage, from the same struct it decided with. |
| **the marker event's verdict** — event, ruling, why, mm, target, direction, observed and expected pole, `two_sided`, `trunk`, `why2`, `stitched`, `paused_ms`, trust | What the navigator did with the recognizer's answer, and the archaeology's reason. Arrives on `mm/marker` within about 2 s; matched by peak and residual, never by time alone. |
| **the build** — sketch name, subtitle, build class, and the boot it belongs to | A residual of 0.1506 means one thing under a three-wide median and another under five (0071). No number in this database is comparable across builds without the build beside it. |
| **every arrival** of the same passage | A refusal dump and a withdraw window can both carry one passage; that it arrived twice is a fact about the transport, recorded, not a duplicate to be silently dropped. |
| **a derived splice, marked derived** | Until the wire carries `stitchAt`, the join of a stitched record is estimated the way `arches_fixtures.py` estimates it (largest median-of-three step, at least 30 counts), only on passages the marker event says were stitched, and the column beside it says so. When the wire carries the real boundary, the column says `wire`. Provenance travels with the number. |
| **every marker event**, waveform or not | The accepted 11:03:54 arc has no waveform and never will; its verdict row is the record that it happened. |
| **every undecodable line**, with the reason | Six lines of 2026-08-31 10:20 were written raw before the logger base64-encoded binary, and their bytes over 127 were lost on the way in. They are in the database as rejects, not absent. |

What is **not** kept: anything computed from the samples. Peaks, fits, splices
and verdicts are stored only when the firmware produced them, or when the
column says they were derived and how.

## Where

`/home/david/NGR/curves/curves.sqlite` on the Pi's card, one SQLite file,
written by `decode_curves.py` from the same `all_YYYYMMDD.log` files the
logger already writes. It is **derived data**: the logs are the record, the
database is an index over them, and the whole thing is rebuilt from the Mac's
telemetry mirror in twelve seconds. A card failure loses nothing the nightly
pull has not already saved. The tools that consume it (`tools/two_sided/`,
`tools/curves/score_bench.py`) read an exported text file, one passage per
line, the format they read today.

## Retention

Never prune. The card is 128 GB with 102 GB free (2026-09-02). A passage is
about a kilobyte; a day of marker events about a megabyte. At the 2026-09-02
rate, with every judged passage published, the database grows by roughly six
megabytes a day. The card outlives the railway's interest in any of it.

## The schema is provisional

The recognizer work is not finished. The archaeology's rules were set on
2026-09-02 and have not run on the railway; what a future judgement needs to
know about a passage may not be what this one records. The columns above are
what today's judgement uses and what the calibration needed. Columns are added
when a judgement needs them; nothing is removed, and nothing already stored is
rewritten. The exporter keeps producing the line format `tools/two_sided/`
reads, so a change of schema never strands a tool.

## The locomotive-side change, and its unintended consequence

To keep every passage, the locomotive has to publish every passage. The
proposal (`docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260902.md`) publishes
each judged passage verbatim, on the existing `diag/waveform` topic and format,
with a trailer carrying `stitchAt`, `restLevel`, `preSamples` and a sequence
number, from a queue that is drained only when nothing else is waiting and,
by preference, at station dwells.

The consequence to state now, because it is the kind that surfaces later as
something nobody decided:

- **It is radio airtime the running train does not have today.** Toby's own
  traffic at cruise is about 2.1 KB/s and 8.5 messages a second; this adds
  under a message a second and, in the typical case, under a millisecond of
  airtime per second. That is small. It is not zero, and the 2.4 GHz radio is
  the one the fielded QUORUM firmware also uses for ESP-NOW CTO at a 500 ms
  cadence. The proposal's queue discipline exists so that an archive frame can
  never be *ahead of* a running message and can delay a CTO beacon by at most
  one frame's airtime. That bound is by design, not by measurement: NAVI_ONE
  has no ESP-NOW today, and the first build that has both must measure it.
- **It is RAM on a build whose free heap has never been reported.** A queue
  deep enough to hold an inter-station run is about 32 KB. The proposal's
  first step is a free-heap figure in the status line, before any queue is
  sized.
- **Passages will arrive at the Pi late and in bursts**, at the next dwell,
  minutes after their marker events. The decoder matches on peak and residual
  over a thirty-minute window and does not care; a person watching the console
  will see waveforms land in clumps at stations and should not read that as a
  fault.
- **A retained archive message would misdescribe every later boot.** The
  archive is never retained, for the reason `diag/waveform` is not.

## What it does not change

- The recognizer, the capture, the ceiling, the stop-episode rule. Nothing the
  locomotive decides changes because it is also remembered.
- The refusal dump and the withdraw window. They keep firing; the archive is
  in addition to them, not instead.
- The logger on the Pi. The archive uses the topic the logger already
  base64-encodes; a new topic would have been logged raw and lossy, which is
  exactly how the six 2026-08-31 lines were lost.

## Attribution

The operator asked for the database, in the words quoted above, on 2026-09-02,
and deferred it behind keeping Toby running. What to keep, the key, the
provenance columns, the derived-data placement, the queue discipline and the
airtime bound are an agent's engineering choices made to serve that request;
none of them is his ruling until he says so.
