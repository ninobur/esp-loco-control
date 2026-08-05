# STATUS — esp-loco-control

**Last updated:** 2026-08-04
**Purpose:** Single catch-up file. Read this first to know where the project
stands, what's decided, and what's next. Update this file in place — do not
create STATUS2 / STATUS-final. One STATUS, always current; git keeps history.

> The previous revision of this file was dated 2026-07-24 and described a
> project that no longer exists: it named `r12_CONTINUITY_FIRST` as the
> current firmware, listed "get onto GitHub" as the top priority, and treated
> HL-Auto as the active work item. All three have been overtaken. This is a
> full rewrite, not an amendment.

---

## 1. Where the project stands, in one paragraph

The locomotive firmware was rewritten twice since the last status. The DNA
tally navigator (r12 → SOLONAV 1.x → 2.x) was replaced by **QUORUM**, which
holds position on a disagreement instead of discarding it, and which is now at
**1.4 and formally cleared for the M1 field campaign** after four CODEX review
rounds. The Flask console was rebuilt three times against three separate field
failures and is at **v1.10.2**. The repository exists, is private, has 44 MB of
committed field evidence, and every implementation report and review finding
now lands in `docs/` as a matter of standing practice. The active work item is
**M1 — take QUORUM to the field** — not HL-Auto.

---

## 2. Ground truth — which file is which

| File | What it actually is |
|---|---|
| `firmware/QUORUM/QUORUM.ino` | **AUTO-capable firmware.** QUORUM_1_5. Booted clean on Otto 2026-08-04; untagged. |
| `firmware/MANUAL/MANUAL.ino` | **Manual-only firmware.** MANUAL_1_1 — QUORUM's navigator with the AUTO chamber removed rather than gated. Not yet flashed. |
| `firmware/QUORUM/LocoConfig.h` | Profile selector — edit the include before flashing. Committed state selects **Otto (9950011)**. |
| `firmware/QUORUM/LL_LocoConfig_995001{1,2}.h` | Otto's and Toby's profiles. |
| `firmware/test-programs/SENSORTEST/` | Marker measurement rig. Standalone, own WiFi/MQTT. |
| `firmware/test-programs/Spoke_IR_RSSI_survey/` | IR survey car **as flown**. Retained as the record; do not flash. |
| `firmware/test-programs/Spoke_IR_RSSI_survey_v2/` | IR survey car, corrected. Not yet flown. |
| `server/ngr_app_v1_9_5.py` | **RUNNING ON THE PI** — reverted to 2026-08-04 20:21 after v1.10.4 was found unreliable (throttle unusable). Verified byte-identical to the live file. |
| `server/ngr_app_v1_10_6.py` | Newest console in the repo. NOT deployed, not field-tested. Adds a fault banner and one-in-flight throttle publishing; the v1.10.4 throttle failure is still unreproduced. |
| `server/ngr_runlog.py` | Per-run MQTT telemetry logger. Never publishes, by construction. **Not yet deployed to the Pi — see §9.** |
| `field-records/` | Committed field evidence: logs, cal recordings, verdicts. See its README. |
| `docs/QUORUM_v3_0_implementation_spec.md` | The QUORUM contract. **Revision 21.** Body frozen; amendments are changelogged. |
| `archive/` | Superseded sketches, including `r12_CONTINUITY_FIRST`. Historical only. |

**Naming is settled.** Sketch folders and `.ino` names match (Arduino
requires it), no spaces, no version in the filename — versions are
`SKETCH_NAME` plus git. `SOLONAV` was renamed to `QUORUM` when the navigator
was replaced, because the name is the navigator's, not the sketch's.

Tags: `v2.16`, `v2.17`, `v2.19`, `v2.21`, `v2.22`. **QUORUM 1.0–1.4 are
deliberately untagged** — a tag means flown, and QUORUM has not flown.

---

## 3. QUORUM — what it is and where it got to

Replaces the tally navigator. `navConfidence` could express *how much am I
disagreeing* but not *which position might I be in*, so when it emptied it
discarded position and rebuilt from nothing.

> *"I am on the tracks. I am not flying. I knew where I was a minute ago."*

- One disagreement is free; the odometer advances but position is **held**.
- Three consecutive misses wake `NAV_EVALUATING`: six candidate offsets
  `{-1,0,+1,+2,+3,+4}` scored against an evidence ring. **Speed is not
  reduced while evaluating.**
