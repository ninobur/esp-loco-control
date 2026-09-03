# The curve database — review response and second implementation report

**Date:** 2026-09-03
**Supersedes the status of:** `docs/CURVES_DATABASE_IMPLEMENTATION_REPORT_20260902.md`
**Reviewer:** Sam/CODEX, 2026-09-03, on the 2026-09-02 commit
**Also incorporates:** the operator's `NAVI_ONE_STATION_CURVES_0_1` field run
of 2026-09-03
**Status:** decoder rewritten and proven; 33 automated tests; decision 0072
narrowed to the Pi side; decision 0075 opened for the locomotive side, gated.
Nothing deployed. No firmware modified.

---

## Disposition

The review's recommendation was to split the decision — accept the Pi-side
historical database, and gate locomotive publishing on identity, queue,
connectivity and CTO conditions. **That is right and it is done.** 0072 now
covers only the database; 0075 carries the transport and states what must be
measured before anything is built. (It was written as 0074 and renumbered on
landing: decision 0074, "Shape is recorded, not refused", was committed to
this branch while this work was in progress.) Both remain PROPOSED, because no
record here has force until the operator ratifies it.

Every finding was accepted. Two of them were real defects that the mirror
could not have exposed, and one of them would have silently destroyed data.

## Findings, and what each cost

**1. Critical — passage identity was not boot-safe. Accepted; fixed.**

The key was `(loco, opened_ms, closed_ms)` and both are `millis()`, which
restarts at every boot. Two passages from different boots could alias, and the
collision was treated as another copy of the same passage: one of them would
have been silently discarded, in a database whose whole claim is that it keeps
everything.

The key is now `(loco, boot_id, source, opened_ms, closed_ms)`, with
`(loco, boot_id, passage_seq)` enforced UNIQUE wherever a passage carries its
own number.

The review proposed `boot_id` as though one existed. It does not, and finding
out how to derive it produced the most useful fact of the day:

- `state/bootid` is published **once**, in `setup()` — but **retained**, so
  the broker replays it to every reconnecting subscriber. A bootid line in the
  log is not proof of a boot. Older QUORUM firmware republished it on every
  connection. Its payload names the build, not the boot. (The operator
  confirmed both halves of this independently.)
- `alert` carries `uptime_ms` about once a second, in **100%** of alerts in
  every era checked: 35,689 on 2026-08-28, 21,625 on 2026-08-12 (the other
  locomotive), 10,954 on 2026-09-03.
- A boot boundary is **uptime going backwards**. Deriving it instead from
  wall-clock minus uptime looks reasonable and is wrong: when the broker link
  flaps and delivery is delayed, that offset wanders by up to 50 s, which
  splits the morning of 2026-09-02 into **46 boots that did not happen**. The
  same window is one boot by uptime monotonicity, which is immune to delivery
  jitter.

A passage is attributed to the boot current **at its arrival**, which is
correct by construction: the device published it. `boot_src` records how the
boot was established and `boot_partial` marks an epoch joined in progress. A
`millis()` rollover reads as a boot, which splits one boot in two and can
never merge two — the safe direction, and asserted in the tests.

**2. High — the sequence number did not solve association, and one marker could
attach to several passages. Accepted; both fixed.**

The pending loop did attach one event to every pending passage that fit. Real
bug, now impossible.

The rule in both directions is: **when the choice is not forced, nothing is
attached.** More than one event fits a passage, or one event fits more than one
passage: `match_ambiguous` is set, `match_candidates` counts them, their
timestamps go in `extra`, and no marker fields are written. `match_method` is
`seq` (deterministic), `heuristic`, or NULL, with `match_confidence` beside it.
A marker already carried by a passage is never offered to a second one.

The deterministic path now has real data to work on: the operator's
`diag/wave_meta` carries `seq`, and the decoder ingests it. On the wire in
`mm/marker` it is still absent, which is gate 4 of decision 0075.

**3. High — the CTO delay bound was overstated. Accepted; withdrawn.**

