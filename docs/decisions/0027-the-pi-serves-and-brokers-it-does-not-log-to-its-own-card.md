# 0027 — The Pi serves and brokers; it does not log to its own card

Status: Accepted  (2026-08-12)

## Decision

The Raspberry Pi's role is the Flask dashboard and the mosquitto broker. It does
not run a continuous telemetry logger writing to its own SD card.

`ngr_runlog.py` and `ngr-runlog.service` are kept in the repository but are
**not** enabled on the rebuilt host. Capture stays on the Mac, where commit
60ec7ea put it.

Second, and separately: the Pi's *configuration* is repository state. Any unit
file, broker setting or provisioning step added to the Pi is committed to
`server/` or `tools/` at the same time, not left resident on the card.

## Context

The SD card failed across 2026-08-11/12 — EXT4 I/O errors and a read-only
remount mid-session, then boots that lasted minutes, then no boot at all. Both
failures happened while running trains.

Two things became visible while rebuilding. The first is that continuous
telemetry logging to flash is a wear load pointed straight at the component that
failed, and it had already caused a silent evidence gap when the subscribers
writing to the card died at 18:11:51 and fifty minutes of running went
unrecorded.

The second is that the repository could not rebuild the host it depends on. The
app source was committed, but the `ngr-app` systemd unit, the mosquitto
configuration, and the provisioning sequence existed only on the failing card.
There was no runbook. The unit file had to be reconstructed from
`ngr-runlog.service` and is marked unverified because the original is gone.

## Alternatives considered

**Redeploy `ngr_runlog.py` to the new card as originally planned** (STATUS.md §9
item 1). Rejected: it was written before capture moved to the Mac, and it aims
exactly the write load that kills cards at the new card. The evidence need it
was meant to serve is now met by `capture.sh`.

**Log to a USB stick or SSD on the Pi.** Viable, and better than the card, but it
adds hardware to a system whose current fault is not yet isolated, and the Mac
already captures with reconnect logging. Not needed.

**Leave provisioning undocumented and rebuild from memory again next time.**
This is what happened, and it cost a rebuild done from reconstruction rather
than from record.

## Consequences

- The Pi becomes closer to disposable: a failed card is a reflash plus
  `tools/provision_pi.sh`, not an archaeology exercise.
- Evidence capture depends on the Mac being awake — already recorded as a
  hazard, and already addressed, in 0026.
- `ngr-runlog.service` stays in the tree unenabled. Anyone reading it should see
  this record before deploying it.
- `server/ngr-app.service` is a reconstruction. If the original is ever
  recovered from the failed card, it should be diffed against this one.
- The root cause is not yet established. The card is the proximate failure, but
  a marginal power supply produces the same signature, and both failures came
  under load. Until `vcgencmd get_throttled` is checked on the rebuilt host,
  "the card was bad" remains a hypothesis rather than a verdict.

## References

- `docs/PI_REBUILD_RUNBOOK.md` — the rebuild procedure and what was lost
- `tools/provision_pi.sh` — the provisioning it replaces
- `server/ngr-app.service` — reconstructed unit
- commit 60ec7ea — capture moved to the Mac, "the Pi card can no longer be trusted"
- 0026 — a capture must hold the host awake and timestamp its own silence
- `firmware/QUORUM/QUORUM.ino:370` — broker address compiled into the locomotives
