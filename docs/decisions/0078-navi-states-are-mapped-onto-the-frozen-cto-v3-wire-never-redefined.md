# 0078 — NAVI states are mapped onto the frozen CTO v3 wire, never redefined

**Date:** 2026-09-10
**Status:** PROPOSED. Not authoritative until the operator reviews and approves
it. Drafting it allocates it no authority whatsoever, and nothing in it
authorizes an agent to act.
**Follows:** 0021 (a field that cannot be honestly filled is zeroed, not
guessed), 0039 (the repeater decodes these structures today), 0077 (the CTO
authority boundary).
**Evidence:** `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §8. Wire
structs at `firmware/QUORUM/QUORUM.ino:556-587`. NAVI enums at
`firmware/test-programs/NAVI_ONE/Stations.h:184` and `Navigator.h:74,110,158`.
**Builds:** none.

---

## Decision

`CtoPeerPacket` (CTO2_VERSION 3, magic 0xC4) and `Cto3RoleEcho` (0xC5 v1) are
copied field-for-field and byte-for-byte. Field order, types, packing and
`sizeof` are unchanged. **No wire change is proposed.** If one becomes genuinely
unavoidable, work stops and the compatibility problem is presented before
anything is implemented.

Three fields carry QUORUM semantics that NAVI maps onto rather than redefines:

| field | wire meaning (frozen) | NAVI source | mapping |
|---|---|---|---|
| `stationPhase` | `ST_IDLE, ST_APPROACH, ST_FINAL, ST_RAMP, ST_DWELL, ST_DEPART` = 0..5 | `StPhase::Idle, Approach, Zone, Ramp, Dwell, Depart` = 0..5 | ordinals coincide 1:1, `Zone` ↔ `ST_FINAL` |
| `truthSource` | 0 none/lost, 1 declared/evaluating, 2 confirmed | `Trust` + `positionKnown()` | `Proven` → 2; `Declared` → 1; `Contradicted`, `Unset`, `Struck` or `!positionKnown()` → 0 |
| `mustHoldEligible` | always 0 in 1.14 | — | stays 0 |

Fields NAVI cannot honestly fill are zeroed, never guessed: `speedX10` and
`speedValid` stay 0 until a speed instrument is aboard and proven.

## Context

The dispatcher and the Grillers Repeater decode these structures today, and
`docs/CLAUDE.md` freezes them. A silent semantic drift in a field whose bytes
still parse is far more dangerous than a struct that fails to parse, because
every consumer keeps working and reports something subtly wrong.

## Alternatives considered

- **Extend the struct with NAVI-native fields.** Rejected: it breaks every
  existing decoder, and the freeze exists precisely to prevent that.
- **Reuse `speedX10` for the estimated speed NAVI already computes.** Rejected:
  `est_mm_s` is a model output, not a measurement. Filling a measurement field
  with a model is the "labelled lie" this log was written to prevent.
- **Assume the phase ordinals will stay aligned.** Rejected — see below.

## Consequences, including the unwelcome ones

- **The `stationPhase` alignment is a coincidence, not a design.** Two
  independently authored enums happen to agree on 0..5. Nothing prevents a
  future edit to either from silently breaking it, and the breakage would be
  invisible: valid bytes, wrong meaning, peers acting on a phase the sender is
  not in. It must be held by `static_assert` and by test, and never assumed.
- Zeroing `speedX10` means peers cannot use speed for spacing and must fall
  back on position and conservative assumptions. That is a real capability cost
  accepted deliberately.
- This record makes the wire harder to evolve. That is the intent, and it will
  be inconvenient the first time a genuinely useful field is wanted.

## References

- `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §8
- `firmware/QUORUM/QUORUM.ino:556-587`
- `firmware/test-programs/NAVI_ONE/Stations.h:184`, `Navigator.h:74,110,158`

## Review

Unreviewed. Operator has not ruled.