The claim that an archive frame could delay a CTO beacon by at most one
frame's airtime accounted only for a frame already transmitting. It ignored
driver contention between TCP/MQTT and ESP-NOW, retransmission and backoff,
rate fallback, the blocking `mqtt.publish()`, task scheduling, and several
locomotives at one station. The proposal now says: the queue rule limits each
scheduling decision to one publish; **end-to-end CTO impact is unbounded until
measured**, and 0075 gates a build on measuring it with two locomotives under
weak WiFi and broker failure.

**4. High — station bursting was worse than trickling. Accepted; redesigned.**

One frame per network-task pass at a 10 ms period is 100 publishes a second.
And `!autoRunning` is not "stationary": a locomotive under manual control is
moving. The gate is now `actualPwm == 0 && commandedPwm == 0`, with a token
bucket of 2 to 5 frames a second **in every state including a dwell**, and
suspension while CTO is paired, while holding or decelerating, while peer
freshness degrades, or while ordinary traffic is backed up.

**5. High — "every passage" was not the delivery contract. Accepted; reworded.**

0072 now says *every passage that arrives is kept*. 0075 says *every judged
passage is offered to the archive, and loss is detectable* — by the sequence
gap, which `decode_curves.py audit` reports per boot, and by the drop counter.
Literal completeness needs durable buffering or acknowledgement, and neither is
proposed.

**6. Medium — the airtime evidence was not representative. Accepted, and the
gate has been run.**

The operator built and ran `NAVI_ONE_STATION_CURVES_0_1`. His 1,177-passage
run replaces every estimate:

| | first draft's estimate | the field |
|---|---|---|
| passages a minute | 45 | **26.7** mean, 50 busiest minute |
| bytes a passage | 494 mean | **375** mean, 336 median |
| archive traffic | 370–600 B/s | **167 B/s** mean, 319 B/s busiest minute |
| single-frame passages | 87% | **96.4%** |

The old sample was biased upward exactly as the review suspected: it consisted
of refusals and withdrawal-window slots, which are the long and decimated ones.
Two locomotives and synchronised station bursts remain unmodelled, and CTO
remains untested.

**7. Medium — chunk validation was too permissive. Accepted; fixed.**

Now refused, each with its reason recorded: a body whose length is not exactly
`chunkSampleCount * width`; `chunkIndex >= chunkTotal`; `chunkOffset +
chunkSampleCount > sampleCount`; `decimation < 1`; `closedAtMs < openedAtMs`; a
`byte_length` disagreeing with the decoded payload; a `quant` outside 1–64; a
chunk repeated with different content; any chunk disagreeing with its slot's
other chunks on any header field that describes the slot; and a slot that
reaches its chunk count without contiguous cover of samples 0..n-1. A chunk
repeated with **identical** content is an arrival, counted in `copies`.

**8. Medium — no durable proof suite. Accepted; written.**

`tools/curves/test_decode_curves.py`, 33 tests, standard library, no network
and no Pi. It synthesises logs in the runlog's exact format and covers reboot
collision, `millis()` rollover, delivery delay not inventing a boot, partial
epochs, duplicate sequence numbers, all ten chunk-validation rules, both
trailer widths, both ambiguity directions, `wave_meta` arriving before and
after its waveform and without one, truncated bootid salvage, malformed JSON,
raw-era binary, a line still being written, re-runs, a growing log, a rewritten
shorter log, and the export format.

