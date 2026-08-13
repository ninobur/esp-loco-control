# 0029 — Clear the supply before diagnosing storage

Status: Accepted (2026-08-13)

## Decision

An SD card is not to be declared faulty until `vcgencmd get_throttled` has been
read **on a freshly booted Pi with nothing attached to USB**, and reads `0x0`.

A non-zero result means the power is suspect and **no storage conclusion may be
drawn at all** — not "bad card", not "worn card", not "bad slot".

Corollary: a clean `throttled` reading is evidence about *that moment*, not a
property of the installation. It expires. Re-read it after any failure rather
than citing an earlier one.

## Context

Two SD cards were destroyed in 36 hours (2026-08-11, 2026-08-13) and both were
diagnosed as faulty hardware. Neither was. A failing 5 V supply was browning out
the Pi mid-write; the cards were timing out because power was interrupted, not
because they were worn or damaged. Replacing the supply fixed it and both cards
work. Evidence:
`field-records/20260813_PI_POWER_SUPPLY_ROOT_CAUSE.md`.

The measurement that settled it took under a minute and was available from the
first hour. Three days of diagnosis went into card replacement, an OS rebuild, a
Mac-side capture rewrite, USB peripheral removal, and a suspected damaged card
socket before anyone booted the machine bare and read the rail.

**The specific trap worth naming.** On 2026-08-12 a rebuild recorded
`throttled=0x0` and concluded *"the undervoltage hypothesis is disproved; the
card was the cause."* The reading was correct. The inference was not: a supply
that degrades over hours reads clean in the afternoon and destroys a filesystem
that night. A point measurement of a *degrading* component cannot disprove that
component.

Two diagnostic signatures worth recognising directly:

- **`mmc0: error -110`** is a *timeout* — the card did not answer. That is
  interrupted power or interrupted connection. Wear and bad blocks produce read
  errors and relocations, which look entirely different.
- **`hwmon: Undervoltage detected!`** preceded each failure. On 2026-08-13 it
  preceded the fatal write by **72 seconds** on one boot and **136 seconds** on
  another. The warning is always there, ahead of time, and nothing was watching.

## Alternatives considered

**Replace the card and move on.** This is what happened twice, and it cost two
cards, an OS rebuild, and three days. It also produced two false records — a
"genuinely bad card" verdict and a "power disproved" verdict — which then misled
the next round of diagnosis. Rejected.

**Monitor undervoltage continuously and alert.** Worthwhile but insufficient on
its own: it does not stop a bad inference being drawn from a stale reading. The
gate above is the cheaper and stricter control. Recording `throttled` alongside
telemetry is now done by `tools/fetch_pi_telemetry.sh` as a complement, not a
replacement.

**Move the write load off SD entirely (USB SSD).** Still worth doing for
endurance, and it would have reduced exposure — a continuously written
filesystem is far likelier to be caught mid-write by a dip than an idle one. But
it treats exposure, not cause: a marginal rail corrupts an SSD too. Deferred and
separable.

## Consequences

- The 2026-08-11 "bad card" verdict is **withdrawn**. That card boots and runs.
- Decision 0027's premise that the card was at fault is superseded in that
  respect. Its conclusion about where logging belongs is unaffected, as are 0026
  (the Mac sleeping) and 0028 (continuous recording).
- `tools/fetch_pi_telemetry.sh` records `throttled`, card serial and mount flags
  beside every nightly pull, and warns when `throttled != 0x0`. The next such
  failure will have its rail state on file before anyone starts guessing.
- **`ngr_runlog.py` still reports healthy while writing nothing.** On 2026-08-13
  it logged `[Errno 30] Read-only file system` per message for fourteen hours
  while `systemctl` showed `active (running)`. It should treat `EROFS` as fatal
  and exit non-zero. Not yet implemented; owed.
- Nothing about the railway, the firmware or QUORUM is affected. No firmware
  change.

## References

- `field-records/20260813_PI_POWER_SUPPLY_ROOT_CAUSE.md` — the evidence
- `tools/fetch_pi_telemetry.sh` — nightly retrieval and health capture
- Decisions 0026, 0027, 0028 — related, and what this does and does not change
