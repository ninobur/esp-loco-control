# Prompt: restore two-train operation (CTO) on the NAVI_ONE navigator

Written 2026-09-04 evening to open a new chat. Paste from the line below.

---

You are working in `/Users/davidbrown/esp-loco-control` on branch
`agent/toby-1-13-flash` (verify with `git branch --show-current` before every
commit; other sessions use `agent/*` branches and share the index, so stage
and commit in one step). Read `docs/CLAUDE.md` and the memory index first.

**Operational goal, kept explicit throughout:** two locomotives, Otto
(9950011) and Toby (9950012), run the Lowline together, coordinating their
own spacing, completing station stops, dwells and departures, with correct
position and no collisions and no unnecessary shutdowns.

**Where the project stands tonight.** Toby navigates on the NAVI_ONE lineage
(recorder image `NAVI_ONE_STATION_CURVES_0_3`, source in
`firmware/programs/NAVI_ONE/variants/NAVI_ONE_STATION_CURVES/`, same recognizer and
capture as `firmware/programs/NAVI_ONE/variants/NAVI_ONE/` at 1.0X13). On 2026-09-04 it
ran two hours in both directions, 3,284 magnets, 76 station stops, zero
false positions, zero shutdowns, zero transport loss
(`docs/NAVI_ONE_X13_FIELD_VERDICT_20260904.md`). The recognizer and capture
are frozen: do not modify them without new field evidence. Decisions 0074
(shape is diagnostic), 0075 (discard fix) and 0076 (navigation recovers from
one wrong observation, not yet built) are PROPOSED and await the operator.

Two-train coordination exists and worked: QUORUM 1.16R, LAYER 5 (CTO3), the
Bubble v1 spec, decisions 0030–0034, field-tested 2026-08-15
(`field-records/20260815_QUORUM_1_16R_BUBBLE_TEST.md`). It was set aside
because the QUORUM navigator did not reliably know where the trains were.
Otto is currently on QUORUM 1.12C with no CTO. NAVI_ONE has an Otto profile
(`LL_LocoConfig_9950011.h`) that has never been flashed, and no ESP-NOW.

**The job.** Put QUORUM's coordination layer under the navigator that now
knows where it is, in this order, one behavioural change per field build:

1. **Otto on NAVI_ONE, alone.** Build recorder 0.3 for Otto's profile,
   confirm the boot line names 9950011, hand the operator the image. He
   flashes. Acceptance: a session of laps and station stops with no false
   advance and no shutdown, measured from the telemetry mirror. Otto's
   detector was always noisier than Toby's; NAVI_ONE has never seen his
   signals. CTO cannot be safer than the position reports it consumes.
2. **Peer radio into NAVI_ONE, transmit and receive truth only.** Carry
   over the frozen `CtoPeerPacket` v3 and `Cto3RoleEcho` exactly, so the
   wire stays compatible with 1.16R and the dispatcher. No speed cap, no
   roles, no fleet stop. Acceptance is a link measurement over full laps
   with both trains running: packet rate, gap distribution, channel, plus
   the archive traffic sharing the radio (decision 0072 requires this to
   be measured, not assumed).
3. **The bubble.** Roles, follower cap, leader hold, fleet stop, per
   0030–0034, ported without redesign. Supervised first run with E-stops.
   Acceptance: the M7 crossing test in `docs/ROAD_TO_CTO.md`, ten laps,
   two trains, zero collisions, zero interventions.
4. **Console findings** from the bubble test: no forced re-declaration
   when the navigator is healthy, no stale interval pre-filled, a warning
   when a declaration contradicts a healthy navigator. Pi-side; propose,
   the operator deploys.

**Before any behavioural change, answer in writing:** what observed failure
it fixes, which existing rule caused it, the smallest change, what can be
removed or demoted, and what field result proves it. Every field build
states its single behavioural difference, everything unchanged, the
observed failure addressed, its acceptance test, its rollback image, and
the condition that ends the experiment.

**Standing rules.** The operator drives and flashes; nothing is flashed
without his explicit approval. Never open the serial port; read only the
sanctioned mirror via `~/ngr-telemetry/bin/fetch_pi_telemetry.sh` into
`~/ngr-telemetry/pi/NGR/telemetry/all_YYYYMMDD.log`. The Pi is his; propose,
do not deploy. Decision records in `docs/decisions/` carry no force until he
ratifies them; never cite one as licence to act. Field evidence outranks
code reading. Commit implementation reports and field verdicts to `docs/`
unprompted. Keep chat replies short and decision-oriented. Experimental
builds name themselves `*_FIELDTEST` and keep a rollback. Use
`/usr/bin/grep`, absolute paths, and compile with `arduino-cli` (libraries
are installed; use `--build-path` in the scratchpad, never the operator's
build directory). Commit attribution: `Co-Authored-By: Claude Fable 5.1
<noreply@anthropic.com>`.

**Start with step 1.** Report what Otto's profile differs in from Toby's,
build the image, and give the boot line to verify. Do not start step 2 until
step 1 has a field verdict.
