# NAVI_WAVEFORM_0_1 — first field runs, 2026-10-27 afternoon

**Status:** field record. It reports observations and their interpretation only. It changes no design, firmware or map, and recommends nothing that David has not decided.

## Build

| Item | Value |
|---|---|
| Sketch | `NAVI_WAVEFORM_0_1` (branch `impl/navi-waveform-recognizer-manual-20261027`, HEAD `5aca12b` at the time of review) |
| Identity at boot | `state/bootid`: `sketch` still reads `NAVI_POWER_OF_NORMAL_MANUAL_ONLY` because `SKETCH_NAME` was not renamed (David confirmed). `recognizer` reads `NAVI_WAVEFORM_MATCHED_FILTER_SINGLE240` |
| Recognizer | Single 240 ms template, pad 0.5, level-only normalization, r ≥ 0.7, \|A\| ≥ 70, ±120 ms NMS (design §2–3) |
| Matcher | Simple next-target rule (the October 21 hold rule), plus a provisional 400 ms duplicate-advance guard. No map-aware matcher in this build |
| Mode | Manual only, AUTO locked |

## Runs (operator's account)

1. **Otto, CCW, start 004-005, end 112-113.** The dashboard MM did not correspond to position.
2. **Two aborted restarts (Otto).**
3. **Otto, CW, start 040-041, end 049-050.** David wrote "039-050"; I read it as 049-050.
4. **Otto, CCW, start 044-045, end 036-037.**
5. **Toby, CCW, start 035-036, end 020-021.**
6. **Toby, CW, start 024-025, end 041-042.**

## Evidence

MQTT logs in `~/Pi_Log_Downloads` (SHA-256, first 16 hex digits):

- `9950011_20261027_155920.log` (`2bbfe73fb919b396`)
- `9950011_20261027_161304.log` (`5c966f5e5b065a68`)
- `9950012_20261027_162642.log` (`55f0f04982ff078a`)
- `9950012_20261027_163657.log` (`301cb5cd2441154b`)

NSR captures examined:

- `navi_session_9950011_CA662AD9DC993994_D2A5D501_20261027_160421.nsr` (`f19900637dd7f667`): 413.7 s, 0 CRC-bad records, 0 serial gaps.
- `navi_session_9950011_563C8E21CDA1CABF_B0B1C945_20261027_162336.nsr` (`6bb25331eb384eaf`): 279.5 s, 0 CRC-bad records, 0 serial gaps.

Scripts (in `research/navi_waveform_design_20261027/field_20261027/`):

- `session_summary.py`: per-boot summary of the logs.
- `replay_vs_firmware.py`: compares on-loco events with the Python replay and prints the run tail.

The Toby captures and the Otto CW capture were not examined.

## Observations

### Per run, from `diag/navi_encounter` [M]

| Run | Boot | Encounters | Matcher result | NAVI's last confirmed | Operator's end |
|---|---|---|---|---|---|
| 1. Otto CCW 004-005 | `CA662AD9…` | 232 | 123 CONFIRMED, 109 NONQUALIFYING | MM053 | 112-113 |
| Restart a | `CC9DF08D…` | 90 | 90 OBSERVATION. The declaration was refused because no session direction was set, so no claim was made | — | — |
| Restart b | `3D46D330…` | 35 | E-stop, then a direction change suspended navigation | — | — |
| 3. Otto CW 040-041 | `7247956A…` | 180 | 180 CONFIRMED, no break | MM049 | 049-050 |
| 4. Otto CCW 044-045 | `563C8E21…` | 178 | 178 CONFIRMED, no break | MM038 | 036-037 |
| 5. Toby CCW 035-036 | `EDFAFAEA…` | 186 | 186 CONFIRMED, no break | MM021 | 020-021 |
| 6. Toby CW 024-025 | `51104A27…` | 188 | 188 CONFIRMED, no break | MM041 | 041-042 |

- In the driven runs, every recognized encounter had |r| ≥ 0.75 and |A| ≥ 104.
- No duplicate-guard refusals occurred.

### Firmware against offline replay [M, R]

The Python reference recognizer, configured as `single240_p0.5`, was run on the two Otto CCW captures:

