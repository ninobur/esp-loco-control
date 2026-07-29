# STATUS — esp-loco-control

**Last updated:** 2026-07-24
**Purpose:** Single catch-up file. Read this first to know where the project
stands, what's decided, and what's next. Update this file in place — do not
create STATUS2 / STATUS-final. One STATUS, always current; git keeps history.

---

## 1. What this project is

ESP32 firmware for two battery-powered garden-railway locomotives — **Otto
(9950011)** and **Toby (9950012)** — controlled over MQTT from a Raspberry Pi
running a Flask console. The current work is adding a new **Highline auto
mode (HL-Auto)** to the existing locomotive firmware.

Three operating modes: **Manual**, **LL-Auto** (Lowline DNA navigation, the
complex existing auto mode), and **HL-Auto** (Highline, new, simple). A loco
switches between them by operator selection at session start — **no
reflashing**. That no-reflash requirement is why HL-Auto is a mode *inside*
the one firmware, not a separate sketch.

Full design is in `SPEC.md`. This file is the status layer on top of it.

---

## 2. Ground truth — which file is which

The project has a history of confusing, version-in-the-name filenames. This
is the authoritative mapping as of the last update:

| File | What it actually is |
|---|---|
| `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` | **CURRENT locomotive firmware.** HL-Auto goes into this. ~2346 lines. ESPNOW_VERSION 14, CTO2_VERSION 3. |
| `LocoConfig.h` | Profile selector — edit the include to pick the active loco before flashing. |
| `LL_LocoConfig_9950011.h` | Otto's profile. |
| `LL_LocoConfig_9950012.h` | Toby's profile. |
| `reference/LL_MQ_CO_GC_DI_1_2.ino` | Dispatcher firmware (trackside GO/STOP unit). NOT modified for HL-Auto. Kept for reference only. |
| `reference/LL_LocoConfig_Dispatcher.h` | Dispatcher config. Reference only. |
| `SPEC.md` | Highline (HL-Auto) design. Active work item. Draft v3. |
| `SPEC-voltage.md` | PARKED — voltage cleanup already done in r12. Not current work. |
| `CLAUDE.md` / `AGENTS.md` | Shared context for AI agents. AGENTS.md points to CLAUDE.md. |

Renaming these to clean, stable names is planned — but only **after** the git
repo exists (see §6), so history isn't lost in the rename.

---

## 3. HL-Auto design (summary — full detail in SPEC.md)

- Operator selects HL-Auto at session start (loco dashboard).
- Dispatcher GO → ramp to cruise **PWM 70**.
- **North** magnet → slow to station speed **PWM 40**, arm the approach.
- **South** magnet → ramp to 0, **dwell 10 s**, depart back to cruise.
- Unarmed South is ignored (run through). Armed approach with no South
  disarms after **30 s** and returns to cruise.
- Single loco, forward only, no Highline traffic coordination.

**Core architecture:** a separate `highlineAuto` flag. HL-Auto must NEVER set
or reuse `dispatcherAuto` — doing so would wrongly enroll the loco in DNA /
CTO2 / governor behaviour. Only shared motor-authority and safety paths
recognise both auto modes. DNA nav, CTO2 packets/versions, MQTT, E-stop,
low-voltage protection, and Manual authority all stay intact.

---

## 4. Spec-vs-code evaluation — DONE, with two corrections

A read-only compatibility review was run in Claude Code against the actual
r12 sketch. Result: **the architecture is sound.** The `highlineAuto` flag
coexists cleanly; all reads of `dispatcherAuto` classify unambiguously into
"motor authority" (should also see HL-Auto) vs "DNA/CTO2 session" (must not).

Two findings **correct the spec** and must be honoured when code is written:

1. **`HALL_POLARITY_INVERTED` is defined but never used in the sketch.** The
   spec assumed the firmware already flips polarity per-loco using this flag.
   It does not — the symbol appears in both config headers but is referenced
   nowhere in the code. Therefore the new `hlAutoOnMagnet()` must apply the
   inversion *itself* (`#if HALL_POLARITY_INVERTED`) before treating
   North=approach / South=stop. This is the one genuine spec-vs-code gap.

