# NAVI_IR 0.1

Experimental locomotive-control sketch for **Toby (9950012)**. Built and host
tested; flashed by the operator on 2026-09-20. Independent review and field
acceptance remain pending. The [first startup](../../../docs/NAVI_IR_FIRST_STARTUP_20260920.md)
found IR audible but unpaired; that attempt did not validate joint navigation.
Manual operation uses the existing console. AUTO remains disabled by default.

Open `NAVI_IR.ino` in Arduino IDE from this directory. Select ESP32 Dev Module,
use the locally installed ESP32 3.3.11 core, and use **115200 upload speed** on
David's Mac. Serial Monitor is also 115200. No new TX or Pi RX flash is required:
this receiver consumes the test car's existing TX 1.5 type-5 movement packets.

The human version is NAVI_IR 0.1; machine-readable `state/bootid.sketch` is
`NAVI_IR_0_1`. This is a new experiment, not a production QUORUM release.

## Authority

- Hall observations plus the declared route frame identify mapped landmarks.
- IR counts measure nominal travel of the unpowered wheel between observations.
- IR uncertainty prevents commitment and can expose competing interpretations.
  IR alone never names a magnet or advances route position.
- PWM remains a motor command, not distance or proof of physical motion.
- Manual control, E-stop and battery protection retain priority. No CTO/peer
  coordination is provided: this is a **single-locomotive experiment**.

## Joint decision process

1. Median-of-five ADC samples feed the X22 excursion detector. Both the Hall
   opening and the strongest signed excursion in its 400 ms window are retained.
   Navigation waits for the completed window, but aligns IR to the opening.
2. A paired IR source supplies cumulative completed pulses. CRC, source MAC,
   boot, sequence, counter continuity and optical diagnostics are checked.
   Dropped radio packets do not by themselves lose cumulative pulse distance.
3. Each retained route interpretation owns its own last Hall/IR anchor. Nominal
   travel is compared to zero, one, and two mapped intervals from that anchor.
   Short travel can question even a polarity-matching Hall observation; travel
   spanning two intervals can expose a missed same-polarity magnet.
4. Branches represent ordinary progression, one wrong Hall polarity, one false
   observation, or one missed magnet. This is explicitly a **one-fault model**,
   not a proof against arbitrary errors. At most 24 hypotheses are retained;
   overflow is LOST, never silent pruning to a preferred location.
5. A route position commits only when retained branches agree on position and
   have nominal distance consistency. Otherwise the last committed MM is held.
   The first Hall observation establishes an IR anchor and is unresolved; the
   operator's declaration is not assumed to be exactly on a magnet.
6. Later observations may restore a single consistent interpretation. Ten clean
   subsequent joint observations renew the fault allowance after resolution.
   Ten additional unresolved observations exhaust the recovery window. This is
   a bounded recovery opportunity, not the old one-strike shutdown.

**Distance consistency is not a certified distance bound.** The current
association chooses the nearest of the three mapped distances; an exact tie is
unresolved. No percentage tolerance has been fabricated from two good laps.
Nominal IR may propose alternatives but cannot hard-exclude an otherwise viable
branch. Hall history resolves identities within the stated fault model.

## Receiver and pairing

The locomotive must hear the test car directly on Wi-Fi/ESP-NOW **channel 11**.
The currently deployed REP2 does not repeat type-5 IR packets. The Pi RX remains
useful as an independent recorder but is not on the locomotive's decision path.

No source MAC is guessed or automatically trusted. After a supervised flash:

1. Keep wheels clear and establish ordinary console control and E-stop first.
2. Turn on the IR car. Inspect `ngr/loco/9950012/diag/ir_link` for `radio_ready:1`,
   `channel_ok:1`, and its CRC-checked `seen_mac`. Confirm that MAC belongs to
   the intended IR car; merely being visible is not pairing.
3. With Toby stopped and not enrolled in AUTO, publish the confirmed MAC to
   `ngr/loco/9950012/cmd/ir_pair`, for example from the Pi:

   ```sh
   mosquitto_pub -h 192.168.68.142 -t ngr/loco/9950012/cmd/ir_pair -m AA:BB:CC:DD:EE:FF
   ```

   Replace that example with the verified unicast MAC. Pairing persists in NVS;
   pairing does not declare or alter route position.
4. Verify `paired:1`, `fresh:1`, increasing `accepted`, and cumulative pulses
   changing with the wheel. Verify no change while genuinely stationary.
5. Declare direction and starting interval using the existing console. Use
   **Manual throttle**, not AUTO/GO, for the first paired run.

Receive callback work is bounded to a queue copy. Validation and history run in
the main loop. The 64-snapshot history uses a minimum receive/capture offset per
sender boot, a 150 ms nearest-snapshot alignment gate, and 1 s receipt freshness.
These are engineering gates, **not proven clock/delivery error bounds**.
Duplicate packets do not refresh freshness. Old sender boots are rejected;
eight retired boots are retained, then the source fails closed until re-paired.
Reversal, declaration, or receive-queue overflow breaks the distance frame.
Reversal requires a new operator declaration because IR counts are unsigned.

## Transparent telemetry

