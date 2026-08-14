# 0034 — v1.14 CTO wire and membership contracts

Status: **Proposed** (2026-08-13). These are the implementation choices the
Bubble v1 spec left open (findings 3 and 4); QUORUM_1_14 is built on them.
They bind nothing until the operator and CODEX accept them — the sketch is
unflashed.

## Decision (proposed)

**Role echo carrier** — a new ESP-NOW packet type, not a reuse:

| | |
|---|---|
| magic / version | `0xC5` / 1 (`Cto3RoleEcho`) |
| fields | senderId, role, partnerId, pairEpochMs |
| cadence | 1 Hz |
| freshness | echo older than 6 s ⇒ UNCONFIRMED |
| mixed-version | r12-era receivers drop it on magic mismatch |

**Revised per CODEX review, 2026-08-13 (finding 5 — the original "absence of
echo never blocks" is withdrawn as unsafe).** The echo is three-valued and
**CONFIRMED is the only state that permits formed-bubble choreography**:

- **CONFIRMED** — fresh echo, *opposite* role, `partnerId == me`. Release
  choreography and the follower's 20 s dwell run.
- **CONFLICT** — fresh echo claiming *my* role with `partnerId == me`. Both
  stop, alert, hold until the echoes agree.
- **UNCONFIRMED** — everything else: stale, absent, role NONE, wrong partner,
  or an older peer that cannot echo. Universal traffic protection stays fully
  active; a paired leader holds at the platform indefinitely and a paired
  follower runs solo dwell rules. A mixed-version bubble therefore degrades
  to operator-supervised running, never to unconfirmed automation.

The frozen `CtoPeerPacket` v3 is transmitted unchanged, field-for-field;
fields QUORUM cannot honestly fill (speedX10, speedValid, lastMoveAgeDs) are
zeroed, never guessed. `TrainPacket.roleId` is NOT reused — it lives in a
command packet, and the audit marked it discard.

**Fleet membership (0031's open item)** — v1 lifecycle:

- **Enrolled:** the first fresh peer packet seen this boot sets
  `ctoExpectedId`. One expected peer in v1 — this is a two-train spec.
- **Armed:** from that moment. The fleet stop fires when the expected peer is
  stale (> 3 s) or reports no position, and clears itself when the peer
  returns fresh and navigating — with no autonomous surge back to speed; the
  station machine's own cruise path restores speed through the limiter.
- **Cleared:** `cmd/cto "clear"` (empties the registry, dissolves the pair,
  disarms), `cmd/cto "off"`, or power cycle. Solo running is valid until a
  peer is heard.

**Consequence accepted knowingly:** a locomotive deliberately removed from
the railway leaves the survivor fleet-held until the operator clears it —
the LBO `CMD_CLEAR_ALL` shape, chosen because the alternative (timeout-based
forgetting) is exactly the silence-means-clear inference decision 0031
forbids.

## Alternatives considered

**Echo inside `CtoPeerPacket`.** No spare field; the struct and version are
frozen (`docs/CLAUDE.md`).

**Reuse `TrainPacket.roleId`.** Rejected: a role statement in a command
packet invites exactly the authority reading 0032 forbids, and the packet's
semantics are dispatcher-era.

**Membership by dispatcher roster.** Cleaner lifecycle, but it puts the
dispatcher inside the safety loop, against governing intent #1 (intelligence
aboard). Revisit if multi-train (>2) operations arrive.

**Membership forgotten on staleness.** Rejected outright — silence would
clear the obligation, which is the inference 0031 exists to forbid.

## References

- `docs/CTO3/BUBBLE_V1_SPEC.md` §§2.4, 4 — the obligations this discharges
- Decisions 0031, 0032; `docs/CTO2_AUDIT_DISPOSITION.md`
- `firmware/QUORUM/QUORUM.ino` LAYER 5 — the implementation