2. **`updateMotorAuthority()` needs no change.** The spec said it must be
   extended to grant authority under HL-Auto. In fact it already grants
   authority unconditionally (only E-stop and neutral zero it), so HL-Auto
   works through it unmodified. One less edit than the spec implied.

**Exact edit points the review identified** (r12 line numbers, for whoever
implements):

- Attach HL logic in `onMagnetEvent()` right after the `if(!dispatcherAuto)`
  return (~line 2016): `if(highlineAuto){ hlAutoOnMagnet(hallPol); return; }`.
- Motor-authority reads that need `|| highlineAuto`: the manual-override
  blocks at ~2235–2237 (throttle/direction/brake), and the ESP-NOW GO/STOP
  handlers at ~2317 / ~2325 need an HL branch.
- New HL ramp state (`highlineFinalRampActive`, `highlineRampUpActive`) hooks
  into `serviceRamp()` (~1597–1623), parallel to the LL station-ramp flags.
- Arm-timeout and dwell timers are new state owned entirely by the HL module.

> ACTION: fold corrections 1 and 2 into SPEC.md so the spec matches reality
> before any code is written.

---

## 5. Three open questions — still to decide (with David + Sam)

These are the last design decisions before implementation. The eval mapped
what each touches:

1. **Mode command shape.** Recommended: a dedicated `cmd/hlauto` MQTT topic,
   parallel to the existing `cmd/auto`, leaving `cmd/auto` untouched. (A
   unified `cmd/mode` value would couple into the existing handler — avoid.)
2. **State reporting.** A new `state/hlauto` topic the dashboard subscribes
   to for the HL badge. `state/auto` keeps reporting `dispatcherAuto` only.
3. **Ramp-up rate `HIGHLINE_RAMP_UP_RATE_MS`.** Needs a value. Note: reusing
   300 ms/step (as LL station ramp does) makes a cruise-70→0 stop take ~21 s;
   confirm that's physically acceptable or tune before committing.

---

## 6. What's next — priority order

1. **`STATUS.md` in the folder.** (This file. Done when saved.)
2. **Get `esp-loco-control` onto GitHub — Option B.** Highest priority,
   ahead of writing code. Every day without the repo adds to the
   which-file-is-latest confusion. Plan:
   - Init git in the folder.
   - Write `.gitignore` **first**, before any commit, excluding the real
     credential headers (they contain the WiFi password in plaintext).
   - Reuse the `credentials.h` / `credentials_template.h` split pattern from
     the archived `NGR-Automated-Train-Control` repo so structure travels
     without secrets.
   - First commit, then push to a new **private** GitHub repo.
   - Leave the old `NGR-Automated-Train-Control` repo alone as a
     Blynk-era archive. Do not merge it in.
   - Do this together, one step at a time — not via an agent.
3. **Clean up the filenames** — only *after* git exists, so history holds the
   old→new mapping. Mind the Arduino rule that a sketch's `.ino` must sit in a
   folder of the same name.
4. **Decide the three open questions** (§5), with David and Sam.
5. **Fold the eval corrections into SPEC.md** (§4).
6. **Then let an agent implement HL-Auto** against the corrected spec.

---

## 7. Environment / tooling

- Editor: VS Code, folder `~/esp-loco-control`.
- Agents working: **Claude Code** (CLI, v2.1.143) and **Codex** (VS Code
  extension). Both read this folder and the shared `CLAUDE.md` / `AGENTS.md`.
- Not yet on GitHub (see §6).
- **Agents don't see each other's work unless it lands in a file.** Reports
  and decisions must be saved here (or into the specs) to persist across
  sessions — that's what this file is for.

---

## 8. Collaboration

David decides; Claude and Sam (ChatGPT / Codex) trade off design and QA;
Sam often assigns concrete tasks and reviews. Roles flex — see `CLAUDE.md`.

Addendum: 

| `ngr_app_v1_9_3.py` | **Flask operator console** (runs on the Pi). Was overlooked in the initial folder set, now added. NOTE: a later revision deleted some desired features — this is known, not a bug. Do not auto-restore or "fix" against SPEC §10; the current state is ground truth. HL-Auto dashboard changes (SPEC §10) are future work, not a regression to repair. |