- A unique two-point lead adopts the offset — one correction, applied once,
  validated by the next agreement.
- Twelve readings without a margin, or a second failed adoption, is
  `NAV_NO_QUORUM`: controlled stop, terminal evidence snapshot, operator
  re-declares. No automatic exit.
- A conservation timing gate rejects phantoms: two events whose intervals sum
  to one expected interval are one magnet read twice.

**Version history and why each exists:**

| | |
|---|---|
| 1.0 | Implementation of spec R20. Twenty certified navigator properties, confirmed by CODEX against source. |
| 1.1 | CODEX round 1 — four High findings, all terminal-evidence: snapshot offsets vs indices, tear-free cross-core handoff, reconnect reconciliation, alert sizing. Plus durable AGREE/DISAGREE. |
| 1.2 | CODEX round 2 — an ABA race in the reconciliation completion guard. Generation counter. |
| 1.3 | **Operator constitutional ruling: bicameral control.** See §4. |
| 1.4 | CODEX round 3 — M+1 fallback reclassified as AUTO-chamber automation and explicitly gated; terminal log made honest in both chambers. |

Flash/RAM has stayed at **72% / 15%** throughout, against the v2.22 baseline.

**CODEX verdict after 1.4's predecessor:** *"Add the retained-state generation
check, then it is ready for the §8 field/replay campaign."* That check is 1.2;
1.3 and 1.4 are the bicameral work on top. The firmware is cleared.

---

## 4. The bicameral doctrine — constitutional, spec §0.2

Operator ruling, 2026-08-02, now normative for this and all future navigators:

> All NGR locomotive controllers are **bicameral**. Two chambers: **MANUAL** —
> the operator is sovereign; navigation observes, records, publishes and warns,
> but **never** writes to the motor. **AUTO** — navigation acts with full
> authority. **E-STOP** belongs to the operator and works in every chamber and
> every state.

v2.22 stated this in its LOST handler and honoured it. QUORUM 1.0–1.2
regressed it exactly once: the `NAV_NO_QUORUM` terminal stop called
`requestPwm(0)` unconditionally — a navigation override of a manual operator.
**The certified property "NAV_NO_QUORUM stops once" was reviewed by everyone
and gated by no one.** That is the lesson worth keeping: a property can be
correct as specified and still be wrong, if nobody asked which chamber it
belonged to.

A full audit of every motor-write call site was run before the fix (table in
`QUORUM_1_3_IMPLEMENTATION_REPORT.md`, amended in `1_4`). Seventeen sites; one
stray. Class (c) — "motor-safety facts exempt from gating" — **is now empty**:
CODEX reclassified its only member, the M+1 station fallback, as AUTO-chamber
automation, and it is explicitly gated in 1.4. Every navigation-originated
motor write is lexically gated on `autoRunning`; operator paths are never
gated; the E-stop direct write is untouched.

---

## 5. The dashboard — three field failures, three revisions

The console displayed stale data as if it were live, and it cost field
sessions. Each revision answers a specific documented failure.

**v1.10.0 — "renders only what the locomotive has said, timestamped."**
Root cause of the stale tiles was **retained MQTT ghosts**: the broker still
holds `telem/voltage 15.40`, `telem/power 1.79`, `mm/speed pkph 12.73` from
firmware that no longer publishes those topics, and every reconnect replayed
them as fresh. Retained deliveries are now dropped for state and every value
carries its arrival age. Also fixed a double-publish (slider sent on both
debounced `oninput` and `onchange`) and an E-STOP that was gated in AUTO.

**v1.10.1 — gates enforce startup order only.** At 16:08:03 on 2026-08-01 the
navigator went LOST and the dashboard re-locked the throttle **on a moving
locomotive**; the last accepted command was too low for the terrain and Otto
stalled with the operator locked out. Plus the QUORUM vocabulary rebind.

**v1.10.2 — the dashboard must never say no to a manual operator.** Toby's tab
demanded re-declaration after an ordinary manual stop, and every confirmation
string said "OTTO" regardless of tab. Declaration now mirrors the locomotive's
reported nav state and nothing else; MANUAL controls are never locked;
confirmations inform but never gate. A **refusal inventory** (in its
implementation report) audited all eleven blocks reachable in MANUAL: eight
removed, two survive — E-STOP latch semantics, and SET INTERVAL while the loco
*reports* motion, which refuses a declare and never refuses driving.

