# The magnet-curve database — implementation report

**Date:** 2026-09-02, evening
**Locomotive:** Toby (9950012); the database is per-locomotive by column
**Status:** SUPERSEDED IN PART, 2026-09-03. Sam/CODEX reviewed this work and
found a critical defect in the passage key (it was not boot-safe) and three
more; the operator then ran his own `NAVI_ONE_STATION_CURVES_0_1` sketch, whose
1,177 passages replace the airtime estimates below. Read
`docs/CURVES_DATABASE_REVIEW_RESPONSE_20260903.md` first — it says what changed
and what the numbers really are. Decision 0072 has been narrowed to the Pi-side
database and the locomotive-side change split into decision 0075, gated.
Everything below stands as written on 2026-09-02 and is kept for the record.
**Files:** `tools/curves/decode_curves.py`, `tools/curves/deploy_curves.sh`,
`server/ngr-curves.service`, `server/ngr-curves.timer`, a step in
`tools/provision_pi.sh` (since made opt-in),
`docs/decisions/0072-every-judged-passage-is-kept.md` (scope narrowed 2026-09-03),
`docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260903.md` (redated).

---

## What was built

`decode_curves.py` reads the Pi's `all_YYYYMMDD.log` files and writes one
SQLite database. Standard library only. Three commands:

```
python3 tools/curves/decode_curves.py ingest <telemetry dir> <curves.sqlite>
python3 tools/curves/decode_curves.py export <curves.sqlite> <passages.txt> [--all]
python3 tools/curves/decode_curves.py stats  <curves.sqlite>
```

Tables: `passages` (one row per captured passage, samples as an int16 blob),
`copies` (every arrival of a passage), `markers` (every `mm/marker` event),
`boots` (every `state/bootid`), `rejects` (every waveform line that could not
be decoded, with the reason), `progress` and `context` (where each log was
read to, and the last bootid per locomotive, so a resumed pass knows the
build).

Two sources are decoded into the same row shape:

- `wire` — NAVI_ONE's `diag/waveform`, the 40-byte header and int16 samples.
- `wave8` — QUORUM 1.13X's `mm/wave` from 2026-08-28, int8 at a stated scale.
  The 187 survey magnets the calibration rests on are these, ingested from
  the mirror's own copy of that day, which carries the same 2,799 records as
  the curated gz in `field-records/`.

The exporter writes the line format `tools/two_sided/*.cpp` read
(`<tag> <n> <pre> v0 v1 …`), accepted passages by default, everything with
`--all`.

## What was proven, on the Mac mirror

The mirror was refreshed with the operator's read-only fetch script before
the runs (`~/ngr-telemetry/bin/fetch_pi_telemetry.sh`, 2026-09-02 20:19).

| | |
|---|---|
| days ingested | 22 (2026-08-12 to 2026-09-02), 2.0 GB of logs, no errors |
| wall time, full backfill on the Mac | 12 s |
| passages | 3,014 — 215 wire (08-31, 09-01, 09-02), 2,799 survey-format (08-28) |
| marker events | 106,006 |
| bootids | 427 |
| passages that arrived twice | 27, every one byte-identical on sample count, peak and close time |
| rejects | 6 — the raw-era binary of 2026-08-31 10:20:14, bytes over 127 lost by the logger before it base64-encoded (`ngr_runlog.py` fixed this the same day) |
| wire passages without a marker event | 2 of 215 (see below) |
| second pass over the same mirror | added 0 |
| incremental pass (first 60 MB of 09-02, then the rest) vs one-shot | identical rows: keys, marker matches, second events, splices, copies |
| export vs `tools/two_sided/extract_passages.py` on the same mirror | **same set of passages.** The extractor writes 327 lines; 8 of them are second arrivals of a passage already on an earlier line (a withdraw window re-publishing what a refusal or an earlier window had already sent). The database holds each once: 319 = 187 survey + 132 wire. The calibration's 312 is the same extractor on the mirror as of that afternoon. |
| derived splices on the stitched refusals | 104, 166, 188, 195, 266, 13 — the values `fixtures_arches.h` carries for A1967, A1818, A1810, A1832, A2460, A3104 |
| database size | 23.6 MB, of which the markers are most |

The second event of a stitched refusal (`NOT_A_MAGNET`, then
`STITCHED_REFUSED` 60 ms later) is on the row as `event2` for all 8 stitched
refusals in the mirror.

## Findings on the way

1. **The outcome byte was renumbered at X9.** `Outcome::Insufficient` was
   inserted at 4 and `NoCurve` moved to 5. The wire has no version field; the
   decoder names the code by the publishing build. No 4 or 5 has been seen on
   the wire yet. The proposal's trailer does not fix this either; a header
   version would. Registered, not fixed.
2. **The draft's key was wrong**, as the brief said: it keyed on a `boot_ms`
   the bootid does not carry. The key is now `(loco, opened_ms, closed_ms)`,
   the locomotive's own clock at open and close, which is what makes the
   refusal dump and the window copy land on one row. The ingest reports a
   collision (same key, different sample count or peak) if one ever occurs.
3. **Two 2026-09-01 passages have no marker event**: two `WRONG_SHAPE`
   window slots published at 16:26:48 by NAVI_ONE 0.9 with peaks 82 and 84,
   whose nearest events carry peaks 72 and 247. Whether 0.9's header peak and
   its event peak were the same number is not established; it predates
   decision 0065's judgement copy. Left unmatched and visible.
4. **A `mm/marker` peak and residual are, in practice, unique per passage
   within half an hour**, and the event follows the refusal dump by
   milliseconds. Matching on them, never on time alone, was right: a window
   dump publishes passages minutes old, and a synthetic test showed that
   time-only matching claims the wrong event when two consecutive passages
   look alike. A marker already carried by one row is not offered to another.
5. **8-bit at scale 2 costs the structural rule.** Whole-fit residuals move
   by 0.0001; the "two apexes" refusal count moves from 3 to 5 of 140. The
   numbers are in the proposal; the recommendation is verbatim.

## What is deployed

Nothing. `deploy_curves.sh` and the two systemd units are written and
syntax-checked and have not been run against the Pi. The Pi was queried
read-only over ssh for its Python (3.13.5), SQLite (3.46.1) and free space
(102 GB of 117) and nothing else. `provision_pi.sh` calls the deploy script
as a step, so a rebuilt card gets the database; that step, too, has not run.

## What needs the operator's hand

1. Read and ratify, amend or refuse decision 0072.
2. Deploy, or say so: `./tools/curves/deploy_curves.sh` from the repo root.
   It copies one file, installs a timer, and runs a first pass that reads
   every log on the card (minutes on a Pi).
3. The loco-side proposal: whether, verbatim or 8-bit, and the free-heap
   figure that comes before any queue is sized. It modifies `NAVI_ONE.ino`,
   `WaveformDump.h` and gate 5 and has not been started.
