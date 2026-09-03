# 0072 — Every judged passage is kept

**Date:** 2026-09-02, evening. Rewritten 2026-09-03 after Sam/CODEX's review
and the operator's own `NAVI_ONE_STATION_CURVES_0_1` field run.
**Status:** PROPOSED. Not authoritative until the operator reviews and
approves it.
**The title overclaims, and the correction is the point.** "Every judged
passage is kept" is true of what reaches the Pi and false of what the
locomotive judges: today's builds publish a waveform only on refusal or
withdrawal, MQTT is QoS 0, and nothing acknowledges receipt. The precise
contract is **every passage that ARRIVES is kept, forever**. The title is left
as it was written because decision 0074 already cites this record by it and
decisions are not renumbered or retitled once landed.
**Scope:** the Pi-side historical database ONLY. The locomotive-side change
that would feed it every passage was split out into **decision 0075** and is
gated there. This record was originally written to cover both; the review's
first recommendation was to separate them, and it was right — one is a reader
of logs that already exist, the other changes what a running locomotive puts
on the radio.
**Touches:** 0065 and 0071 (the recording is never altered; only the judgement
copy is filtered), 0070 (the stitched passage and its `stitchAt`), the
waveform-capture rulings of 2026-08-31.
**Evidence:** `docs/NAVI_ONE_TWO_SIDED_CALIBRATION_20260902.md`,
`docs/NAVI_ONE_1_0X9_THE_ARCHAEOLOGY_20260902.md`,
`docs/CURVES_DATABASE_REVIEW_RESPONSE_20260903.md`.
**Builds:** none. `tools/curves/decode_curves.py` reads logs; it is not
firmware and it does not touch a radio, a serial port or a locomotive.

---

## The decision, in the operator's words

> One thing we should be doing is building a database of magnet curves with
> every run, storing it on the 128 GB card in the Pi.
>
> — the operator, 2026-09-02

Every Hall passage that reaches the Pi is kept, accepted or refused, in one
database on the card, exactly as the firmware recorded it, together with the
verdict the firmware reached on it and enough provenance to say how sure of
each field we are. The archaeology, the two-sided calibration, and whatever
recognizer comes next run against the whole history rather than a text file
assembled by hand.

## What forced it

The two-sided calibration of 2026-09-02 rests on 312 real passages that took a
day of forensics to assemble, and the single most informative record of the
day — the one stitched arc that **passed**, Arches CW at 11:03:54, residual
0.1223 — was never transmitted and cannot be recovered. Its scalar event is in
the log; its shape is gone.

## The contract, stated exactly

**The logs are the record. The database is a rebuildable index over them.**
`all_YYYYMMDD.log` is authoritative. `curves.sqlite` is derived: it is rebuilt
from the Mac's mirror in about eighteen seconds, and losing it loses nothing
the nightly pull has not already saved. Nothing may be written into the
database that cannot be re-derived from the logs.

**Completeness is what arrived, not what happened.** This database keeps every
passage the locomotive *published*. It does not and cannot promise every
passage the locomotive *judged*: today's builds publish a waveform only on
refusal or withdrawal, MQTT is QoS 0, and nothing acknowledges receipt. The
honest sentence is *every passage that arrives is kept forever*. Any claim
about what was judged belongs to decision 0075 and is gated there.

**A passage is identified by its boot.** `openedAtMs` and `closedAtMs` are
`millis()` values that restart at every boot, so they cannot identify anything
on their own; two passages from different boots can carry identical times, and
treating that as a duplicate would silently discard one. The key is

> `(loco, boot_id, source, opened_ms, closed_ms)`

with `(loco, boot_id, passage_seq)` enforced unique wherever a passage carries
its own number — the canonical identity, which the firmware should eventually
put on the wire (0075).

