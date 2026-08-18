# 2026-08-18 — IR Test A: bench pairing and the first live field run

**Session:** bench pairing at first light, then a live field run — Toby solo
AUTO, IR Test Car coupled behind, clockwise, MM040-041 start. Observational
only throughout: `authority:"OBSERVE_ONLY"` on every `telem/speed` frame,
zero navigation/motor/CTO/E-stop authority. Firmware: Toby
`QUORUM_1_16R_IR_TEST_A` (built and adversarially reviewed prior session,
commits `d71320d` + `46fe686`); sender `IR_TEST_CAR_ESPNOW_1_0`.

## Verdict: clean first run — every mechanism Gate 2 exists to test behaved as designed

- **Bench pairing completed and verified at boot.** Toby: `sensor MAC
  EC:E3:34:78:A2:60 (usable)`. Sender: `local MAC EC:E3:34:78:A2:60  target
  38:18:2B:30:8C:2C`. Both devices reflashed after cross-filling the MAC
  placeholders; no all-zero/UNSET state remained.
- **Transport.** An initial high failure rate (`tx=92 txe=0 txf=87`) was
  diagnosed live as Toby being unpowered during the cable swap, not a radio
  fault — `txe` (local driver error) stayed 0 throughout; only `txf` (peer
  didn't ACK) climbed. Confirmed once Toby was powered: 100/100 delivered,
  0 new failures.
- **Pulse counting** confirmed correct across two independent bench
  hand-spins (277, then 403 cumulative pulses).
- **First live IR-vs-marker speed agreement:** `ir_mmps 214.48` vs
  `mm_mmps 232.3`, 7.7% apart — inside the existing 10% tolerance (spec
  §9.2, still provisional pending more samples).
- **Epoch/reboot discipline (§5.3) proven under a real physical
  disturbance.** The IR car uncoupled mid-run and was restored. The
  receiver logged three sensor reboots (`boot:3`) with `regr:0, rebase:0`
  — no fabricated cross-epoch pulse delta, no false regression flag. This
  is the first field exercise of that mechanism against a real disturbance
  rather than a bench simulation.
- **Station dwell (Arches).** `STOPPED`/`0.00` held steady for the entire
  fixed dwell while `mm_mmps` correctly showed its last pre-stop value,
  stale rather than reset to null — both exactly per spec (§6, and the
  "bind speed fields to this event" fix from the prior review round).
- **Departure.** IR caught the first instant of real motion (`VALID` at
  `apwm=35`, 49.2 mm/s) and tracked a clean acceleration curve — 49.2 →
  54.8 → 89.4 → 97.5 mm/s — as commanded PWM ramped, while marker speed
  had nothing to report until the next magnet. This is precisely the
  continuous-coverage gap Test A exists to fill.
- **Delivered mid-run, at the operator's explicit request:** a live IR
  speed readout on the Pi console (`irRow()` in `ngr_app_v1_11_2.py`
  v1.11.3), same hidden-until-heard pattern as the existing CTO row.
  Deployed to the Pi and confirmed rendering real data during the run —
  before this, the operator had no way to see any of the numbers being
  discussed except through chat narration.

No rejects of any kind were observed on the wire-validation counters
during the run (`blen/bver/bwire/bsrc/bsen/btgt/benum/benc` all held at the
values established on the bench). No effect on navigation, AUTO, CTO, or
E-stop was observed or expected.

## What Gate 2 has not yet covered

- ESP-NOW inter-arrival/jitter distribution not yet measured — still
  needed to replace the provisional `IR_LINK_STALE_MS` (500 ms) and
  projection window (150 ms) with numbers from the real channel.
- No deliberate pass at a known phantom site (MM008, the migrated-magnet
  site from the 2026-08-16 session) this run.
- Live `VALID` was never caught in real time on the bench, only
  after-the-fact in `STOPPED` — not a defect; pulse counting is already
  proven and a moving locomotive produces far more data than a hand-spin.
- The uncoupling incident's exact moment could not be pinned to a specific
  `STOPPED`→`VALID` transition with certainty — both an ordinary station
  stop and the uncoupling produce numerically similar cycles. Attributed
  instead to the `boot:3` counter, which is solid evidence of three real
  reboots but not a frame-by-frame reconstruction of which one was the
  uncoupling.
- Grades, curves, shade, and direction reversal — all named in the spec's
  Gate 2 observation list — were not specifically exercised or reported on
  this run.

## Uncommitted at session end

Three files carry this session's work and were not committed:

- `firmware/QUORUM/LL_LocoConfig_9950012.h` — `IR_SENSOR_MAC_BYTES`
  bench-paired to the IR Test Car.
- `firmware/test-programs/IR_ESPNOW_SENDER/ir_espnow_config.h` —
  `TOBY_STA_MAC` bench-paired to Toby.
- `server/ngr_app_v1_11_2.py` v1.11.3 — the console IR row. Already
  deployed to the Pi (`ngr-app` restarted, confirmed active) independent
  of this repo's commit state, per the project's established deploy
  pattern.