Also added: a per-locomotive **polarity agreement tile** (session AGREE /
DISAGREE, percentage, last ten verdicts as mm ticks). It diagnosed a noisy
cable at 27% and a flipped sensor at 100% in one week and has earned permanent
instrumentation.

---

## 6. Field evidence — `field-records/`

Committed, not left on the Pi. Logs are renamed from content (first timestamp,
and the locomotive whose topics they carry), cal recordings kept as captured,
and verdicts sit beside the logs they analyse. `.gitignore` un-ignores
`field-records/logs/` only; the repo-wide `*.log` ignore stands elsewhere.

**The certification verdict worth knowing** — Toby, across the overnight
2026-08-02/03 boundary, answering the five metrics Sam asked for:

| | before | after |
|---|---|---|
| markers detected | 380 | 3,631 |
| polarity agreement | **72.6%** | **90.6%** |
| floor rejects per 100 markers | 11.1 | 3.3 |
| median Hall peak | 103 | 187 |
| weak reads (<80) | **21.6%** | **1.4%** |

The step change is not gradual — every run before the gap is weak, every run
after is strong. **The improvement is in the signal, which is where a
shielding fix should show up** — not in the navigator's tolerance of bad
signal. The payoff was bought in hardware and the firmware's job got easier as
a result, rather than the firmware hiding the problem.

Two caveats on the record, both in the verdict: phantom counts are **not
measurable** in that data (the conservation gate is a QUORUM feature and
postdates the run — floor rejects are the stated proxy), and **the cable
change itself is not logged**. The overnight boundary is inference from the
discontinuity. David should confirm the install window before this is cited as
the cable's payoff.

---

## 7. The IR wheel sensor — diagnosis and correction

**It was never mainly a sunlight problem.** The three largest v1 "lockouts"
aligned exactly with MQTT dropouts; `pulse_count` advanced **+10, +9, +10**
across them, so the sensor kept seeing wedges throughout; and the ADC never
exceeded **2511 of 4095**, nowhere near saturation. A rising pulse count is
not something an optical failure can produce.

The mechanism was architectural. `connectWiFi()` blocks up to 30 s in a `while`
loop called from `loop()`, and sampling was polled in that same loop — so
wedges passed an unwatched sensor, and the pulse open when the stall began was
closed on resume, publishing a **21-second "pulse width"** that measured CPU
busy time, not light.

`Spoke_IR_RSSI_survey_v2` rebuilds it on the pattern this project has already
proven twice: a 1 kHz FreeRTOS task at priority 2 pinned to core 0, feeding a
256-slot timestamped event queue that `loop()` drains, with the network on its
own task. **Three requirements in `IR_SENSOR_NOTES.md` were themselves wrong
and are corrected in place there**, each from reading the installed toolchain
rather than from assumption:

1. **A task, not an ISR.** `adc1_get_raw()` is the *deprecated* legacy driver
   on core 3.3.11, and its replacement `adc_oneshot_read()` takes a mutex and
   is not ISR-safe either — so the ISR advice had no safe ADC call behind it.
   `hallTask` measured 1–4 ms gaps while `loop()` stalled 20 seconds.
2. **`setSocketTimeout(2)` is not the connect bound** — it is PubSubClient's
   read timeout. `espClient.setConnectionTimeout(3000)` is the connect bound.
3. **The reconnect flush is a hazard.** A 30 s outage buffers ~105 events;
   flushing them individually stampedes the link that just failed. Capped
   peek-publish-remove, as the locomotive's marker queue does.

`IR_TELEMETRY_WIFI` splits the build: undefined, no radio is compiled in at
all (21% flash vs 69%) and pulses log over USB, so capture can be validated
with nothing able to stall it; defined, it does the RSSI survey. **Run the USB
build first** — validating capture and network in one run leaves a bad result
unattributable.

This makes the sensor **honest, not sun-proof**. A real optical failure will
now show a *flat pulse count with healthy span and sane widths*, the opposite
signature from the v1 artefact. That reading is what would justify the
differential-sampling work, which remains gated on whether the emitter is
broken out to a GPIO.

---

## 8. Open decisions — for David

1. **The pKPH scale disagreement.** Settled for firmware: the navigation
   lineage uses `PKPH_MM_PER_SEC = 5.37325` (0.18611 per mm/s, ≈1:51.7 — a
   house unit, not a geometric scale), and the IR sketch now matches it. **The
   Pi dashboard still uses 0.162 (1:45)** and agrees with neither. Changing it
   alters every KpH figure the console has displayed, so it needs a decision,
   not a silent fix.