Writing them was worth it immediately: **the suite found the two defects in
finding 2 and one error in a test's own header offset.** The `growing log
equals one shot` test is the automated form of what the first report asserted
by hand.

**9. Low — provisioning enacted an unratified decision. Accepted; fixed.**

`tools/provision_pi.sh` no longer deploys the database. It prints how to, and
`CURVES=1` opts in. A rebuilt card no longer silently enacts a proposal.

## A finding of our own, from the ingest

**NAVI_ONE 1.0X11's `state/bootid` is truncated at 399 bytes** by the
`char b[400]` buffer in `setup()`; X11's added `subtitle` pushed it over. All
four X11 boots of 2026-09-03 published unparseable JSON, and any consumer using
`json.loads` — including the first draft of this decoder — silently learned
nothing about those boots. The decoder now salvages the leading fields, records
a reject, and flags the boot. Full record in
`docs/NAVI_ONE_1_0X11_FIELD_FINDING_15_THE_BOOTID_IS_TRUNCATED.md`. The
firmware fix is one line and is the operator's to make.

## The state of the database now

Mirror refreshed 2026-09-03 15:52 with the operator's read-only fetch script.

| | |
|---|---|
| days ingested | 23 (2026-08-12 to 2026-09-03), no errors |
| passages | **4,245** — 1,446 wire, 2,799 survey-format |
| markers / wave_meta / boots | 107,444 / 1,177 / 302 |
| rejects | 10 — six raw-era binary lines of 08-31, four truncated X11 bootids |
| boots partial / bootid truncated | 30 / 4 |
| marker match | 1,444 heuristic, 2 none, **0 ambiguous** |
| provenance | stitch from the wire 1,177, derived 9, none 3,059 |
| sequence integrity | 9950012@2026-09-03T15:07:02: seq 1–1177, **0 missing** |
| full backfill, on the Mac | 18 s |
| second pass | adds 0 |
| incremental vs one-shot, on the real 09-03 log | **identical**, row for row |
| export vs `extract_passages.py` | **identical as a set**: the extractor writes 1,508 lines, 8 of which are second arrivals of a passage already on an earlier line; the database holds each once, 1,500 |
| automated tests | 33, all passing |

## Librarian check on `NAVI_ONE_STATION_CURVES_0_1`

Per `docs/CLAUDE.md`, role and evidence status stated separately, and the
catalog checked.

**Role:** diagnostic instrument, `test-programs/NAVI_ONE_STATION_CURVES/`. It
declares `BUILD_CLASS "DIAGNOSTIC_FIELD_TEST"` and its own README says not to
treat it as an operational release. Correct placement under decision 0018; it
is not a production-control artifact and nothing about it is promoted by
having produced good evidence.

**Provenance, verified rather than taken on trust.** Every shared header is
**byte-identical** to `NAVI_ONE/`: `MagnetRecognizer.h`, `HallCapture.h`,
`TwoSided.h`, `Navigator.h`, `Ops.h`, `RouteMap.h`, `Stations.h`,
`WaveformWindow.h`, `WaveformDump.h`, `LocoConfig.h` and the Toby profile. The
sketch differs from `NAVI_ONE.ino` by 124 lines, of which the only ones
touching acquisition read `capture.open()` before and after the **unchanged**
`capture.sample()` call, so a passage's opening can be observed. No threshold,
no `examine()`, no capture argument is altered. The operator's statement that
recognition and stopping behaviour are unchanged is confirmed by the tree.

**Evidence status:** flashed and field-run 2026-09-03, 15:08–15:52, 1,177
passages, `pub_drop` 0. Measurement only. It is the controlling evidence for
decision 0075's size-and-rate gate and for nothing else — it says nothing
about CTO, which it does not have.

**Two catalog gaps, flagged not fixed** (`docs/CLAUDE.md`: flag anomalies,
don't silently fix them):

1. **The sketch is not committed.** It exists in the working tree only. The
   evidence decision 0075 rests on comes from a build that is not in git, so
   the run cannot presently be reproduced from the repository. Committing it
   is the operator's call, and its directory carries a `credentials.h`.
2. **`firmware/README.md` does not mention NAVI_ONE at all** — not this
   sketch, and not any of the eleven `NAVI_ONE_1_0X*` builds that have been
   flashed since 2026-09-01. The whole lineage is uncatalogued. That predates
   this work and is too large a librarian judgement to make unasked, but it
   means the catalog currently describes only the QUORUM lineage while the
   railway has been running NAVI_ONE for three days.

## What needs the operator's hand

1. **Decision 0072** — the Pi-side database. Ratify, amend or refuse.
2. **Decision 0075** — the locomotive-side transport, and specifically whether
   the CTO gate is worth running before any permanent archive is built.
3. **Deploy, or say so.** `./tools/curves/deploy_curves.sh`. Still not run
   against the Pi; the only thing done to it was a read-only ssh for its
   Python, SQLite and free space.
4. **Field finding 15** — the one-line buffer fix on a fielded build.
5. **Whether the station-curves sketch keeps running.** Its 1,177 passages are
   the best evidence the archive has, and it is already publishing exactly what
   the database wants.
6. **Committing that sketch**, and whether the NAVI_ONE lineage should be in
   `firmware/README.md` at all — see the librarian check above.
