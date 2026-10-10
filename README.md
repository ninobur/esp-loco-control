# Ninobur Garden Railway — start here

## Current NAVI sketch

**[NAVI_EWO_0_1_MM045_STOP](firmware/programs/NAVI_EWO_0_1_MM045_STOP/)**
is the current MM045 STOP review candidate on this branch.

Open **[NAVI_EWO_0_1_MM045_STOP.ino](firmware/programs/NAVI_EWO_0_1_MM045_STOP/NAVI_EWO_0_1_MM045_STOP.ino)**
in Arduino IDE. The folder, filename and name reported by the locomotive match.

| Program | Purpose | Selected locomotive | Status |
|---|---|---|---|
| **NAVI_EWO_0_1_MM045_STOP** | Repeating stop near MM045 from either direction; five-second dwell and restart; cruise 90 with grades retained | Otto 9950011 | Host-tested and ESP32-built; prior source independently reviewed. Restart corrected to 150 ms/count; first CW trials reported on the preceding version. Track calibration pending; not field accepted. |

The [sketch README](firmware/programs/NAVI_EWO_0_1_MM045_STOP/README.md) explains
the trial and build requirements. The [implementation report](docs/NAVI_MM045_STOP_IMPLEMENTATION_20261009.md)
links the governing architectural decisions, validation and rollback points.

## Find a program or record

| What you need | Open |
|---|---|
| Current programs and diagnostic tools | [Firmware index](firmware/README.md) |
| Older builds, deployment records and previous names | [Firmware history](firmware/HISTORY.md) |
| Non-runnable reference packages | [Firmware reference](firmware/reference/) |
| Superseded source retained for evidence | [Archive](archive/) |
| Architectural decisions and specifications | [Documents](docs/) |
| Railway observations and logs | [Field records](field-records/) |
| Raspberry Pi dashboard and services | [Server](server/) |
| Analysis and test utilities | [Tools](tools/) |

## Where to find this checkout on David's Mac

The visible review checkout is **`/Users/davidbrown/esp-loco-control/CURRENT`**.
In Finder: **davidbrown → esp-loco-control → CURRENT → firmware → programs →
NAVI_EWO_0_1_MM045_STOP**.

`CURRENT` contains a complete checkout, including the shared headers needed to
build. The enclosing `esp-loco-control` checkout contains other work in progress;
its existing edits and branch are preserved. Use `CURRENT` for this candidate.
Do not copy just the `.ino` out of the repository.

On GitHub this candidate is on **`codex/navi-cruise-stop-20261009`**, in
[PR #9](https://github.com/ninobur/esp-loco-control/pull/9). It is not merged into
the base branch. A different branch can legitimately contain older programs.

## Naming and handoff rules

- For a new or explicitly renamed sketch, use the same program identity for the
  directory, primary `.ino` basename and runtime `SKETCH_NAME`.
- Name a distinct trial for its purpose, as with `NAVI_EWO_0_1_MM045_STOP`.
  Use commits to distinguish revisions of that trial; do not duplicate editable
  sketches for each small change.
- Keep the current-program list short. Put superseded descriptions in history,
  retaining the source commit and field evidence. Historical naming exceptions
  remain visible in the history; they are not silently renamed en masse.
- Each handoff must identify the exact sketch, a usable visible local path,
  branch/commit, selected locomotive and evidence status. A build or code review
  does not establish physical field acceptance.

## Project authority

[AGENTS.md](AGENTS.md) governs agent work. Read the
[decision-system principles](docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md),
[NAVI decision model](docs/NAVI_DECISION_MODEL.md), and
[NAVI_EWO governing index](docs/NAVI_EWO_GOVERNING_DOCUMENTS.md) before proposing
navigation changes. For this candidate, the implementation report links the
October 9 governing guide and David's final STOP decisions.
