# Firmware catalog

This is the authoritative index of runnable Arduino sketches in this
repository. It answers two separate questions for every artifact:

1. **Role:** is it the production control firmware, a diagnostic instrument,
   or historical reference?
2. **Evidence status:** how far has this particular version been validated?

A narrow or experimental mission does not make QUORUM a test sketch. QUORUM is
the one control-firmware lineage; its capabilities advance through isolated,
reviewed commits. Diagnostic sketches measure a question and never become
production implicitly.

## Roles

| Role | Location | Rule |
|---|---|---|
| **Production control** | `QUORUM/QUORUM.ino` | The sole locomotive-control sketch. Manual and AUTO authority coexist here. |
| **Diagnostic / experiment** | `test-programs/<purpose>/<purpose>.ino` | A purpose-built measuring instrument or prototype. Not normal operating firmware. |
| **Historical reference** | `../archive/` | Superseded artifacts retained for evidence. Never the starting point for new implementation. |

## Evidence statuses

Use these words consistently in reports and this catalog:

- **Development** — being edited; may be uncommitted or unbuilt.
- **Built** — compiles for every applicable profile; no field claim.
- **Reviewed** — source review complete; no field claim.
- **Ready for field test** — built and reviewed with a defined protocol.
- **Field accepted** — passed its stated field gate; the operating baseline.
- **Diagnostic active** — an instrument currently gathering evidence.
- **Reference only** — retained for comparison; do not flash for operation.
- **Superseded** — replaced; history only.

More than one status may be useful (`Built; awaiting review`), but **field
accepted** must never be inferred from “latest,” “current,” a version string,
or a successful build.

## Current catalog

| Artifact | Role | Target and purpose | Evidence status | Controlling record |
|---|---|---|---|---|
| `QUORUM/QUORUM.ino` — 1.8 | Production control | Otto/Toby; Hall baseline adapts only in motion | **Field accepted operating baseline** | decision 0017; QUORUM 1.8 field verdict |
| `QUORUM/QUORUM.ino` — 1.9 | Production control | Adds Otto-only, Arches-only Station Stop v1 mission filter | **Ready for field test; not field accepted** | `docs/CTO3/station-stop-v1/`; commit `ede7a08` |
| `QUORUM/QUORUM.ino` — 1.10 | Production control | Adds console-authority firmware items P11/P13/P14 while retaining 1.8 and 1.9 behavior | **Built and implementation-reported; review/field status pending. THE BASE FOR 1.12** (operator ruling 2026-08-08) | `docs/QUORUM_1_10_IMPLEMENTATION_REPORT.md`; commit `c39c439` |
| `QUORUM/QUORUM.ino` — 1.11 | Production control, **diagnostic stub** | Otto; network-transport instrumentation only ([WIFI] events w/ reasons, 2 s [DIAG] health line, BSSID/channel, queue depths, heap, publish timing, state/netdiag duplicate). No behaviour change over 1.10 | **Diagnostic active — flashed to Otto 2026-08-08 for the link-failure hunt. A STUB: not a base for development; 1.12 branches from 1.10** (operator ruling) | this commit; `FIELD_20260808_INAUGURAL_FINDINGS.md`; CODEX measurement table |
| `test-programs/HALL_DIAG/HALL_DIAG.ino` | Diagnostic | Hall sensor bench diagnosis; serial only | Diagnostic tool | Header in sketch |
| `test-programs/IR_DIAG/IR_DIAG.ino` | Diagnostic | Current QRE1113 wheel-sensor evidence instrument | Diagnostic active | `docs/IR_DEV_REC/`; IR decision records |
| `test-programs/IR_TEST/IR_TEST.ino` | Diagnostic / prototype | Earlier survey-car IR and network test lineage | Retained diagnostic; check current IR development record before use | `docs/IR_SENSOR_NOTES.md` |
| `test-programs/SENSORTEST/SENSORTEST.ino` | Diagnostic | Hall event measurement without a navigation map | Reference diagnostic | Header in sketch |
| `test-programs/Spoke_IR_RSSI_survey/Spoke_IR_RSSI_survey.ino` | Diagnostic | Original two-flag IR/RSSI survey | Superseded by the later IR test lineage | `IR_TEST` header and field records |
| `test-programs/MANUAL/MANUAL.ino` | Historical reference | Structural audit reference for Manual sovereignty | **Reference only; do not flash to an operating locomotive** | decision 0003 |

The QUORUM rows describe immutable points in git history even though the path is
the same. The sketch at repository HEAD may be newer than the field-accepted
operating baseline.

## Promotion rule

Diagnostic results enter production only through this sequence:

1. State the diagnostic question and expected evidence.
2. Capture and preserve the raw result with firmware/commit provenance.
3. Record the conclusion and the production requirement it supports.
4. Implement only that requirement in `QUORUM/QUORUM.ino` as a separate scoped
   change; do not promote the diagnostic sketch wholesale.
5. Build both locomotive profiles when the shared control sketch changes.
6. Review, field-test against a written gate, and update this catalog.

No behavior is production merely because it worked in a diagnostic sketch.
Conversely, a narrow capability inside QUORUM remains part of the production
lineage even while it awaits field acceptance.

## Naming and lifecycle

- Keep the Arduino production filename and folder stable as `QUORUM/QUORUM.ino`.
- Put the human-readable version in `SKETCH_NAME`; identify immutable versions
  by commit and, when flashed, by git tag.
- Name diagnostics for the question or instrument (`IR_DIAG`, `HALL_DIAG`), not
  as apparent QUORUM releases.
- Give each non-trivial diagnostic a short README or a complete sketch header
  stating purpose, hardware, outputs, safety limitations, status, and the
  production decision it may inform.
- Move an artifact to `archive/` only when it is no longer an active tool and its
  replacement or conclusion is recorded. Do not delete the evidence trail.
- Never keep a second editable copy of QUORUM under a feature name such as
  `ONE_STATION.ino`.

## Librarian check for firmware changes

Before a firmware-related commit is considered complete:

- classify every new sketch by role;
- state its evidence status without overstating it;
- link its controlling spec, decision, report, or field record;
- confirm whether anything was promoted into QUORUM;
- update this catalog when status, replacement, or promotion changes;
- preserve unrelated and untracked work; never use `git add -A` for a scoped
  firmware commit.

Decision 0018 governs this catalog. Decision 0003 continues to govern the
one-control-sketch rule.
