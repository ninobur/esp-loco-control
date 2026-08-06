# 0007 — NVS persists only a pulse-proven envelope snapshot

Status: Accepted  (2026-08-06, CODEX finding; recorded same day)

## Decision
The envelope written to NVS is `provenMin/provenMax` — the bounds as they
stood at the last completed, filter-fed pulse. The write is authorized
only at an edge-silence whose prior state was VALID with a flat trace (or
a witness-verified STOPPED). Authorization and content are separate
decisions: the flat test decides *whether*, the proven snapshot decides
*what*.

## Context
The first design saved the live envelope on every timeout — persisting the
very envelope that caused a blind episode and restoring the failure across
reboots. The second gated the save on a flat trace but still wrote the
live pair; a sensor gone stuck, saturated, disconnected or sun-blinded is
also flat, and unconditional expansion means the live pair may already
contain the extreme of whatever ended the run.

## Alternatives considered
- No persistence — rejected earlier: the car boots blind and the pulses it
  needs to become sighted are the ones it cannot see.
- Flat-trace gate alone — rejected (CODEX): authorizes persisting the
  corruption.

## Consequences
A system must never persist the state that broke it. A boot with no proven
pulse writes nothing. The pattern generalises: recovery data is captured
at moments of demonstrated health, not at moments of failure.

## References
Commits `dcf3936`, `39842d7`; `IR_DIAG_DAYLIGHT_PREP.md` addendum.
