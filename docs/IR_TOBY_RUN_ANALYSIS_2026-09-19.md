# IR/Toby run analysis - 2026-09-19

See the [session record](IR_SESSION_RECORD_2026-09-19.md) for activities,
verification, limitations, and the outstanding diagnostic caller issue.
TX 1.4 is compiled; flashing and field acceptance remain pending.

## Trust boundary

Toby's accepted Hall advances are used only through MM 10 at 17:43:10.442.
The MM 11 polarity disagreement at 17:43:12.572 put NAVI-ONE into `STRUCK`.
The strike and all later landmark identities are excluded. Hall timestamps are
mapped from Toby's monotonic `close_ms`; delayed MQTT receipt time is not used
as the event clock. `ir_hall_expost_align.py` now enforces the first NAVI
`STRUCK` event in the selected time range as its default trust cutoff. Data
after a strike can be included only with the explicit `--include-after-strike`
diagnostic override.

## Result

- Stationary shade, stationary direct sun, and the shade dwell produced zero
  false pulses. No ADC saturation, sample misses, queue drops, or send errors
  were found in the corresponding sensor counters.
- Healthy repeated track sections were stable: MM 120-127 measured 199 pulses
  on both traversals; MM 111-117 measured 185 and 186.
- MM 127-143 measured only 316 and 393 pulses. Several individual intervals
  contained 0, 1, or 4 pulses despite strong optical contrast.
- The failed intervals alternated between being almost wholly below the stale
  low threshold and wholly above the stale high threshold. This identifies
  envelope tracking lag at rapid shade/sun or angle transitions, not ADC
  saturation and not loss of optical modulation.

## Offline correction

Raw received samples were replayed through five envelope configurations:

| Window / refresh / prime | Stationary false pulses | Coupled-run transitions | Bad pass A | Bad pass B |
| --- | ---: | ---: | ---: | ---: |
| 2048 / 250 / 512 (old) | 0 | 4475 | 227 | 293 |
| 1024 / 100 / 256 | 0 | 4653 | 303 | 330 |
| 512 / 50 / 128 (selected) | 0 | 4759 | 329 | 349 |
| 256 / 25 / 128 | 0 | 4357 | 331 | 349 |
| 128 / 20 / 64 | 0 | 4309 | 320 | 332 |

The 512 ms candidate preserved the old detector's counts exactly on the first
healthy MM 120-127 pass (144 received transitions) and differed by one on the
second (160 versus 161). The shorter candidates lost healthy transitions.
Only the envelope window, refresh, and prime are changed in TX 1.4; the 15 ms
debounce, Schmitt fractions, minimum contrast, and 2.5 second latch remain
unchanged.

Raw ESP-NOW packet loss means replay totals are lower than the detector's live
cumulative pulse count. All candidates use the same received samples, with
re-arming across packet gaps. This approximate replay supports candidate
selection, not an absolute error rate or proof of firmware accuracy.

## Next acceptance run

1. Flash `IR_SCOPE_ESPNOW_ACTIVE_TX_1_4` onto the test-car ESP32 and verify the
   startup banner reports `env=512 update=50 prime=128`.
2. Repeat two minutes stationary in shade and two minutes stationary in direct
   sunlight. Both must remain at zero pulse growth.
3. Hand-roll slow, medium, and fast in shade, then repeat in direct sunlight.
   Include crawl and stationary changes in lighting. Record independent turns
   or measured travel where possible.
4. Only after those checks are clean, tow at one steady speed for at least two
   complete circuits, deliberately
   crossing the MM 127-143 lighting region on both laps.
5. Use accepted Hall events only while NAVI remains normal. Compare repeated
   IR counts by physical marker interval; do not let IR assign a marker.
6. Reject the firmware if a stationary false pulse appears, healthy repeated
   sections regress, or any lighting-boundary interval remains near zero.
