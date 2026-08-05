# esp-loco-control

Firmware for battery-powered garden railway locomotives on the NGR.
ESP32-based, controlled over MQTT from a Raspberry Pi running a Flask app.

Locomotives: **Otto** (9950011) and **Toby** (9950012).

---

## The hard constraint

**Everything built here must integrate with the existing MQTT controller.**

This is not a rewrite and not a replacement. New features are additions to
the working system. If a change would require altering the Pi-side Flask app,
the dashboards, or the way the operator drives the trains today, stop and ask
before proceeding.

Specifically, unless a spec explicitly says otherwise:

- Existing MQTT topic names and payload formats do not change.
- `ESPNOW_VERSION` (14) and `CTO2_VERSION` (3), and the packet structs that
  use them (ESP-NOW `TrainPacket` and `CtoPeerPacket`), do not change.
  Compatibility with the dispatcher and peer locomotives must be preserved.
- Existing LL-Auto behaviour — DNA continuity navigation, the measured-speed
  governor, adaptive baseline, station logic, traffic logic, and CTO2 peer
  coordination — is not modified.
- `dispatcherAuto` semantics do not change. HL-Auto uses its own separate
  flag and never sets `dispatcherAuto`, so it stays invisible to DNA/CTO2.
- E-stop, low-voltage cutoff, and Manual authority always take precedence
  over any automatic mode.

---

## Files

| File | Purpose |
|---|---|
| `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` | Main sketch (~2346 lines) |
| `LL_LocoConfig.h` | Profile selector — edit the include before flashing |
| `LL_LocoConfig_9950011.h` | Otto's profile |
| `LL_LocoConfig_9950012.h` | Toby's profile |
| `SPEC.md` | Highline auto mode (HL-Auto) |
| `SPEC-voltage.md` | PARKED — voltage cleanup already done in r12; note only |

The three operating modes are Manual, LL-Auto (DNA dispatcher navigation on
the Lowline), and HL-Auto (two-magnet stations on the Highline). A loco
switches between them by operator selection at session start — no reflashing.
This no-reflash requirement is why HL-Auto is a mode inside the one firmware
rather than a separate simpler sketch (see SPEC.md decision record).

Per-locomotive values belong in the config headers, not in the sketch.

The operator console is a Flask app (`ngr_app_v1_9_3.py`) on the Pi. It
already has a mode selector (publishes `cmd/auto`) and a MAN/CTO/CE badge.
Highline needs a third mode command, an HL badge, and a conditional pre-auto
gate (CW/CCW + MM required for LL only, not HL). Details in SPEC.md §10.

## Collaboration

This project is worked by David with two AI collaborators, and the lead goes
back and forth depending on the task:

- **David** — operator and engineer. Makes all final operational and design
  decisions, does the field testing, and is the single source of ground
  truth about how the railway actually runs.
- **Claude (Anthropic)** — often drives design and specification alongside
  David, plus code review, analysis, and document production (like this file
  and the specs).
- **Sam (ChatGPT / Codex)** — more QA-focused, and often the one who assigns
  concrete tasks: writes a code change, then hands David something specific
  to fix or verify.

Roles flex; treat this as who tends to do what, not a rigid hierarchy. When
a firmware-architecture choice is significant (like how HL-Auto slots into
the Continuity code), surface it plainly for David so he can weigh it and
cross-check with Sam if he wants — don't quietly decide it and move on.

An earlier planning doc assigned fixed leads ("Sam leads firmware
architecture"); that no longer describes how the work actually flows. This
section supersedes it.

Note: the Module Plan doc (May 2026) and its sketch names
(`LL_PM_Loco_ModuleC_v0_4.ino`, `GoldCore v1.7`) are older than the current
`r12 CONTINUITY_FIRST` firmware. Treat the doc as historical context, not
current state.

---

## Hardware

- Hall sensor GPIO 33 — magnet detection, polarity-sensitive
- Motor PWM GPIO 4, direction GPIO 2
- INA219 on I²C, SDA 21 / SCL 22 — voltage, current, power
- 4S LiPo pack, under 1 A in normal duty cycle
- **Hall sensors are mounted identically on both locomotives** (operator,
  2026-08-04). `HALL_POLARITY_INVERTED` is `true` for Otto and `false` for
  Toby, but that inconsistency is harmless because **no firmware reads the
  symbol** — QUORUM decides polarity purely from which threshold was crossed
  (`evOpenPole = (raw >= northEnter) ? 1 : 0`). Dead config. An earlier
  revision of this file claimed the sensors were mounted in opposite
  orientations; that was wrong and had begun to generate speculation about a
  latent bug that does not exist.
- Pin 33 is the Hall sensor; **pin 34 is the IR wheel sensor**. Both are ADC1.
  Confusing the two is easy and produces a convincing "dead sensor" reading.

---

## How to work in this repo

- **One work item at a time.** Each spec file is a separate change with its
  own commits. Do not combine them. The only active work item right now is
  Highline (`SPEC.md`); `SPEC-voltage.md` is parked.
- **Ask before assuming.** The author is not a professional programmer and is
  learning this workflow. Explain what you are about to change and why,
  before changing it.
- **Flag anomalies, don't silently fix them.** If something in the existing
  code looks wrong, say so and wait. Some of it is deliberate.
- **Preserve the comment headers.** The sketch documents its own version
  history at the top. Keep that convention.

---

## Secrets

The config headers currently contain WiFi credentials in plain text. Do not
commit these to a public repository. Either keep the repo private or move
secrets into a separate header listed in `.gitignore`. (The archived
`NGR-Automated-Train-Control` repo already uses a
`credentials.h` / `credentials_template.h` split behind `.gitignore` — the
same pattern can be reused here.)