- It produced exactly the firmware's event list: 232 / 232 and 178 / 178.
- No event appeared in only one of the two lists.
- Passage times agreed within 1.5 ms (median 0.8 ms).

This is design check R1 (replay parity), met in the field on these two captures.

### The magnets NAVI did not recognize [R, from the native samples]

| Where | Recording | 240 ms template | Wider template, same pad, for diagnosis only |
|---|---|---|---|
| Run 4 end, MM037 (S) | 273.06–273.85 s; PWM ≈ 28 while braking; 786 ms above the 70-count level (MM038 at PWM 44 was 322 ms); peak 1712 against a background of 1900 | r −0.69, A −30: rejected | 1.5 s: r −1.00, A −193 · 0.6 s: r −0.80, A −142 |
| Run 1 end, MM113 (N) | 406.55–407.25 s; PWM ≈ 34 while braking; 696 ms; peak 2242 against a background of 2082 | r 0.70, A 29: rejected on amplitude | 1.5 s: r 1.00, A 162 |
| Run 1, MM169 (N), before a stall | 70.14–70.54 s; PWM 90 commanded; 400 ms; peak 2208 against a background of 2052 | r 0.66: rejected | 0.6 s: r 0.95, A 166 · 1.5 s: r 0.88, A 114 |

- **After each end-of-run miss, the Hall level returned to background:** 1,901–1,904 counts after run 4 and 2,079–2,081 after run 1. The sensor was not resting in a magnetic field when the loco stopped.
- **The stall in run 1.** After MM169 the Hall signal was flat for 20 s, with a standard deviation of about 2 counts at PWM 90 commanded (71.5–89.5 s). Neither the recognizer nor the diagnostic 70×2 detector reported anything in that time. It is consistent with Otto standing still. The cause of the stall was not determined.

### Run 1, reinterpreted [R]

- The 232 recognized polarities were fitted to RouteMap.h as continuous CCW travel from MM004, with MM169 taken as missed. On that fit, **every polarity agrees with the map.**
- The hold rule instead compared the first encounter after the stall (really MM168) with its expected MM169. Both are N, so it confirmed "MM169" and stayed one magnet behind from then on.
- The 109 NONQUALIFYING rows, and 115 of the 123 CONFIRMED rows, belong to that displaced sequence.
- Run 1 ended physically at about MM113–112, as the operator reported. NAVI reported MM053.

## Interpretation

1. **The recognizer recognizes every magnet passed at cruise.** That holds in both directions on both locos, with no false events on these runs. On-loco recognition is identical to the replay.
2. **All three recognizer misses were slow or decelerating passages**, with waveforms 400–786 ms wide. The single 240 ms template scores them below r 0.7, or below |A| 70. A wider template recognizes each of them.
   - The design document did not establish this. It recorded slow passage as the least-covered condition (§8 T4), and the build README listed "PWM-60 and lower speeds" as not done.
   - The operator's alternative explanation, that the sensor was still in the magnetic field, is not supported. The level had returned to background at both stops.
3. **One miss was enough to displace the hold rule** for the rest of the run, and nothing in the build recovers from it. Replay of the Oct 25–26 corpus showed the same behaviour (design §4.4).
4. **The magnet map agreed with the observations** on these runs, including MM066/067/069.

## Open, for David

- **Slow passages.** Whether to add a second, wider template width (0.6 s or 1.5 s were tried here, diagnostically only) is a design decision. It would change the recognizer's cost and its false-event behaviour, neither of which has been evaluated for this railway. No change has been made.
- **Matcher recovery after a miss.** The October 21 hold rule remains the established rule. The experimental map-aware matcher (design §4.5) has not been in any build.
- **Otto's stall** at about 16:05:39 near MM169–168, at PWM 90: **explained by David, 2026-10-27.** A piece of plastic blew across the track and held Otto up. David freed him by hand. The miss of MM169 was therefore a passage slowed by an obstruction, the same slow-passage case as the two end-of-run misses. It was not a recognizer or track-magnet fault.
- **Build identity.** `SKETCH_NAME` should name the build. That is a code change in the implementation branch, for whoever owns it.