**The boot is derived, and says so.** There is no boot id on the wire.
`state/bootid` is published once in `setup()` but **retained**, so the broker
replays it to every reconnecting subscriber and a bootid line is not proof of
a boot; older QUORUM firmware republished it on every connection. Its payload
names the build, not the boot. The boot is therefore derived from the only
signal that is dense, prompt and monotone: `alert` carries `uptime_ms` about
once a second, in every build era and on both locomotives, and a boot boundary
is uptime going **backwards**. `boot_src` records how it was established and
`boot_partial` marks an epoch joined in progress, so an uncertain attribution
is never silent.

**Association is never a silent guess.** Until the firmware puts a shared
identity in both the waveform and the marker event, a passage is matched to
its event on peak, residual and gap. `match_method` says how, and
`match_confidence` how close. When more than one event fits a passage, or one
event fits more than one passage, **nothing is attached**: `match_ambiguous`
is set, `match_candidates` counts them, and their timestamps are recorded.
Choosing the nearest would be a guess presented as a fact.

**Verbatim means verbatim, and derived says derived.** The samples are the
firmware's own recording — oriented, entry-baseline-relative, decimated as
stated, spikes included, per 0065 and 0071. `quant` says how coarse the stored
copy is and `decimation` how long each stored sample spans. Every field that
was not on the wire carries its provenance beside it: `stitch_src`,
`rest_src`, `pre_src`, `splice_src`, `boot_src`, `match_method`. A derived
splice is computed only for a passage the marker event says was stitched, by
the `arches_fixtures.py` rule, and labelled `derived:medstep30`.

**Malformed input is recorded, not swallowed.** A waveform line that cannot be
decoded becomes a row in `rejects` with the reason. A truncated `state/bootid`
is salvaged for the fields it still names and flagged — see the 2026-09-03
finding that NAVI_ONE 1.0X11's bootid is cut at 399 bytes by its `char b[400]`
buffer, the same fault 0071 records for marker events.

## What is kept per passage

The samples verbatim; decimation, sample count and quantisation; the wire
header's verdict (outcome, isMagnet, shapeTested, peak, gain, ratio, residual,
gap, polarity); the marker event's verdict (event, ruling, why, mm, target,
direction, poles, `two_sided`, `trunk`, `why2`, `stitched`, `paused_ms`,
trust); the build, subtitle and class, and the boot they belong to; every
arrival of the same passage, counted; and, where `diag/wave_meta` exists, the
passage's sequence number, station phase at open and close, station, marker
offset, commanded and actual PWM, `stitchAt`, `restLevel` and `preSamples`.

Every `mm/marker` event is kept whether or not a waveform came with it, so the
accepted 11:03:54 arc has its verdict on record even though its shape is gone.

## Where, and retention

`/home/david/NGR/curves/curves.sqlite` on the Pi's card. Deliberately **not**
under `NGR/telemetry`, so the nightly pull does not copy a derived file back
and forth.

Never prune. The card is 128 GB with 102 GB free. A passage is about a
kilobyte. The 2026-09-03 station-curves run published 1,177 passages in 44
minutes — about 1.2 MB an hour of running, with markers. The card outlives the
railway's interest in any of it.

## The schema is provisional

The recognizer work is not finished, and what a future judgement needs to know
about a passage may not be what today's records. Columns are added when a
judgement needs them; nothing already stored is removed or rewritten, and the
exporter keeps producing the line format `tools/two_sided/` reads, so a change
of schema never strands a tool.

## What it does not change

Nothing the locomotive decides. No recognizer, capture, ceiling or
stop-episode rule is touched by remembering what it did.

Decision 0074 relies on this one in a way worth naming: if shape becomes
advisory, the archive is the only place a residual does any work, and 0074's
requirement that comparisons across its discard fix "carry the build" is met
by the build, subtitle and boot stored on every row here.

## Attribution

The operator asked for the database, in the words quoted above, on 2026-09-02.
What to keep, the boot-scoped key, the provenance columns, the ambiguity rule
and the derived-data placement are an agent's engineering choices made to
serve that request. The boot-safety defect in the first draft's key, the
ambiguity rule, the chunk validation and the demand for a repeatable test
suite came from Sam/CODEX's review of 2026-09-03. None of it is the operator's
ruling until he says so.