All topics below are under `ngr/loco/9950012/`. Existing console command names
and payloads are preserved; these are additions, not a Pi/controller rewrite.

| Topic | Evidence |
|---|---|
| `state/bootid` | Version, Hall configuration, AUTO gate, authority and limitations |
| `diag/ir_link`, `telem/ir` | Pairing, seen MAC, channel, receipt age, counts, boot, optical reason, rejects/drops |
| `nav/ir_compare` | Per-branch zero/one/two-step mapped distance, nominal travel, residual, quality, Hall matches, old 500 ms timing comparison |
| `nav/discrepancy` | Hall opening/window votes, baseline age, source sequence, alignment, ruling and recovery state |
| `nav/hypotheses` | Every retained branch, MM, anchor, cause mask, fault allowance and distance consistency |
| `state/nav`, `mm/marker` | Committed position and explicit ruling; ambiguity does not masquerade as an advance |
| `diag/hall_decision` | Baseline collection acceptance/refusal; no route authority |
| `state/loopstat` | Queue/publish losses and stale-frame rejections |
| `alert` | Console-compatible summary; `moving:1` only for usable positive IR travel, otherwise `null`, never PWM-derived physical motion |

Cause bits are polarity=1, false observation=2, missed marker=4, vote
disagreement=8. `distance_confirmed` means nominal association consistency only;
`ir_bounds:UNVALIDATED` and `CONDITIONAL_SEQUENCE` qualify that claim. The optional
physical maximum-speed check is unconfigured (zero); the old 500 ms guard is
logged but does not reject observations. No pulse counts are inferred/repaired.

## Control and unresolved risks

The gated AUTO path evaluates station actions from the same actual station
state for every candidate position. It commits an action only if all outcomes
and their internal state agree. Disagreement, a missed approach, or station
timeout withdraws AUTO with a controlled stop, without restarting automatically.
Ambiguity does not cancel a station ramp or restore cruise. Route-section cruise
is restored only at a resolved position with the station machine idle.

Do not enable `NGR_ENABLE_EXPERIMENTAL_AUTO` for railway use yet. These are
remaining acceptance gates, not known-safe defaults:

- The new 400 ms deferred Hall judgement changes station reaction timing.
  Station approaches, landings, dwell/departure and recovery need hardware tests.
- X22's baseline can still mistake a flat magnetic fringe for background. Its
  PWM/time-based lost-lock re-prime is disabled; baseline diagnostics stay live.
  Electrical rearm and the 400 ms capture window still limit acquisition.
- The inherited zero-command/zero-output operating hold suppresses Hall
  declarations. It is not proof that wheels cannot coast or be hand-moved.
- TX 1.5 low-speed/departure behavior is not validated. A zero count in invalid
  optical state is not proof of zero movement. Clean cruising evidence does
  not validate crawl, stop or restart.
- Ten-observation recovery is not a wall-clock or travel-bounded safety fence
  if Hall stops producing observations. A reviewed no-observation policy and
  live IR-unavailability behavior are required before AUTO acceptance.
- No survey-grade uncertainty budget covers wheel error, packet-clock alignment,
  Hall opening position, mechanical offsets, reversals or coasting. This build
  cannot establish whether IR outperforms timing at false-Hall exclusion yet.
- Telemetry is bounded and counted, not lossless. Many branches can generate
  bursts larger than the publish queue. No raw waveform archival is added here.
- INA219 absence is loudly reported, but then battery protection is unavailable
  as in the inherited manual-control base. Do not mistake a warning for protection.

Dependencies are the installed ESP32 core, PubSubClient, Adafruit INA219, local
QUORUM credentials, the Toby NAVI_SIMPLIFIED profile, X22 detector header, and
the common movement wire/contract. Keep this sketch inside the repo tree; copying
only its `.ino` elsewhere omits required headers.

## Verification

From the repo root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I firmware/programs/NAVI_IR firmware/programs/NAVI_IR/tests/test_navi_ir.cpp -o /private/tmp/test_navi_ir
/private/tmp/test_navi_ir
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I firmware/programs/NAVI_IR firmware/programs/NAVI_IR/tests/replay_core.cpp -o /private/tmp/replay_navi_ir
python3 firmware/programs/NAVI_IR/tests/replay_noon.py /private/tmp/replay_navi_ir
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/programs/NAVI_IR
```

The 35 host checks cover transport validation, ties, two-way laps/wrap, route-word
uniqueness, stale data, false/missed Hall observations, renewed recovery,
reversal, exhaustion and station action consensus. Historical noon replay:
513 observations, 438 tracking, 75 ambiguous, 30 recoveries, zero LOST, zero
committed differences from the old sketch's MM. Peak branch count was one:
**that clean run did not exercise competing fault branches**. It uses old Hall
close timestamps and real logged polarity, not new opening/window acquisition;
the old MM is a comparison label, not independent ground truth.

See the [implementation and design record](../../../docs/NAVI_IR_0_1_IMPLEMENTATION_REPORT.md)
for operator principles, design choices and alternatives, evidence provenance,
build results and the intended hard-wired follow-on. The architectural summary
is [decision 0089](../../../docs/decisions/0089-navi-ir-joint-evidence.md).
