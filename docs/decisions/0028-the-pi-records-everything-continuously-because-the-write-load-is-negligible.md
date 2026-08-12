# 0028 — The Pi records everything, continuously, because the write load is negligible

Status: Accepted  (2026-08-12)  — supersedes 0027

## Decision

`ngr_runlog.py` runs on the Pi as `ngr-runlog.service`, subscribed to `ngr/#`,
writing every message with a millisecond receipt timestamp. It starts at boot
and is never off. The rolling `all_YYYYMMDD.log` is the system of record; the
per-run files under `runs/` are a convenience.

Nothing is pruned. The operator's requirement is that everything is recorded all
the time.

The half of 0027 that concerned *configuration* carries forward unchanged: any
unit file, broker setting or provisioning step added to the Pi is committed to
`server/` or `tools/` at the same time.

## Context

0027, written hours earlier during the SD card rebuild, refused to run a
continuous logger on the Pi on the grounds that continuous telemetry writes were
"the wear load aimed at the part that failed."

That was an estimate, and it was wrong. Measured on the live broker with a train
running, the full `ngr/#` stream is **48,507 bytes over 30 seconds — about 1.6
KB/s**, or roughly 6 MB per hour of running. Even continuously that is tens of
GB per year, against a 128 GB card with 106 GB free. The new card is also a
SanDisk Extreme rather than whatever failed.

Two other facts pointed the same way. The rebuilt host measured `throttled=0x0`,
so the failure was a genuinely bad card and not a power fault that a new card
would inherit. And Mac-side capture has its own failure mode already recorded in
0026 and commit 10e64cb — the Mac slept, and the capture went silent in a way
that read as quiet running. The Pi does not sleep.

## Alternatives considered

**Keep capture on the Mac only** (0027's position). Rejected: the Mac sleeps,
which has already destroyed a capture, and 0026's mitigation is a caffeinate
wrapper rather than a guarantee. The always-on host is the better recorder.

**Log to a USB stick or SSD on the Pi.** Still the most conservative option, and
worth doing if the write rate ever rises by an order of magnitude. Not justified
at 1.6 KB/s, and it adds a removable dependency to a system that just spent a
day being rebuilt.

**Prune or rotate old logs.** Rejected explicitly by the operator: everything is
recorded and kept. At the measured rate the card holds years, and a capacity
problem is visible long before it is urgent.

## Consequences

- The Pi is again writing continuously to its own card. If a card fails a second
  time, this decision is the first thing to re-examine — but re-examine it with
  a measurement, as here, not an estimate.
- Disk headroom becomes something to watch. `df -h /` is in the provisioning
  script's verify step for that reason.
- `capture.sh` on the Mac is now redundant for routine work, though it remains
  useful for ad-hoc captures and as an independent check when the Pi's own
  record is the thing in question.
- The IR survey car is recorded for the first time by default: `ngr_runlog.py`
  subscribes to `ngr/#`, not the old `ngr/loco/+/#` which silently excluded
  `ngr/spoke/...`. Verified — a `spoke-IR_SPEED_SENSOR` run file appeared within
  seconds of enabling the service.
- 0027 stands as a record of a call made on an unmeasured assumption, and is
  left in place rather than deleted.

## References

- 0027 — superseded by this record
- 0026 — a capture must hold the host awake and timestamp its own silence
- `server/ngr_runlog.py` — the logger; structurally incapable of publishing
- `tools/provision_pi.sh` — installs and enables it on a rebuild
- `docs/PI_REBUILD_RUNBOOK.md` — the rebuild this came out of