2. **Confirm the shielded-cable install window** (§6) before the certification
   numbers are cited as the cable's payoff.
3. **Is the QRE1113 emitter hard-tied to VCC on the breakout?** This gates the
   entire differential-sampling approach — the actual optical fix.
4. **HL-Auto / `SPEC.md` is stale.** It is written against `r12`, which is now
   archived, and its edit points cite r12 line numbers. It is not current work
   and should be either rebased onto QUORUM or explicitly parked. Right now it
   is neither, which is the state that causes confusion later.
5. **`SPEC.md` at the repository root is a one-line stub** and untracked,
   distinct from `docs/SPEC.md` (243 lines). Delete it or fold it in.

---

## 9. What's next — priority order

1. **Deploy `ngr_runlog.py` to the Pi.** It has *never run there* — no file,
   no service, no process. The certification runs had to be reconstructed
   from ad-hoc `mosquitto_sub` redirects with no per-run metadata, and the IR
   survey car was never recorded at all because the subscription was
   `ngr/loco/+/#` (fixed 2026-08-04 to `ngr/#`; see the file header). This is
   first because everything below produces evidence, and there is currently
   nothing systematically capturing it.
   ```bash
   scp server/ngr_runlog.py david@192.168.68.142:/home/david/NGR/telemetry/
   ```
   Then on the Pi: create `/home/david/NGR/telemetry/`, add a systemd unit
   mirroring `ngr-app` (`WorkingDirectory=/home/david/NGR/telemetry`,
   `ExecStart=/usr/bin/python3 ngr_runlog.py`, `Restart=always`,
   `After=mosquitto.service`), then `systemctl enable --now ngr-runlog`.
   Runs land in `telemetry/runs/`, the rolling record in `telemetry/all_*.log`.
   Kill the stray manual `mosquitto_sub` loggers once it is up — two of them
   captured identical topic sets, which is why concatenated logs showed every
   message twice.
2. **M1 field campaign.** Run QUORUM 1.4 against the §8 checklist in the
   implementation spec — the replay and field items listed in
   `QUORUM_1_0_IMPLEMENTATION_REPORT.md`, now including the bicameral checks
   (NO_QUORUM in MANUAL must leave the motor untouched).
3. **Deploy dashboard v1.10.2** to the Pi and click through its verify list.
4. **Flash `Spoke_IR_RSSI_survey_v2` USB-only** for one loop to prove capture,
   then the WiFi build for the RSSI survey.
5. **Tag what flies.** QUORUM 1.4 gets its tag from the field, not the desk.
6. Resolve the open decisions in §8.

---

## 10. Standing practice

**Every implementation report, review finding and field verdict is committed
to `docs/` as part of finishing the job** — unprompted, as its own `docs:`
commit. Agents do not see each other's work unless it lands in a file, and
neither does David in three weeks. Deviations from a spec are recorded **as
deviations**, never absorbed into a "zero deviations" claim; CODEX contested
exactly that wording once, correctly.

Current documents: five QUORUM reports, the CODEX findings file (two review
rounds, verbatim), the dashboard report with its refusal inventory, and the
Toby certification verdict.

---

## 11. Environment / tooling

- Editor: VS Code, folder `~/esp-loco-control`. **On GitHub, private**
  (`ninobur/esp-loco-control`).
- ESP32 Arduino core **3.3.11**. Build:
  `arduino-cli compile --fqbn esp32:esp32:esp32 <sketch-folder>`.
- Pi: `david@192.168.68.142`. Console runs as systemd `ngr-app` from
  `/home/david/ngr_app.py`. Broker is mosquitto on the same host.
- Credentials live in per-sketch `credentials.h`, git-ignored at any depth;
  `firmware/config/credentials_template.h` is the pattern to copy. **The
  repository has never carried a plaintext password** — one arrived inline in
  a contributed sketch and was split out before that sketch was committed.
- The broker holds **retained ghosts** from old firmware. Any consumer must
  treat retained state as stale when `online` is 0.

---

## 12. Collaboration

David decides; Claude and Sam (ChatGPT / Codex) trade off design and QA; CODEX
reviews implementations against source and has found defects that text review
could not — the four terminal-evidence findings on QUORUM 1.0 and the ABA race
on 1.1 all live in the seams between the spec and the code. Roles flex; see
`CLAUDE.md`.
